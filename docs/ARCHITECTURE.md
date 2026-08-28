# Audio architecture and design rules

This document explains how the pieces fit together and which part of the system owns each buffer.

## The four layers

### 1. Source and container layer

The source may be a local file, a network stream, or an application-owned packet queue. A container such
as MP4 is not itself an audio codec. It contains tracks, and the track codec determines the next layer.

`libSceAvPlayer` combines demuxing, codec selection, worker threads, and decoded-frame queues. The local
`MediaPlayer` wrapper exposes the result as `AudioFrame` and `VideoFrame` values.

For a radio or Moonlight client, the application usually owns the network and framing. In that case,
`AudioDecoder` or a native `sceAudiodec*` loop is a better fit than AvPlayer.

### 2. Compressed decode layer

The direct public decoder path is:

```text
AudioDecoder / sceAudiodec*
        |
        +--> AAC / MP3 / Opus native or bounded CPU fallback
        |
        +--> libSceAjm / libSceAjmi
                |
                +--> /dev/ajm
                        ioctl + batch job + completion
```

AvPlayer contains a second, internal split:

```text
AvPlayer decoder factory
        |
        +--> AudioDecHw(AAC)  -> sceAudiodecDecodeEx -> AJM path
        +--> AudioDecSw(AAC)  -> sceAudiodecCpuInternalDecode
        +--> AudioDecHw(Opus) -> shared hardware decoder base
        +--> AudioDecHw(AC3)
        +--> AudioDecSw(Eac3)
```

The names above are literal strings recovered from the AvPlayer binary. They are not guesses based on
library names.

### 3. PCM processing layer

Once decoded, audio is ordinary application-owned PCM. Keep these operations
in small native C++20 components:

- resampling, channel conversion, gain, normalization, trim, and concatenation;
- bounded voice mixing and voice limits;
- procedural tone generation and lightweight stateful DSP; and
- bounded WAV or VAG asset parsing when runtime conversion is actually needed.

For a custom graph, `libSceNgs2` provides sampler, submixer, reverb, mastering, stream, geometry, and
pan APIs. NGS2 is a lower-level engine than `AudioMixer` and requires caller-owned option structures,
query-sized buffers, racks, voices, and render buffers.

### 4. Output and spatial layer

The simplest output is `libSceAudioOut`: open a port, submit one fixed-size block, and repeat. The call
is blocking enough to pace a producer against the audio queue, so the producer belongs on an audio
thread rather than the UI/render thread.

For object-based or spatial output, use the raw `AudioOut2` or `Audio3d` bindings. They require explicit
port/context setup and attribute arrays. Do not mix their queueing model with `AudioOutDevice` without
deciding which subsystem owns timing and buffering.

## Recommended application shapes

### Local music player

```text
MediaPlayer.Open(path)
    -> Start()
    -> TryGetAudioFrame()
    -> AudioClip channel/rate adaptation if needed
    -> AudioOutDevice.Output(block)
```

This is the shortest path for local `.m4a`, `.mp4`, `.mov`, and `.webm` playback. AvPlayer owns demuxing
and its decoder threads.

### Radio or network audio

```text
HTTP/network thread
    -> byte ring / ADTS frame finder
    -> AudioDecoder.Decode()
    -> PCM ring or mixer
    -> AudioOutDevice.Output()
```

Keep network reads, decode, and output decoupled. The existing native PSRadio implementation is a useful
reference: it finds complete ADTS/MP3 frames, incrementally demuxes Ogg, parses
bounded HLS/MPEG-TS, retries decoder creation when HE-AAC behavior requires it,
resamples to the fixed 48 kHz output, and fills 256-frame stereo blocks. Its
production AAC path uses `libSceAudiodec` codec 3 and its MP3 path uses codec 2
through AJM.

### Production Opus

```text
Ogg pages/lacing
    -> bounded complete Opus packets
    -> libSceOpusDec codec 21 (general decoder first)
    -> native -502 on TOC 16..31: one retry through libSceOpusCeltDec codec 16
    -> signed-16 PCM
```

