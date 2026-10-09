# Vorbis validation

## Platform decoding

Platform decoding of Vorbis was not tested conclusively. The only device test
opened local WebM files through the platform media player, and it failed
identically for Vorbis, Opus, and an AAC/M4A control. The test application
decodes Vorbis itself.

## In-application decoder

The application uses [`stb_vorbis.c`](https://github.com/nothings/stb/blob/master/stb_vorbis.c),
pinned to nothings/stb commit
`2c980bb59875b0d32144a71867fbdebb2f77cd20`.

| Property | Limit in the application |
|---|---|
| Input API | Incremental push-data API |
| Disabled surfaces | stdio, pull decoding, and integer-conversion API |
| Channels | Mono or stereo |
| Sample rate | 8,000 through 192,000 Hz |
| Compressed input | 256 KiB maximum |
| Decode step | 8,192 frames maximum |
| Safety | Explicit no-progress and allocation ceilings |
| Output | Signed-16 PCM, normalized by the shared output path |

The `stb_vorbis.c` SHA-256 is
`4C7CB2FF1F7011E9D67950446B7EB9CA044F2E464D76BFFB0B84DD2E23E65636`.

The decoder is fed Ogg data as it arrives. Decoded samples enter the same
two-second PCM ring, channel normalization, 48 kHz resampler, and cancellable
output consumer as the other codecs.

## Device results

A 44.1 kHz stereo Vorbis stream played, then switched directly back to
HLS/AAC and stopped. A longer run played at 44.1 kHz stereo for about eleven
minutes without a rebuffer or error, then closed normally.

The fault-injected matrix added full Ogg page CRC validation, bounded
malformed-page failure, logical-stream chaining, orphan live-join
continuation, ICY metadata stripping, and chained Vorbis decoder recreation.
Chained Vorbis playback completed at 44.1 kHz stereo without a title crash or
a stuck title.

## References and alternatives

The [Xiph Vorbis project](https://xiph.org/vorbis/) and its
[Vorbis I specification](https://xiph.org/vorbis/doc/Vorbis_I_spec.pdf) are
the format references. [Tremor](https://wiki.xiph.org/Tremor) is an
integer-only alternative. `libvorbis` is mature, but it is a larger multi-file
library with a libogg dependency.

More malformed stream shapes and other system software versions and sample
rates are not tested yet.
