# Vorbis validation and fallback

## Native-route result

No callable native/AJM/AvPlayer PCM route was found on the inspected PS5
firmware baseline. The recovered AvPlayer decoder factory exposes AAC
hardware/software, Opus hardware, AC-3 hardware, and E-AC-3 software branches,
but no Vorbis branch. The inspected AJM Vorbis helper prepares and parses
headers but exposes no PCM decode job.

The AvPlayer ABI itself initialized successfully, but its source probes cannot
answer the codec question. `sceAvPlayerAddSource` rejected Vorbis/WebM,
Opus/WebM, and an AAC/M4A positive control at `stage=source` with the identical
`-2140536829` (`0x806a0003`). The identical positive-control failure means
these probes do not discriminate codec support and must not be reported as
runtime proof that AvPlayer lacks Vorbis.

The evidence-bounded conclusion is therefore: **no callable native Vorbis route
was found**. This is different from claiming that every Sony container path
cannot contain or decode Vorbis.

## Implemented CPU path

PSRadio uses [`stb_vorbis.c`](https://github.com/nothings/stb/blob/master/stb_vorbis.c),
pinned to nothings/stb commit
`2c980bb59875b0d32144a71867fbdebb2f77cd20`.

| Property | Production bound |
|---|---|
| Input API | Incremental push-data API |
| Disabled surfaces | stdio, pull decoding, and integer-conversion API |
| Channels | Mono or stereo |
| Sample rate | 8,000 through 192,000 Hz |
| Compressed input | 256 KiB maximum |
| Decode step | 8,192 frames maximum |
| Safety | Explicit no-progress and allocation ceilings |
| Output | Signed-16 PCM, normalized by the shared PS5 AudioOut path |

The vendored `stb_vorbis.c` SHA-256 is
`4C7CB2FF1F7011E9D67950446B7EB9CA044F2E464D76BFFB0B84DD2E23E65636`.

The decoder is fed complete or incrementally available Ogg data by the
allocation-bounded stream worker. Decoded samples enter the same two-second
PCM ring, channel normalization, 48 kHz resampler, and cancellable AudioOut
consumer used by the native codecs.

## Device evidence

Device testing played a 44.1 kHz stereo Vorbis stream, then switched directly
back to HLS/AAC and stopped. A sustained run played at 44.1 kHz stereo for
about eleven minutes without a rebuffer/error transition, then closed and
released runtime layers normally.

The fault-injected matrix extended this evidence with full Ogg page CRC validation, bounded
malformed-page failure, logical-stream chaining, orphan live-join continuation,
ICY metadata stripping, and chained Vorbis decoder recreation. The fault matrix
also completed chained Vorbis playback at 44.1 kHz stereo without a title
crash, fatal signal, loader failure, or stuck runtime layer.

## Alternatives and remaining scope

The [Xiph Vorbis project](https://xiph.org/vorbis/) and its
[Vorbis I specification](https://xiph.org/vorbis/doc/Vorbis_I_spec.pdf) are
the format references. [Tremor](https://wiki.xiph.org/Tremor) is an integer-only
alternative that may be useful if floating-point cost becomes material.
`libvorbis` is mature, but its larger multi-file API and libogg dependency make
it a less focused first choice for a live-radio application.

Additional malformed stream shapes and broader firmware/sample-rate coverage
remain useful follow-up work. They do not invalidate the implemented and
device-validated baseline.
