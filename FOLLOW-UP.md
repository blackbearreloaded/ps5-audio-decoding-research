# Follow-up

The decoder routes used by the test application are implemented and
device-tested on the tested system software: platform decoding for AAC, MP3,
and Opus, in-application decoding for Vorbis and FLAC, and a bounded HLS/AAC
path. The device record is in [docs/VALIDATION.md](docs/VALIDATION.md) and
[docs/AUDIO_RELEASE_VALIDATION.md](docs/AUDIO_RELEASE_VALIDATION.md).

## Remaining scope

- Measure audible gaps and exact stop/switch latency across more live sources.
- Test other system software versions, sample rates, and stream shapes.
- Two-channel HE-AAC v2 (Parametric Stereo): every configuration tested
  produced one channel. The test application plays such streams as mono
  duplicated to stereo.
- HLS beyond the current subset (encryption, byte ranges, fMP4/CMAF, LL-HLS,
  alternate renditions, video or multi-program streams, non-AAC audio) is not
  implemented and needs its own tests.
- Local container playback (MP4/M4A/WebM) through the platform media player
  was not established: opening the source failed identically for every test
  file, including an AAC/M4A control.
- Platform decoding of Vorbis was not tested conclusively.
- More malformed-stream shapes for the Vorbis and FLAC decoders.

## Alternatives noted

[Tremor](https://wiki.xiph.org/Tremor) is an integer-only Vorbis decoder that
may be useful if floating-point cost matters. `libvorbis` is mature but is a
larger multi-file library with a libogg dependency.
