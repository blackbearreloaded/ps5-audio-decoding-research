# Compressed audio decoding

There are three practical native entry points:

1. `sceAudiodec*` provides MP3 and AAC packet/frame-to-PCM decoding.
2. `libSceOpusDec` and `libSceOpusCeltDec` provide direct Opus packet-to-PCM paths.
3. AvPlayer owns container demuxing when the application does not already own
   compressed frames.

These are application-owned pipelines: the decoder returns PCM; the application still has to resample,
mix, and submit the PCM to an output port.

The native decoder calls reach the platform's hardware/firmware audio-offload
services. The evidence does not identify a particular physical DSP block.
Opus uses a separate, smaller packet API that reaches AJMI/AJM directly and
does not use the generic `sceAudiodec*` control structures.

## Module and lifetime sequence

For AAC or MP3, use this order. The production PSRadio implementation uses
`AudioDec = 0x0088`, `AUDIODEC_AAC = 3`, and MP3 codec type `2`:

```text
SystemModule.Load(AudioDec / 0x0088)
    -> sceAudiodecInitLibrary(codec)
    -> sceAudiodecCreateDecoder(...)
    -> repeat sceAudiodecDecode(...)
    -> sceAudiodecDeleteDecoder(...)
    -> sceAudiodecTermLibrary(codec)
    -> unload AudioDec
```

Keep the system module loaded until every decoder is destroyed. The native
C++20 probes use RAII objects so partial initialization and every return path
release only the resources they own.

## Native MP3 example

The [complete C++20 MP3 probe](../examples/native-audio-poc/mp3/src/main.cpp)
parses bounded MPEG Layer III frames, submits each exact frame through
`libSceAudiodec` codec type `2`, validates consumed and produced byte counts,
and sends mono/stereo signed-16 PCM through the shared RAII `AudioOut` sink.
The standalone probe accepts 48 kHz input because its output port is fixed at
48 kHz; production code must resample other rates.

An MP3 frame can produce up to 1152 samples per channel, so it rarely matches a 256-frame stereo
AudioOut block. Use a small PCM accumulator rather than calling `Output` with each decoder result.

Read sample rate and channel count from the validated MP3 header before
selecting or adapting the output port.

The native MP3 route is runtime-proven in the production PSRadio build:
codec type `2` reached AJM, decoded a live 48 kHz stereo stream to 4,608
signed-16 PCM bytes, and reached AudioOut. This closes the earlier gap between
the static `MP3 Decoder`/`sceAjmDecMp3ParseFrame` evidence and application use.

## Native AAC example

Network AAC in PSRadio is handled as ADTS. The
[complete C++20 AAC probe](../examples/native-audio-poc/aac/src/main.cpp)
finds bounded ADTS frames, uses self-describing configuration `1`, submits
each exact frame through codec type `3`, reads the reported sample rate and
channel count, and sends signed-16 PCM to `AudioOut`.

The wrapper reports the AAC stream's sample rate and channel count after a successful decode. AAC can
produce up to 2048 samples per channel per frame and supports up to six channels in the low-level
binding, although the basic POC intentionally accepts only mono or stereo.

## AAC configuration

The native AAC parameter structure uses these fields:

| Field | Self-describing/ADTS | Raw AAC blocks |
|---|---|---|
| `WordSize` | `Signed16` or `Float` | `Signed16` or `Float` |
| `ConfigNumber` | `1` | `2` |
| `SamplingFrequencyIndex` | Historical value carried by the wrapper; header supplies the actual rate | Index into `AacSampleRates` |
| `MaxChannels` | 1 through 6 | 1 through 6 |
| `EnableHeAac` | `1` for HE-AAC | `1` for HE-AAC |

The raw-block rates are:

```text
96000, 88200, 64000, 48000, 44100, 32000,
24000, 22050, 16000, 12000, 11025, 8000
```

For an ordinary file or ADTS radio stream, do not select raw-block mode. For a raw stream:

```cpp
AacParam raw_blocks{
    sizeof(AacParam),
    1, // signed-16 output
    2, // raw AAC blocks
    4, // 44.1 kHz index
    2, // maximum channels
    0  // HE-AAC disabled
};
```

