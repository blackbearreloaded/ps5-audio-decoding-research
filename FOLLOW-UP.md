# Follow-up and codec roadmap

The production audio implementation and release hardening are complete for the
validated PS5 firmware 6.02 baseline. The remaining items below are broader
coverage and fidelity boundaries; they are not evidence that AAC, MP3, Opus,
Vorbis, FLAC, or the bounded HLS/AAC path lacks an implementation.

The finalized source of truth is [PSRadio v0.2.0 commit `5c25b46`](https://github.com/blackbearreloaded/psradio/tree/5c25b4651efdcfb034431054e2b7e0309d4c88d2),
including its [`docs/CODEC_INVESTIGATION.md`](https://github.com/blackbearreloaded/psradio/blob/5c25b4651efdcfb034431054e2b7e0309d4c88d2/docs/CODEC_INVESTIGATION.md)
and [`docs/AUDIO_RELEASE_VALIDATION.md`](https://github.com/blackbearreloaded/psradio/blob/5c25b4651efdcfb034431054e2b7e0309d4c88d2/docs/AUDIO_RELEASE_VALIDATION.md).
The earlier [`cba75ee`](https://github.com/blackbearreloaded/psradio/tree/cba75ee)
CELT-routing milestone remains a useful historical checkpoint.

## Completed native milestones

- AAC-LC ADTS uses `libSceAudiodec` codec type `3` through `AudioDec` module
  `0x0088`. It reaches device-backed AJM and sends signed-16 PCM to AudioOut.
  Decoder recreation with HE-AAC disabled remains available when stream
  metadata or output characteristics require the timing-safe fallback.
- MP3 uses `libSceAudiodec` codec type `2` through AJM. A live 48 kHz stereo
  stream produced 4,608 bytes of signed-16 PCM and reached AudioOut.
- General Opus uses `libSceOpusDec`, internal module `0x80000069`, and AJM
  codec `21`. CELT-only Opus uses `libSceOpusCeltDec`, internal module
  `0x80000044`, and AJM codec `16` as the one-time recovery route for TOC
  configurations 16-31 after a native `-502` result.
- A production lifecycle probe stopped in 67 ms and switched to a second
  48 kHz stereo stream without a crash. The general Opus path also completed a
  ten-minute live soak.

Use **hardware/firmware audio offload** or **AJM-backed native decoding** for
these native paths. The evidence does not prove a particular physical DSP or
fixed-function decoder block.

## Completed software fallbacks

### Ogg Vorbis: bounded `stb_vorbis`

Static inspection found no callable native/AJM/AvPlayer PCM route. The recovered
AvPlayer decoder factory has no Vorbis branch, and the inspected AJM Vorbis
helper builds/parses headers but exposes no PCM decode job. AvPlayer source
probes are not codec evidence: Vorbis/WebM, Opus/WebM, and an AAC/M4A positive
control all rejected `sceAvPlayerAddSource` at `stage=source` with identical
`-2140536829` (`0x806a0003`).

PSRadio uses the single-file [`stb_vorbis.c`](https://github.com/nothings/stb/blob/master/stb_vorbis.c)
push-data API, pinned to nothings/stb commit
`2c980bb59875b0d32144a71867fbdebb2f77cd20`. The vendored file SHA-256 is
`4C7CB2FF1F7011E9D67950446B7EB9CA044F2E464D76BFFB0B84DD2E23E65636`.
The production configuration disables stdio, pull decoding, and integer
conversion; accepts mono/stereo and 8-192 kHz; caps compressed input at
256 KiB; limits each decode step to 8,192 frames; and rejects no-progress or
allocation-limit violations.

[Xiph Vorbis](https://xiph.org/vorbis/) and the
[Vorbis I specification](https://xiph.org/vorbis/doc/Vorbis_I_spec.pdf) are the
format references. [Tremor](https://wiki.xiph.org/Tremor) is a reasonable
integer-only alternative when floating-point cost is unacceptable. `libvorbis`
is mature but is a larger multi-file API with a libogg dependency, so neither
is the first choice for this focused live-radio fallback.

### FLAC and Ogg-FLAC: bounded `dr_flac`

`libSceAudiodecCpu.sprx` references `libSceAudiodecCpuFlac.prx` as internal
sysmodule `0x80000053`. This is a CPU plug-in reference, not AJM/hardware
evidence. It was absent from the inspected firmware inventory, and a runtime
probe reported `-2141581312` (`0x805a1000`) from
`sceSysmoduleLoadModuleInternal(0x80000053)`. No callable native AJM/AvPlayer
FLAC route was found on this baseline.

PSRadio uses the single-file [`dr_flac.h`](https://github.com/mackron/dr_libs/blob/master/dr_flac.h)
callback decoder, pinned to mackron/dr_libs commit
`b55a0d9a30b91ad8901f89ecf05f76a33186c185`. The header SHA-256 is
`D947F54784467160D30DCA540542BF92CED94965703E5DEEB9E82DB2EC5E0C02` and the
retained MIT-0 license text SHA-256 is
`DD1C647E6F767F8FF4B2DFAE0FED314726600A01E0CF1EF556AFDDD5FA96FF15`.

The integration enables strict open and CRC validation, no stdio, cancellable
callback reads, bounded forward-seek drain, mono/stereo and 8-192 kHz input,
source blocks of at most 8,192 frames, reads of at most 4,096 frames, a 1 MiB
allocation/opening-read ceiling, and signed-16 output. It supports both native
container FLAC and Ogg-FLAC.

## Completed HLS slice

HLS is transport and delivery, not a codec. The first PSRadio slice reuses the
bounded design from `workspace/dev/ps5-iptv-client`:

- bounded master/media playlists and relative URL resolution;
- live reload and stale-segment suppression;
- discontinuity reset;
- MPEG-TS PAT/PMT/PES parsing and ADTS extraction; and
- native AAC-LC decoding through `libSceAudiodec` codec `3`.

The supported input is unencrypted, audio-only MPEG-TS carrying one AAC-LC
ADTS elementary stream. It explicitly rejects encryption, byte ranges,
fMP4/CMAF, LL-HLS parts, alternate rendition groups, video, multiprogram
transport streams, and non-AAC variants. HLS behavior is bounded by
[RFC 8216](https://www.rfc-editor.org/rfc/rfc8216.html).

## Shared streaming and output hardening

All codecs feed one cancellable PS5 AudioOut path with channel normalization
and 48 kHz resampling where needed. The decoded-PCM ring is two seconds; the
initial prime is approximately one second and underrun re-prime is
approximately 0.5 seconds. Stop/error aborts the active HTTP request and
discards queued PCM immediately. Decoder reset does not tear down AudioOut.

Retries are three consecutive attempts with 250/500/1,000 ms
cancellation-aware backoff. The retry budget is renewed only after real
`sceAudioOutOutput` progress and at least 30 seconds of active playback. This
prevents lifetime-budget exhaustion on long streams without turning a
persistent failure into unlimited retries.

Generic Radio Browser `OGG` records remain visible because their metadata is
ambiguous. PSRadio signature-probes the resolved stream and routes it to Opus,
Vorbis, or Ogg-FLAC. Unknown Ogg payloads fail before reaching a mismatched
decoder; an OGG catalog label alone must never be advertised as Opus.

## Completed release hardening

PSRadio v0.2.0 completed the formerly open recovery and container gates: three
forced AAC reconnects, delayed Opus underrun, malformed Ogg
Opus retry exhaustion, three live HLS discontinuity cycles, ICY AAC stripping,
chained Vorbis, MP3, FLAC, and 12 rapid cross-codec switches. The run recorded
92 telemetry events and 16 status reads without a title crash, fatal signal,
loader failure, or stuck runtime layer. See
[Audio release validation](docs/AUDIO_RELEASE_VALIDATION.md) for the complete
matrix and package hash.

The HE-AAC probe also confirmed SBR reconstruction: HE-enabled
public parameters produced nonzero 48 kHz mono PCM with energy above 12 kHz,
while HE-disabled output was the 24 kHz mono AAC core. Valid public
HE-AAC v2 Parametric Stereo configurations exposed one channel, and no callable
public native two-channel PS path was established on firmware 6.02. Production
therefore retains the timing-safe mono AAC-core fallback for PS-intended HLS
and normalizes or duplicates mono to the 48 kHz stereo AudioOut contract.

## Remaining scope

The next work is broader coverage, not missing baseline decoder
implementations:

- characterize audible gaps and exact stop/switch latency across more live
  sources;
- test additional firmware revisions and stream shapes;
- investigate a callable native two-channel Parametric Stereo path only if a
  future firmware exposes one; and
- expand HLS only with separate evidence for encryption, byte ranges,
  fMP4/CMAF, LL-HLS, alternate renditions, multiprogram/video variants, or
  non-AAC elementary streams.

The full release and device smoke validation is recorded in
[docs/AUDIO_RELEASE_VALIDATION.md](docs/AUDIO_RELEASE_VALIDATION.md). Do not
report the native PS-stereo boundary as absent AAC support.

Further static analysis is not required before using the implemented baseline.
New investigation is justified only for a specific missing control, ABI
completeness, or a newly targeted format.
