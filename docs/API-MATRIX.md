# Audio library and API matrix

The table distinguishes the inspected platform inventory from the SDK wrapper. “Wrapper” means there is local callable
code; it does not mean the path has been runtime-tested on every firmware.

| Library / service | What it does | System module / load note | Local wrapper | Current status |
|---|---|---|---|---|
| `libSceAudioOut` | Fixed-port PCM output, volume, queue/drain | Usually resident; call init before opening | `AudioOut`, `AudioOutDevice` | Binding available; native output used by PS5 apps |
| `libSceAudioIn` | Microphone capture and silent-state query | Usually resident | `AudioIn`, `AudioInDevice` | Binding available |
| `libSceAudiodec` | Registered compressed decoders | `AudioDec = 0x0088` | `Audiodec`, `AudioDecoder` | AAC codec 3 and MP3 codec 2 are runtime-proven through the native/AJM path |
| `libSceAudiodecCpu` | CPU decoder service/fallback | `AudioDecCpu = 0x00BD` | No high-level CPU decoder wrapper | CPU AAC branch statically confirmed through AvPlayer |
| `libSceAudiodecCpuM4aac` | CPU AAC implementation module | Loaded by the CPU path as needed | No direct high-level wrapper | Present in the inspected inventory; direct use open |
| `libSceAjm` / `libSceAjmi` | Batched decode/encode job manager | Usually loaded/initialized by the codec path | `Ajm` raw entrypoints | Device-backed path statically confirmed; direct recipe incomplete |
| `libSceAvPlayer` | Container demux, stream selection, audio/video frame queues | `AvPlayer = 0x00A5` | `AvPlayer`, `MediaPlayer` | Local wrapper available; recovered ABI initialized, but the 6.02 AddSource probes rejected Vorbis/WebM, Opus/WebM, and AAC/M4A identically before codec selection |
| `libSceAvPlayerStreaming` | Streaming-side AvPlayer support | Separate library | No matching local high-level source path | Export/library present; not documented as a ready recipe |
| `libSceAudioOut` AudioOut2 API family | Context/port/object-based output, mastering | Called through the AudioOut2 binding in the same library | `AudioOut2` | Raw binding and docs; requires explicit context structures |
| `libSceAudio3d` | Object/bed spatial mixing and speaker coefficients | `Audio3d = 0x00A7` | `Audio3d` | Raw binding; needs a target probe for a full example |
| `libSceNgs2` | Sampler, submixer, reverb, mastering, geometry, streams | `Ngs2 = 0x000B` | `Ngs2` | Broad raw binding; use only for custom graphs |
| `libSceM4aacEnc` | AAC-LC encoding | `M4aacEnc = 0x00BC` | `M4aacEnc`, `AacEncoder` | Binding available; 48 kHz, mono/stereo encoder |
| `libSceAt9Enc` | ATRAC9 encoding | Separate encoder library | `At9Enc` raw binding | Binding available; container/config work remains application-owned |
| `libSceAudioPropagation` | Audio propagation support | No local system-module enum in the current guide | No high-level wrapper | Library present; no usable workflow established |
| `libSceAudioSystem` | System-level audio facilities | No local recipe | No high-level wrapper | Library present; scope not yet mapped |
| `libSceCustomMusicAudioOut` | Custom music output facilities | No local recipe | No high-level wrapper | Library present; scope not yet mapped |
| `libSceOpusDec` | Stateful SILK/hybrid/general Opus packet-to-PCM decoder | Runtime loader maps `libSceOpusDec.sprx` to internal module `0x80000069` | Recovered native ABI and production wrapper | Runtime-proven through AJMI/AJM codec 21 |
| `libSceOpusCeltDec` | CELT-only packet decoder | Runtime loader maps `libSceOpusCeltDec.sprx` to internal module `0x80000044` | Recovered native ABI and production fallback | Runtime-proven through AJMI/AJM codec 16; use for TOC configurations 16-31 |
| `libSceAudiodecCpuFlac` | Internal CPU FLAC plug-in referenced by the CPU decoder service | Internal sysmodule `0x80000053` | No application wrapper | Static reference only; absent from inspected firmware and a runtime load returned `0x805a1000` |
| `stb_vorbis` | Single-file Ogg Vorbis CPU decoder | Vendored source; no PS5 module | `src/vorbis_decoder.c` adapter | Implemented with bounded push-data input and PS5-validated at 44.1 kHz stereo |
| `dr_flac` | Single-file native/Ogg-FLAC CPU decoder | Vendored source; no PS5 module | `src/flac_decoder.c` adapter | Implemented with bounded callbacks and PS5-validated for native FLAC and Ogg-FLAC |