## HE-AAC behavior

HE-AAC is exposed by the `EnableHeAac` parameter and reported through `SceAudiodecM4aacInfo.HeAac`.
The controlled Game-category probe decoded a 24-frame HE-AAC v2
ADTS fixture. Public 24-byte parameters with HE enabled produced nonzero 48 kHz
mono PCM with energy above 12 kHz, confirming SBR reconstruction. HE disabled
produced the 24 kHz mono AAC core, and a valid public 28-byte extended form also
produced 48 kHz mono. Valid public HE-AAC v2 Parametric Stereo configurations
exposed one channel; recovered AvPlayer-style word-size/configuration variants
did not provide a callable public two-channel PS path, and the 16-bit word-size
configuration was rejected.

The production implementation therefore retains native AAC offload, with a
timing-safe AAC-core fallback for PS-intended HLS. It normalizes or duplicates
mono to the 48 kHz stereo AudioOut contract. Do not claim native PS stereo on
firmware 6.02. For inconsistent internet-radio metadata, the application uses
this defensive strategy:

1. Decode one frame with HE-AAC enabled.
2. Inspect the source ADTS signaling and returned `HeAac`/channel/rate fields.
3. If the stream should be treated as ordinary AAC, destroy and recreate the decoder with
   `enable_he_aac = 0`.
4. Decode the frame again with the new context.

This is an application compatibility workaround, not a claim that every stream needs it.

## Raw C ABI

The current native applications define the following compatible structures:

```cpp
typedef struct {
    uint32_t size;
    void *address;
    uint32_t length;
} sce_audiodec_au_info_t;

typedef struct {
    uint32_t size;
    void *address;
    uint32_t length;
} sce_audiodec_pcm_item_t;

typedef struct {
    void *param;
    void *stream_info;
    sce_audiodec_au_info_t *au_info;
    sce_audiodec_pcm_item_t *pcm_item;
} sce_audiodec_ctrl_t;

typedef struct {
    uint32_t size;
    int32_t word_size;
    uint32_t config_number;
    uint32_t sampling_frequency_index;
    uint32_t max_channels;
    uint32_t enable_he_aac;
} sce_audiodec_param_aac_t;

typedef struct {
    uint32_t size;
    uint32_t sampling_frequency;
    uint32_t channel_count;
    uint32_t he_aac;
    int32_t result;
} sce_audiodec_aac_info_t;
```

The essential imports are:

```cpp
extern int sceSysmoduleLoadModule(uint16_t id);
extern int sceSysmoduleUnloadModule(uint16_t id);
extern int sceAudiodecInitLibrary(uint32_t codec_type);
extern int sceAudiodecTermLibrary(uint32_t codec_type);
extern int sceAudiodecCreateDecoder(sce_audiodec_ctrl_t *ctrl, uint32_t codec_type);
extern int sceAudiodecDeleteDecoder(int handle);
extern int sceAudiodecDecode(int handle, sce_audiodec_ctrl_t *ctrl);
```

The AAC codec type is `3`, the MP3 codec type is `2`, signed 16-bit output is `1`, and AudioDec is
module `0x0088`.

## Minimal native lifecycle

```cpp
int module_loaded = sceSysmoduleLoadModule(0x0088) >= 0;
int library_started = module_loaded && sceAudiodecInitLibrary(3) >= 0;

sce_audiodec_param_aac_t param = {
    sizeof(param), 1, 1, 4, 2, 0
};
sce_audiodec_aac_info_t info = { sizeof(info) };
sce_audiodec_au_info_t au = { sizeof(au), nullptr, 0 };
sce_audiodec_pcm_item_t pcm = { sizeof(pcm), nullptr, 0 };
sce_audiodec_ctrl_t ctrl = { &param, &info, &au, &pcm };

int decoder = sceAudiodecCreateDecoder(&ctrl, 3);
if (decoder >= 0) {
    au.address = compressed_frame;
    au.length = compressed_frame_size;
    pcm.address = pcm_buffer;
    pcm.length = pcm_buffer_capacity;
    int result = sceAudiodecDecode(decoder, &ctrl);
    /* pcm.length is now the number of output bytes; au.length is consumed input. */
    sceAudiodecDeleteDecoder(decoder);
}

if (library_started) sceAudiodecTermLibrary(3);
if (module_loaded) sceSysmoduleUnloadModule(0x0088);
```

