# Source and test map

This is the quickest way to find the implementation behind each section of the guide.

The production reference is the finalized [PSRadio v0.2.0 source at commit
`5c25b46`](https://github.com/blackbearreloaded/psradio/tree/5c25b4651efdcfb034431054e2b7e0309d4c88d2).
The companion [codec investigation](https://github.com/blackbearreloaded/psradio/blob/5c25b4651efdcfb034431054e2b7e0309d4c88d2/docs/CODEC_INVESTIGATION.md)
and [audio release validation](https://github.com/blackbearreloaded/psradio/blob/5c25b4651efdcfb034431054e2b7e0309d4c88d2/docs/AUDIO_RELEASE_VALIDATION.md)
record the final format decisions, hardening matrix, release checks, and
firmware 6.02 probes. The earlier
[`cba75ee`](https://github.com/blackbearreloaded/psradio/tree/cba75ee) commit
remains the production CELT-routing milestone.

## Host-side format tooling

- `workspace/dev/ps5-at9-converter/README.md` — Windows `snd0.at9` converter and fixed profile.
- `workspace/dev/ps5-at9-converter/docs/atrac9-profile.md` — container layout and size calculations.

## Native C/C++ examples

| Example | What it demonstrates |
|---|---|
| `examples/native-audio-poc/include/native_audio.hpp` | Boilerplate-aligned bounded target-file reads and RAII stereo AudioOut sink |
| `examples/native-audio-poc/aac/src/main.cpp` | Native C++20 ADTS framing, `libSceAudiodec` codec 3, and signed-16 AudioOut output |
| `examples/native-audio-poc/mp3/src/main.cpp` | Native C++20 MP3 framing, `libSceAudiodec` codec 2, and signed-16 AudioOut output |
| `examples/native-audio-poc/opus/src/main.cpp` | Native C++20 codec-21 `libSceOpusDec` lifecycle and signed-16 AudioOut output |
| `examples/native-audio-poc/microphone/src/main.cpp` | Native C++20 user-routed `libSceAudioIn` capture, WAV output, and per-block silence diagnostics |
| `workspace/dev/ps5-native-app-boilerplate/src/main.cpp` | Canonical C++20 app entry point, ownership, and no-exception/no-RTTI build contract |
| `workspace/dev/psradio/src/radio_service.c` | HTTP AAC/ADTS stream, native `sceAudiodec`, mono/stereo adaptation, linear resampling, AudioOut blocks, decoder recreation for HE-AAC behavior |
| `workspace/dev/ps5-radio-browser/src/radio_service.c` | Production AAC codec 3, MP3 codec 2, bounded format dispatch, cancellable HTTP, two-second PCM ring, stable-playback retry renewal, shared AudioOut, and codec recovery |
| `workspace/dev/ps5-radio-browser/src/opus_decoder.c` | General `libSceOpusDec` codec 21 and CELT-only `libSceOpusCeltDec` codec 16 lifecycle wrapper |
| `workspace/dev/ps5-radio-browser/src/vorbis_decoder.c` | Bounded `stb_vorbis` push-data CPU adapter and signed-16 output conversion |
| `workspace/dev/ps5-radio-browser/vendor/stb/stb_vorbis.c` | Pinned third-party Vorbis decoder; SHA-256 `4C7CB2FF1F7011E9D67950446B7EB9CA044F2E464D76BFFB0B84DD2E23E65636` |
| `workspace/dev/ps5-radio-browser/src/flac_decoder.c` | Bounded `dr_flac` callback CPU adapter for native FLAC and Ogg-FLAC |
| `workspace/dev/ps5-radio-browser/vendor/dr_flac/dr_flac.h` | Pinned third-party FLAC decoder; SHA-256 `D947F54784467160D30DCA540542BF92CED94965703E5DEEB9E82DB2EC5E0C02` |
| `workspace/dev/ps5-radio-browser/include/opus_decoder.h`, `vorbis_decoder.h`, `flac_decoder.h` | Decoder state and packet/PCM interfaces |
| `workspace/dev/ps5-radio-browser/vendor/ps5/sdk/stubs/libSceOpusCeltDec_stub.a` | Generated six-symbol import metadata only; SHA-256 is `AFCEAAD3A442CC87412FC96E30716082081AEC846AA1697608910B9FD1F3F51D` |
| `workspace/dev/ps5-radio-browser/src/radio_playlist.c` | Bounded M3U/PLS detection, URL resolution, and HLS separation |
| `workspace/dev/ps5-radio-browser/src/radio_hls.c` | Bounded HLS master/media parsing, live sequence tracking, URL resolution, and explicit unsupported-feature rejection |
| `workspace/dev/ps5-radio-browser/src/radio_ts_aac.c` | Audio-only MPEG-TS PAT/PMT/PES parsing and ADTS extraction; preserves PES state when a repeated PMT keeps the same AAC PID |
| `workspace/dev/ps5-radio-browser/docs/CODEC_INVESTIGATION.md` | Final FLAC, Vorbis, HLS, HE-AAC, retry, and codec roadmap decisions |
| `workspace/dev/ps5-radio-browser/docs/AUDIO_RELEASE_VALIDATION.md` | Final PSRadio v0.2.0 release, fault-injected matrix, and device smoke validation |
| `workspace/dev/ps5-radio-browser/docs/VORBIS_VALIDATION.md` | Pinned `stb_vorbis` provenance, bounds, host checks, and PS5 validation |
| `workspace/dev/ps5-radio-browser/docs/FLAC_VALIDATION.md` | Pinned `dr_flac` provenance, bounds, host checks, and native/Ogg-FLAC validation |
| `workspace/dev/ps5-radio-browser/docs/HLS_VALIDATION.md` | HLS/AAC subset, MPEG-TS regression, HE-AAC SBR/PS boundary, and device evidence |
| `workspace/dev/ps5-moonlight-client/src/radio_service.c` | Reusable native AAC radio service in the Moonlight application |
| `workspace/dev/ps5-moonlight-client/src/moonlight_stream.c` | Native Moonlight AudioOut lifecycle and 256-frame stereo output |
| `workspace/dev/ps5-iptv-client/src/iptv_hls.cpp` | Bounded HLS master/media parsing, URL resolution, variant selection, and discontinuities |
| `workspace/dev/ps5-iptv-client/include/iptv_hls.h` | HLS limits and explicit rejection codes for encryption, byte ranges, fMP4, and unsupported codecs |
| `workspace/dev/ps5-iptv-client/src/iptv_stream.cpp` | Companion MPEG-TS PAT/PMT/PES handling, ADTS extraction, live reload, and native AAC stream path |
## Tests

The finalized PSRadio host checks are:

- `workspace/dev/ps5-radio-browser/tools/aac_timing_check.c`
- `workspace/dev/ps5-radio-browser/tools/mp3_header_check.c`
- `workspace/dev/ps5-radio-browser/tools/pcm_queue_check.c`
- `workspace/dev/ps5-radio-browser/tools/playback_retry_check.c`
- `workspace/dev/ps5-radio-browser/tools/radio_input_check.c`
- `workspace/dev/ps5-radio-browser/tools/radio_playlist_check.c`
- `workspace/dev/ps5-radio-browser/tools/radio_hls_check.c`
- `workspace/dev/ps5-radio-browser/tools/radio_ts_aac_check.c`
- `workspace/dev/ps5-radio-browser/tools/ogg_opus_check.c`
- `workspace/dev/ps5-radio-browser/tools/vorbis_decoder_check.c`
- `workspace/dev/ps5-radio-browser/tools/flac_decoder_check.c`
- `workspace/dev/ps5-radio-browser/tools/radio_service_json_check.c`
- `workspace/dev/ps5-radio-browser/tools/radio_text_check.cpp`

The retry, Vorbis, and FLAC checks also pass under ASan/UBSan. These checks
establish deterministic host behavior; the device matrix in
[`VALIDATION.md`](VALIDATION.md) establishes runtime decoder and AudioOut
behavior.

The canonical native source overlays are under `examples/native-audio-poc`.
They keep the platform ABI visible and use the same ownership boundaries as
the boilerplate. AAC, MP3, Opus, and controller microphone capture are the
complete runnable example set in this repository.

Runtime execution and evidence capture are specified in [VALIDATION.md](VALIDATION.md).

## Static evidence

The function-level summary is in [STATIC-EVIDENCE.md](../STATIC-EVIDENCE.md).
No proprietary binaries or analysis databases are included. The inspected
AvPlayer and AJM call chains provide static evidence;
the production PSRadio source and firmware 6.02 probe record provide the
runtime corroboration.