## Codec matrix

| Input / codec | Direct application route | Offload evidence | CPU evidence | Recommendation |
|---|---|---|---|---|
| AAC-LC ADTS | `AudioDecoder.CreateAac` or native `sceAudiodec*` | High: AvPlayer `AudioDecHw(AAC)` and AJM chain | High: `sceAudiodecCpuInternalDecode` branch | Use the AAC POC or `AudioDecoder` |
| AAC in MP4/M4A | `MediaPlayer` or demux then `AudioDecoder` | Follows AAC track path | Depends on AvPlayer selection | Use `MediaPlayer` for local files |
| HE-AAC | `CreateAac(highEfficiency: true)` and native control fields | A controlled probe confirmed SBR reconstruction with nonzero 48 kHz mono PCM and energy above 12 kHz; valid public PS configurations exposed one channel | CPU fallback exists | Keep native AAC plus the timing-safe 24 kHz mono AAC-core fallback for PS-intended HLS; normalize/duplicate mono to 48 kHz stereo |
| MP3 | `AudioDecoder.CreateMp3` or native codec 2 lifecycle | High: live 48 kHz stereo produced 4,608 signed-16 PCM bytes and reached AudioOut through AJM; production matrix also played/stopped MP3 | No CPU-vs-AJM comparison required for the native route | Keep the native decoder |
| Opus | General `libSceOpusDec` first; `libSceOpusCeltDec` on TOC 16-31 recovery | High: codec 21 and codec 16 both runtime-proven in production | CPU libopus remains a compatibility fallback | Keep the dual native route and recover intermittent `-502` live packets |
| Ogg Vorbis | No callable native/AJM/AvPlayer PCM route found; AvPlayer probes are not a codec discriminator | None | Bounded `stb_vorbis` push-data CPU path | Implemented and validated with live 44.1 kHz stereo playback, stop, switch, and sustained run |
| FLAC | No loadable native route on firmware 6.02; CPU plug-in `0x80000053` is absent and returned `0x805a1000` | No AJM/AvPlayer route | Bounded `dr_flac` callback CPU path | Implemented and validated for native FLAC and Ogg-FLAC |
| ATRAC9 / AT9 | Low-level encoder exists; decoder enum exists | AJM comments/API cover ATRAC9 | Not mapped here | Use VAG/AT9 tooling or continue targeted research |
| AC-3 / E-AC-3 | AvPlayer branches are visible | AC-3 hardware branch and E-AC-3 software branch visible | E-AC-3 software branch visible | Do not claim a standalone public decoder yet |
| MP4 | Container only | Depends on contained audio codec | Depends on contained audio codec | Inspect the track codec before choosing an API |

## Public versus internal APIs

`AudioDecoder` uses the public-shaped `sceAudiodecInitLibrary`, `sceAudiodecCreateDecoder`, and
`sceAudiodecDecode` family exposed in the local SDK. AvPlayer's `sceAudiodec*Ex` and
`sceAudiodecCpuInternal*` calls are recovered implementation details. The direct Opus lifecycle is
available through `libSceOpusDec` and `libSceOpusCeltDec`, but their internal module IDs and ABI remain
firmware-specific. `stb_vorbis` and `dr_flac` are application CPU decoders, not native offload paths.
None of these observations identifies a physical DSP; use “hardware/firmware audio offload” or
“AJM-backed native decoding” only for the native AAC/MP3/Opus paths.