The TOC mode change or an Ogg serial change closes and reopens the matching
decoder. SILK and hybrid packets remain on `libSceOpusDec`; only TOC
configurations 16-31 are eligible for the CELT-only recovery route.

The production playback worker places decoded PCM in a bounded two-second ring,
primes approximately one second before starting AudioOut, and re-primes about
0.5 seconds after an underrun. It aborts active HTTP requests, makes three
consecutive attempts with 250/500/1,000 ms cancellation-aware backoff, discards
PCM immediately on stop/error, and resets the decoder without tearing down
AudioOut. The retry budget is renewed only after real AudioOut progress and
30 seconds of active playback.

### Vorbis and FLAC CPU fallbacks

```text
Ogg pages / FLAC callbacks
    -> bounded stb_vorbis or dr_flac
    -> signed-16 PCM
    -> shared channel conversion and 48 kHz resampling
```

No callable native Vorbis route was found, and the internal FLAC CPU plug-in
`0x80000053` was absent/unloadable on firmware 6.02. These two paths are
application CPU decoders. They use bounded input, allocation, and decode-step
limits before joining the same PCM ring and AudioOut consumer as native AAC,
MP3, and Opus.

### HLS/AAC

```text
HLS master/media playlists
    -> bounded URL resolution and live reload
    -> MPEG-TS PAT/PMT/PES
    -> AAC-LC ADTS
    -> native libSceAudiodec codec 3
```

HLS is transport, not a decoder. The supported slice is unencrypted,
audio-only MPEG-TS with one AAC-LC ADTS stream. Encryption, byte ranges,
fMP4/CMAF, LL-HLS parts, alternate renditions, video/multiprogram transport,
and non-AAC audio are rejected before decode dispatch.

### Moonlight Opus

```text
Opus RTP depacketization
    -> native libSceOpusDec/libSceOpusCeltDec path
    -> CPU libopus compatibility fallback
    -> PCM ring
    -> AudioOutDevice / sceAudioOut
```

The native general and CELT paths are runtime-proven in PSRadio. A native
`-502` is intermittent/live-packet-dependent on some WALM variants; the
production one-time CELT retry is recovery logic, not proof that the general
decoder or station is permanently unsupported.

## Ownership and lifetime rules

- Load a required system module before creating objects that use it.
- Keep input and output buffers alive for the duration of the native call.
- `sceAudiodecDecode` updates the input/output lengths in native control structures; copy or consume
  the returned PCM before reusing the buffer.
- AvPlayer audio/video frame memory belongs to AvPlayer and stays valid only until the next frame request
  or player teardown. Consume or copy it immediately.
- An `AudioOut` call consumes one full block. Hold partial decoded frames until a complete block is
  available; zero-fill the final tail before draining.
- Dispose decoder/player objects before unloading their system modules.
- A decoder instance is not thread-safe; use one decoder per decoding thread.
- Never call a blocking output write from the render frame loop or controller polling loop.

## Timing and format rules

The canonical native AudioOut sink opens signed-16 stereo at 48 kHz. Most application
audio should therefore be normalized to:

```text
sample rate: 48000 Hz
channels:    2
format:      signed 16-bit interleaved
grain:       256, 512, 768, 1024, 1280, 1536, 1792, or 2048 frames
```

The decoder's rate is determined by the stream. AAC reports sample rate and channel count after a
successful frame. Read MP3 rate and channels from the validated frame header or
use a container/stream description before opening output. Resample and duplicate mono to stereo before
submitting to a fixed stereo port.

## Hardware wording

Use these terms precisely:

- **CPU software decode**: the AvPlayer branch calls `sceAudiodecCpuInternal*`.
- **Platform audio decode**: the call goes through `libSceAudiodec` and its registered codec service.
- **AJM offload**: the service reaches `libSceAjm`, `/dev/ajm`, and ioctl-backed batch jobs.
- **Fixed-function DSP confirmed**: not established by the current evidence.

The repository uses “hardware/offload” for the AJM path because that is the strongest defensible claim,
while explicitly avoiding a claim about the exact silicon block. Runtime
success through AJM strengthens the offload claim but does not identify a
particular physical decoder.