Production code must check each return value and perform cleanup on every error path. The complete
streaming implementation is in `workspace/dev/psradio/src/radio_service.c`.

## Streaming ADTS correctly

For an internet stream, do not assume one network read equals one AAC frame. Keep a byte buffer and:

1. Search for an ADTS sync word.
2. Parse the 13-bit frame length from the header.
3. Read until the complete frame is buffered.
4. Point `AuInfo.Address/Length` at exactly that frame.
5. Call `sceAudiodecDecode`.
6. Remove only the consumed frame bytes; retain the next partial frame.
7. Append decoded PCM to an output accumulator.

The native radio implementation also caps the amount of data scanned without a valid sync word, which
prevents an unbounded buffer walk when the source is not really AAC. The production path can recreate
the decoder with HE-AAC disabled when the stream metadata or returned output characteristics indicate
that the original HE-AAC interpretation is wrong.

The same production implementation has switched AAC to Opus and Opus back to
AAC without a crash, and validated immediate AAC stop. The final
dual-Opus-decoder build opened an AAC stream, produced PCM, and stayed alive
for the complete observation window.

## Seeking and reset

After seeking, call native `sceAudiodecClearContext` or recreate the RAII
decoder object. The decoder may carry state across frames; clearing prevents
samples from the old position from affecting the new position.

## CPU versus offload

The direct `sceAudiodec*` API is the correct starting point for the AAC hardware/offload POC because
the recovered AvPlayer hardware branch uses the same platform decoder service. It is not a user-selectable
“force hardware” flag in the wrapper. The CPU alternative is an internal AvPlayer path that calls
`sceAudiodecCpuInternalQueryMemSize`, `sceAudiodecCpuInternalInitDecoder`, and
`sceAudiodecCpuInternalDecode`.

`AudioDecCpu = 0x00BD` and the `libSceAudiodecCpu*` modules are present in the inspected platform inventory, but a
standalone tested CPU wrapper is not included. A useful comparison probe would decode the same AAC
frames through both paths and measure CPU time, output equivalence, and setup failures.

## Direct Opus ABI

Static analysis recovered this minimal signed-16 lifecycle:

```cpp
int sceOpusDecInitialize(uint32_t *context);
int sceOpusDecGetSize(int channels);
int sceOpusDecCreateEx(uint32_t *context, void *state, int sample_rate, int channels);
int sceOpusDecDecode(void *state, const void *packet, int packet_bytes,
                     int16_t *pcm, int pcm_capacity_bytes);
int sceOpusDecDestroy(void *state);
int sceOpusDecTerminate(uint32_t *context);
```

Important recovered constraints:

- `sceOpusDecGetSize` returns 640 bytes for one or two channels.
- Valid creation rates are 8000, 12000, 16000, 24000, and 48000 Hz.
- `sceOpusDecDecode` consumes one complete raw Opus packet per call.
- The fifth decode argument is PCM capacity in bytes; a nonnegative successful return is PCM bytes
  produced. Negative values are platform errors.
- Integer output capacity is rounded down to an even byte count. The float variant rounds capacity down
  to a multiple of four and returns float-output bytes.
- The named decode wrapper has no explicit packet-size maximum; downstream AJM may still reject an
  unsupported packet.
- The wrapper registers AJM codec 21 and submits AJMI batches internally.
- `libSceOpusCeltDec` mirrors the shape but registers codec 16; use it only
  for a demonstrated CELT-only input requirement, specifically TOC
  configurations 16-31 in the production route.

Ogg is a container, so an `.opus` file still needs its pages/lacing values parsed into raw packets.
`OpusHead` and `OpusTags` are metadata packets and must not be sent to the decoder. Network protocols
such as Moonlight already deliver raw Opus packets and do not need Ogg demuxing.

This ABI is runtime-proven on firmware 6.02. A production lifecycle probe
stopped one decoder in 67 ms, then switched to a second 48 kHz stereo stream
without a crash. The direct packaged probe also converted a known 20 ms,
48 kHz stereo packet to 3,840 signed-16 PCM bytes.

