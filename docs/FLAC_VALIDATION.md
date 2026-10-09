# FLAC validation

## Platform decoding

One device test tried to load a platform FLAC decoder on the tested system
software; the load failed. No other platform FLAC test was run. The test
application decodes FLAC itself.

## In-application decoder

The application uses [`dr_flac.h`](https://github.com/mackron/dr_libs/blob/master/dr_flac.h),
pinned to mackron/dr_libs commit
`b55a0d9a30b91ad8901f89ecf05f76a33186c185`.

| Property | Limit in the application |
|---|---|
| Input API | Custom callback reads and bounded forward-seek drain |
| Containers | FLAC and Ogg-FLAC |
| Validation | Strict open and CRC validation |
| Disabled surface | stdio |
| Channels | Mono or stereo |
| Sample rate | 8,000 through 192,000 Hz |
| Source block | 8,192 frames maximum |
| PCM read | 4,096 frames maximum |
| Memory/opening scan | 1 MiB allocation and opening-read ceilings |
| Cancellation | Callback reads observe the active stop state |
| Output | Signed-16 PCM, normalized by the shared output path |

The header SHA-256 is
`D947F54784467160D30DCA540542BF92CED94965703E5DEEB9E82DB2EC5E0C02`.
The retained MIT-0 license text SHA-256 is
`DD1C647E6F767F8FF4B2DFAE0FED314726600A01E0CF1EF556AFDDD5FA96FF15`.

Decoded samples enter the shared two-second PCM ring, channel normalization,
48 kHz resampler, and cancellable output consumer.

## Device results

A FLAC stream played at 44.1 kHz stereo, stopped, and switched to AAC. An
Ogg-FLAC run stayed active at 44.1 kHz stereo for more than eleven minutes
with no error or rebuffer.

Fault-injected FLAC playback reached 44.1 kHz stereo and stopped cleanly,
without a title crash or a stuck title.

## Host results

Host checks cover FLAC and Ogg-FLAC, split input, truncation, invalid
signatures, malformed metadata, oversized blocks, bounded opening scans, and
memory/read ceilings. ASan/UBSan pass for the FLAC check.

More malformed live-source shapes and other system software versions and
sample rates are not tested yet.
