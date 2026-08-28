# FLAC validation and fallback

## Native-route result

Static inspection found one relevant firmware reference:
`libSceAudiodecCpu.sprx` references `libSceAudiodecCpuFlac.prx` as internal
sysmodule `0x80000053`. This is a CPU plug-in reference, not AJM or hardware
evidence. The plug-in was absent from the inspected firmware inventory.

The runtime probe called
`sceSysmoduleLoadModuleInternal(0x80000053)` and received
`-2141581312` (`0x805a1000`). No usable native FLAC path exists on this
firmware 6.02 baseline. No callable AJM or AvPlayer FLAC route was found.

## Implemented CPU path

PSRadio uses [`dr_flac.h`](https://github.com/mackron/dr_libs/blob/master/dr_flac.h),
pinned to mackron/dr_libs commit
`b55a0d9a30b91ad8901f89ecf05f76a33186c185`.

| Property | Production bound |
|---|---|
| Input API | Custom callback reads and bounded forward-seek drain |
| Containers | Native-container FLAC and Ogg-FLAC |
| Validation | Strict open and CRC validation |
| Disabled surface | stdio |
| Channels | Mono or stereo |
| Sample rate | 8,000 through 192,000 Hz |
| Source block | 8,192 frames maximum |
| PCM read | 4,096 frames maximum |
| Memory/opening scan | 1 MiB allocation and opening-read ceilings |
| Cancellation | Callback reads observe the active stop state |
| Output | Signed-16 PCM, normalized by the shared PS5 AudioOut path |

The header SHA-256 is
`D947F54784467160D30DCA540542BF92CED94965703E5DEEB9E82DB2EC5E0C02`.
The retained MIT-0 license text SHA-256 is
`DD1C647E6F767F8FF4B2DFAE0FED314726600A01E0CF1EF556AFDDD5FA96FF15`.

Decoded samples enter the shared two-second PCM ring, channel normalization,
48 kHz resampler, and cancellable AudioOut consumer. The fallback is CPU
decoding; it must not be described as hardware/firmware offload.

## Device evidence

Device testing played native-container FLAC at 44.1 kHz stereo, stopped, and
switched to AAC. A sustained Ogg-FLAC run remained active at 44.1 kHz stereo
for more than eleven minutes with no later error or rebuffer.

Fault-injected FLAC playback reached 44.1 kHz stereo and stopped cleanly. The
same release hardening run exercised the shared cancellation, retry, PCM-ring,
and AudioOut lifecycle without a title crash, fatal signal, loader failure, or
stuck runtime layer.

## Host and target boundary

Host checks cover native and Ogg-FLAC, split input, truncation, invalid
signatures, malformed metadata, oversized blocks, bounded opening scans, and
memory/read ceilings. ASan/UBSan pass for the FLAC check.

Additional malformed live-source shapes and broader firmware/sample-rate
coverage remain follow-up work. They are not reasons to omit the implemented
FLAC decoder from the baseline.