## Production Opus dispatch

PSRadio parses Ogg incrementally with an allocation-bounded demuxer. It emits
complete compressed Opus packets; `OpusHead` and `OpusTags` are metadata and
are not passed to the decoder. A chained Ogg serial change closes and reopens
the decoder.

Icecast live joins can provide fresh `OpusHead` and `OpusTags` pages with
sequence numbers 0 and 1, then join the current broadcast at a much higher
audio-page sequence number. PSRadio permits exactly one such discontinuity
between validated tags and the first complete audio page. If that page starts
with an orphan continued packet, the incomplete packet is discarded through
its terminating lace. A second pre-audio jump, or any sequence gap after audio
starts, remains an error. The original `ffffffffb` result was signed parser
error `-5` (`OGG_OPUS_ERR_SEQUENCE`), not native Opus decoder result `-502`.

The native decoder selection is based on the Opus TOC byte:

```text
config = packet[0] >> 3

general decoder: libSceOpusDec      internal module 0x80000069, AJM codec 21
                 SILK and hybrid configurations
CELT decoder:    libSceOpusCeltDec  internal module 0x80000044, AJM codec 16
                 TOC configurations 16..31 (CELT-only; RFC 6716 section 3.1)
```

See [RFC 6716 section 3.1](https://www.rfc-editor.org/rfc/rfc6716.html#section-3.1)
for the Opus TOC configuration split.

Production routing attempts the general decoder first. If native decoding
returns intermittent error `-502` (`0xfffffe0a`), it retries once through the
CELT-only decoder when the packet TOC is in configurations 16-31. A mode
change closes and reopens the matching decoder. The retry is a live-stream
recovery measure, not proof that every `-502` is a codec capability failure.

Both public lifecycles have the same shape:

```cpp
sceOpusDecInitialize          / sceOpusCeltDecInitialize
sceOpusDecGetSize             / sceOpusCeltDecGetSize
sceOpusDecCreateEx            / sceOpusCeltDecCreateEx
sceOpusDecDecode              / sceOpusCeltDecDecode
sceOpusDecDestroy             / sceOpusCeltDecDestroy
sceOpusDecTerminate           / sceOpusCeltDecTerminate
```

The companion PSRadio source checks in `libSceOpusCeltDec_stub.a` as generated
import metadata only. It retains the six CELT lifecycle symbol names, has SHA-256
`AFCEAAD3A442CC87412FC96E30716082081AEC846AA1697608910B9FD1F3F51D`, and
does not ship a Sony runtime module with the application.

## Production buffering and cancellation

The PSRadio playback worker separates network/decode from synchronous
AudioOut pacing:

- decoded PCM is held in a bounded two-second ring;
- startup primes approximately one second before AudioOut begins consuming;
- an underrun re-primes approximately 0.5 seconds;
- stop and station switch abort the active HTTP request;
- each cancellable operation has three consecutive attempts with 250, 500, and
  1,000 ms cancellation-aware backoff; the budget is renewed only after real
  AudioOut progress and at least 30 seconds of active playback;
- stop/error discards queued PCM immediately; and
- decoder reset is performed without tearing down AudioOut.

This design prevents a blocked network read or a synchronous output call from
owning the UI thread. The v0.2.0 release hardening matrix also covered forced
reconnects, delayed delivery, malformed Ogg retry exhaustion, rapid switching,
ICY metadata, and live HLS discontinuities without a title crash, fatal signal,
loader failure, or stuck runtime layer. Audible gap characterization and exact
stop/switch latency across broader live-stream shapes remain useful follow-up
measurements.

## Bounded Ogg Vorbis CPU fallback

The hardware-first investigation found no callable native/AJM/AvPlayer Vorbis
PCM route. The recovered AvPlayer decoder factory has no Vorbis branch, and the
inspected AJM Vorbis helper prepares/parses headers but exposes no PCM decode
job. The AvPlayer WebM source probes are not codec evidence because a Vorbis
source, an Opus control, and an AAC/M4A positive control all failed
`sceAvPlayerAddSource` at `stage=source` with `-2140536829` (`0x806a0003`).

PSRadio therefore uses the single-file
[`stb_vorbis.c`](https://github.com/nothings/stb/blob/master/stb_vorbis.c)
push-data API, pinned to nothings/stb commit
`2c980bb59875b0d32144a71867fbdebb2f77cd20`. The vendored file SHA-256 is
`4C7CB2FF1F7011E9D67950446B7EB9CA044F2E464D76BFFB0B84DD2E23E65636`.

The production adapter disables stdio, pull decoding, and integer conversion.
It keeps a 256 KiB compressed-input buffer, accepts mono/stereo at 8-192 kHz,
limits each decode step to 8,192 frames, converts planar float samples to
interleaved signed-16 PCM, and rejects no-progress or allocation-limit
violations before the shared channel/rate normalizer and PCM ring.

The adapter owns the buffering and cancellation policy; callers must not assume
one network read equals one Ogg page or frame. The release also validates full
Ogg page CRC checks, chained streams, and orphan live-join continuation. See
[`VORBIS_VALIDATION.md`](VORBIS_VALIDATION.md) for device evidence.

## Bounded FLAC and Ogg-FLAC CPU fallback

The only firmware FLAC reference is the CPU plug-in
`libSceAudiodecCpuFlac.prx`, internal sysmodule `0x80000053`. It was absent from
the inspected firmware inventory, and a runtime probe returned
`-2141581312` (`0x805a1000`) from
`sceSysmoduleLoadModuleInternal(0x80000053)`. No callable native AJM/AvPlayer
FLAC route was found.

PSRadio uses [`dr_flac.h`](https://github.com/mackron/dr_libs/blob/master/dr_flac.h)
from mackron/dr_libs commit
`b55a0d9a30b91ad8901f89ecf05f76a33186c185`. The header SHA-256 is
`D947F54784467160D30DCA540542BF92CED94965703E5DEEB9E82DB2EC5E0C02`; the
retained MIT-0 license SHA-256 is
`DD1C647E6F767F8FF4B2DFAE0FED314726600A01E0CF1EF556AFDDD5FA96FF15`.

The adapter uses `drflac_open_with_callbacks`, strict open and CRC validation,
custom cancellable reads, bounded forward-seek drain, and
`drflac_read_pcm_frames_s16`. Standard I/O is disabled. It supports native
container FLAC and Ogg-FLAC, mono/stereo and 8-192 kHz, caps source blocks at
8,192 frames, reads at 4,096 frames, and caps allocations and opening reads at
1 MiB. Output joins the shared signed-16 channel/rate normalizer and PCM ring.

These are CPU-decoding paths and must not be described as hardware/firmware
offload. See [`FLAC_VALIDATION.md`](FLAC_VALIDATION.md) for native and Ogg-FLAC
device records.

## HLS is transport, not a decoder

HLS supplies playlists and media segments; it does not replace the elementary
audio decoder. PSRadio's first slice uses the bounded design from
`workspace/dev/ps5-iptv-client`: master/media parsing, URL resolution, live
reload, stale-segment suppression, discontinuity handling, MPEG-TS PAT/PMT/PES
parsing, and ADTS extraction. It accepts unencrypted audio-only MPEG-TS
carrying AAC-LC, then submits complete ADTS frames to the validated native AAC
path.

Encryption, byte ranges, fMP4/CMAF, low-latency HLS, alternate renditions,
multi-program transport streams, video, and non-AAC audio are rejected until a
separate implementation and validation pass exists. A generic Radio Browser
`OGG` record must not be advertised as Opus unless its resolved stream is
explicitly identified as Opus.

## What the AAC POCs prove

The [native AAC probe](../examples/native-audio-poc/README.md) loads only
`AudioDec` (`0x0088`), creates codec type `3`, obtains signed-16 PCM, and
submits it to native `AudioOut`. It does not use FFmpeg, libopus, or a software
AAC implementation.

The production PSRadio path provides the device runtime evidence for native
AAC/MP3/Opus and the CPU Vorbis/FLAC fallbacks plus HLS/AAC integration. The
POC remains a minimal repeatable AAC experiment; neither it nor the production
path identifies a physical DSP. Use
“hardware/firmware audio offload” or “AJM-backed native decoding.”
