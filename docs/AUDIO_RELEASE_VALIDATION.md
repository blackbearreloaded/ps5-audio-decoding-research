# PSRadio v0.2.0 audio release validation

This record captures the finalized production evidence from [PSRadio
v0.2.0](https://github.com/blackbearreloaded/psradio/tree/5c25b4651efdcfb034431054e2b7e0309d4c88d2),
released from commit `5c25b4651efdcfb034431054e2b7e0309d4c88d2`. The detailed
companion records are [`docs/CODEC_INVESTIGATION.md`](https://github.com/blackbearreloaded/psradio/blob/5c25b4651efdcfb034431054e2b7e0309d4c88d2/docs/CODEC_INVESTIGATION.md)
and [`docs/AUDIO_RELEASE_VALIDATION.md`](https://github.com/blackbearreloaded/psradio/blob/5c25b4651efdcfb034431054e2b7e0309d4c88d2/docs/AUDIO_RELEASE_VALIDATION.md).
Device evidence below was collected on firmware 6.02.

The accurate terminology is **hardware/firmware audio offload** or
**AJM-backed native decoding**. The evidence establishes the platform service
and AJM path; it does not identify a particular physical DSP or fixed-function
decoder block.

## Final decoder boundary

| Format or path | Final result | Production route |
|---|---|---|
| AAC-LC ADTS | Native `libSceAudiodec` codec `3` reaches the AJM-backed firmware path and produces PCM for AudioOut. | Network ADTS framing, native AAC decoder, shared PCM/output path |
| MP3 | Native `libSceAudiodec` codec `2` reaches AJM. A live stream produced 4,608 bytes of signed-16 PCM at 48 kHz stereo and reached AudioOut; the cross-format matrix also played and stopped a current live MP3. | Native `sceAudiodec*` lifecycle |
| Ogg Opus | Native general `libSceOpusDec` codec `21`, with `libSceOpusCeltDec` codec `16` for CELT-only TOC configurations. SILK and hybrid packets remain on the general decoder. | General decoder first; TOC configurations 16–31 retry once through the CELT decoder only after native `-502` |
| Ogg Vorbis | No callable native/AJM/AvPlayer PCM route was found. The recovered AvPlayer source probes are not a codec discriminator. | Bounded pinned `stb_vorbis` CPU decoder |
| FLAC / Ogg-FLAC | No callable native route was found. The only firmware reference is the absent CPU plug-in `libSceAudiodecCpuFlac.prx` / internal sysmodule `0x80000053`; a runtime load returned `0x805a1000`. | Bounded pinned `dr_flac` CPU decoder |
| HLS | Delivery/container processing, not a codec. | Bounded unencrypted audio-only MPEG-TS/AAC-LC subset feeding native AAC |

Vorbis and FLAC CPU decoding are application-owned fallbacks. They must not be
described as hardware/firmware offload.

## HE-AAC result

A controlled Game-category probe decoded a 24-frame HE-AAC v2 ADTS fixture.

- Public 24-byte AAC parameters with HE enabled produced nonzero 48 kHz mono
  PCM with energy above 12 kHz. SBR reconstruction is confirmed.
- HE disabled produced the 24 kHz mono AAC core.
- A valid public 28-byte extended form also produced 48 kHz mono.
- Every valid public configuration exposed HE-AAC v2 Parametric Stereo as one
  channel.
- Recovered AvPlayer-style word-size/configuration variants did not provide a
  callable public two-channel PS path; the 16-bit word-size configuration was
  rejected.

Production therefore retains native AAC offload and the timing-safe AAC-core
fallback for PS-intended HLS, then normalizes or duplicates mono to the 48 kHz
stereo AudioOut contract. Do not claim native PS stereo on firmware 6.02.

## Completed stream and container hardening

The release implementation includes:

- full Ogg page CRC validation before dispatch;
- bounded failure for truncated, inconsistent, and serial-invalid Ogg pages;
- Ogg logical-stream chaining, one validated header-to-first-audio sequence
  jump for Icecast live joins, and orphan continuation handling;
- Opus output gain, pre-skip, end-granule trimming, bounded padded packets, and
  clean decoder resets;
- decoder recreation for chained Vorbis streams;
- ICY metadata interval parsing and stripping before codec framing, including
  split blocks and malformed/truncated metadata;
- HLS discontinuities that drain old PCM, recreate AAC, clear ADTS framing, and
  reopen output with new sample-rate/channel geometry; and
- three bounded reconnect attempts with 250/500/1,000 ms cancellation-aware
  backoff. The failure budget renews only after real AudioOut production and 30
  seconds of stable playback.

The shared playback path uses a cancellable active HTTP request, a two-second
decoded-PCM ring, approximately one second of initial prime, approximately
0.5 seconds of underrun re-prime, immediate PCM discard on stop/error, and
decoder reset without tearing down AudioOut.

## Fault-injected device matrix

The release hardening matrix recorded 92 telemetry events, 16 status reads,
and 12 rapid cross-codec switches.

| Scenario | Observed result |
|---|---|
| AAC reconnect | Three forced server disconnects; each recovery reached 48 kHz stereo. |
| Opus underrun | Delayed delivery reached and retained 48 kHz stereo without a hang. |
| Malformed Ogg Opus | Exactly three retries, then error `-12`, with no crash or UI lock. |
| HLS discontinuity | 44.1 kHz stereo to 48 kHz mono across three cycles with fresh output geometry. |
| ICY AAC | Metadata was stripped and 48 kHz stereo output was reached. |
| Chained Vorbis | 44.1 kHz stereo playback completed. |
| MP3 | 48 kHz stereo playback completed. |
| FLAC | 44.1 kHz stereo playback completed and the final state stopped cleanly. |

There was no title crash, fatal signal, loader failure, or stuck runtime layer.

## Release validation

The complete local and GitHub CI suite passed lint, host regressions, the
deterministic clean-room libc rebuild, the PS5 build, and FFPFSC packaging.

The downloaded release asset was verified by upload and download round trips
and launched as a Game-category application. It displayed the expected
version, played and switched live stations, visibly reached MP3 at 48 kHz
stereo, and closed cleanly. Runtime monitoring confirmed normal resource
release. The final production build has zero unresolved imports.

## Evidence boundary and remaining scope

This release proves the implemented AAC, MP3, Opus, bounded Vorbis, bounded
FLAC/Ogg-FLAC, and bounded HLS/AAC baseline on the tested firmware. SBR
reconstruction is proven, but a callable public native two-channel Parametric
Stereo path was not established; production intentionally uses the timing-safe
mono AAC-core fallback for PS-intended HLS.

The results do not automatically generalize to every firmware, stream shape,
or HLS feature. Encryption, byte ranges, fMP4/CMAF, LL-HLS parts, alternate
renditions, video/multiprogram variants, and non-AAC HLS remain outside the
implemented boundary. Generic Radio Browser OGG records must be signature-
probed and routed to Opus, Vorbis, or Ogg-FLAC; an ambiguous catalog label must
not be advertised as Opus.
