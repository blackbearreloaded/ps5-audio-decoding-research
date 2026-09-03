# PS5 Audio Decoding Research

[![License: GPL-3.0-or-later](https://img.shields.io/badge/license-GPL--3.0--or--later-blue.svg)](LICENSE)

Technical and proof-of-concept research into PS5 compressed-audio
decoding, platform audio services, AJM offload, PCM output, and application
integration.

The repository covers the validated AAC, MP3, Opus, Vorbis, FLAC, and HLS paths
used by the production PSRadio implementation. It documents the difference
between CPU software decoding and the platform's hardware/firmware audio-offload
path, and includes direct decoder and controller-microphone probes.

## Project status

| Area | Status |
| --- | --- |
| AAC platform decoder | Runtime-proven in production PSRadio through `libSceAudiodec` codec 3 / AJM; ADTS PCM reached AudioOut |
| AAC CPU fallback | Static AvPlayer path confirmed through `sceAudiodecCpuInternal*` |
| AAC ELF proof of concept | Source-complete and statically validated; production native path independently runtime-proven |
| MP3 | Runtime-proven through `libSceAudiodec` codec 2 / AJM at 48 kHz stereo; 4,608 signed-16 PCM bytes reached AudioOut |
| Opus | Runtime-proven through `libSceOpusDec` codec 21 and `libSceOpusCeltDec` codec 16; production routing is crash-free |
| Opus ELF proof of concept | Source-complete, host-built, and target-validated with a 20 ms packet |
| MP4/M4A | Local AvPlayer wrapper exists; AddSource probe was not a valid codec discriminator on firmware 6.02 |
| Ogg Vorbis | No callable native/AJM/AvPlayer PCM route found; bounded `stb_vorbis` CPU decoding is implemented and PS5-validated |
| FLAC | No usable native route on firmware 6.02; bounded `dr_flac` CPU decoding supports native FLAC and Ogg-FLAC and is PS5-validated |
| HLS | Bounded unencrypted audio-only MPEG-TS/ADTS AAC-LC slice is implemented and reuses native AAC |
| PCM output | `libSceAudioOut` wrapper and working native application examples |
| Controller microphone input | Runtime-proven through `libSceAudioIn`; VoiceChat, General, and VoiceRecognition produced valid 16 kHz mono PCM in two Chiaki-free runs |
| Advanced audio | Raw bindings for AJM, AudioOut2, Audio3d, NGS2, AAC encoding, and ATRAC9 encoding |

Firmware interfaces, title capabilities, and ABI details can change. Treat
target-console runtime evidence and static library evidence as separate claims.

The final PSRadio v0.2.0 release adds fault-injected reconnect, malformed Ogg,
rapid switching, ICY metadata, and live HLS discontinuity evidence. Native
HE-AAC SBR reconstruction is confirmed, but the public two-channel Parametric
Stereo path was not established on firmware 6.02; PSRadio retains the
timing-safe mono AAC-core fallback for PS-intended HLS. See
[Audio release validation](docs/AUDIO_RELEASE_VALIDATION.md) for the complete
release record.

## Research highlights

| Finding | Result |
| --- | --- |
| AAC decoder selection | AvPlayer contains separate `AudioDecHw(AAC)` and `AudioDecSw(AAC)` branches |
| Hardware/offload boundary | `libSceAudiodec` reaches `libSceAjm`, `/dev/ajm`, ioctl, and batch jobs |
| Hardware decode calls | Hardware AAC reaches `sceAudiodecInitialize`, `sceAudiodecRegisterCodec`, `sceAudiodecCreateDecoderEx`, and `sceAudiodecDecodeEx` |
| CPU decode calls | Software AAC reaches `sceAudiodecCpuInternalQueryMemSize`, `InitDecoder`, `ClearContext`, and `Decode` |
| MP3 | Production native path uses `libSceAudiodec` codec 2 / AJM and produced 4,608 signed-16 PCM bytes at 48 kHz stereo |
| Opus | `libSceOpusDec` uses AJM codec 21; `libSceOpusCeltDec` uses codec 16 for TOC configurations 16-31 |
| Opus recovery | General decode is attempted first; an intermittent native `-502` retries once through the CELT-only path |
| Buffering/recovery | Bounded two-second PCM ring, ~1 s initial prime, ~0.5 s underrun re-prime, cancellable HTTP, and stable-playback retry-budget renewal |
| FLAC | `libSceAudiodecCpu.sprx` references internal module `0x80000053`, but the runtime probe could not load it |
| Vorbis | Static AvPlayer/AJM review found no callable native PCM route; AvPlayer source probes cannot prove absence |
| Container semantics | MP4 is a container; AAC, Opus, AC-3, or another track codec determines the route |
| Application path | Production PSRadio demonstrates AAC/MP3/Opus/Vorbis/FLAC/HLS-to-PCM-to-AudioOut streaming |
| Controller microphone | A powered-on, unmuted DualSense is captured through the normal user-routed `libSceAudioIn` path; direct HID/Bluetooth PCM access is unnecessary |

The safest technical term is **hardware/firmware audio offload**. Static and
runtime evidence confirms submission to the AJM audio device; it does not
identify the exact physical DSP or fixed-function block used for every codec.

## Proven data path

```text
Local file or network stream
        |
        +--> libSceAvPlayer
        |       demux + decoder selection + decoded audio frames
        |
        +--> libSceAudiodec
                codec registration + frame decode
                        |
                        +--> libSceAjm / libSceAjmi
                                /dev/ajm + ioctl + batch jobs
        |
        v
Application-owned PCM
        |
        +--> AudioClip / AudioMixer / NGS2 / Audio3d / AudioOut2
        |
        v
libSceAudioOut -> speakers / BGM / voice / personal / pad routes
```

## Capability matrix

| Codec or feature | Evidence | Practical route |
| --- | --- | --- |
| AAC-LC ADTS | Runtime-proven in production through codec 3; ADTS frames became signed-16 PCM and reached AudioOut | Native `sceAudiodec*` or `AudioDecoder.CreateAac` |
| AAC in M4A/MP4 | Container path available through AvPlayer | `MediaPlayer.Open` |
| HE-AAC | SBR reconstruction runtime-proven at 48 kHz mono; valid public PS configurations expose one channel, not native PS stereo | Keep native AAC; use the timing-safe AAC-core fallback and normalize/duplicate mono to the 48 kHz stereo contract |
| MP3 | Runtime-proven through `libSceAudiodec` codec 2 / AJM; live 48 kHz stereo produced 4,608 signed-16 PCM bytes and reached AudioOut | Native `sceAudiodec*` or `AudioDecoder.CreateMp3` |
| Opus | Runtime-proven general codec 21 and CELT codec 16; TOC 16-31 selects CELT after a native `-502` retry | General `libSceOpusDec` first; `libSceOpusCeltDec` recovery path |
| Ogg Vorbis | No callable native/AJM/AvPlayer PCM route found; AvPlayer probes failed before codec selection | Bounded [`stb_vorbis`](https://github.com/nothings/stb/blob/master/stb_vorbis.c) push-data CPU decoder |
| FLAC | Internal CPU plug-in `0x80000053` is absent/unloadable; no AJM/AvPlayer FLAC route found | Bounded [`dr_flac`](https://github.com/mackron/dr_libs/blob/master/dr_flac.h) callback CPU decoder |
| HLS | Delivery protocol, not a codec; bounded playlist and MPEG-TS primitives are implemented | Unencrypted audio-only MPEG-TS/AAC-LC -> native AAC |
| AC-3/E-AC-3 | AvPlayer hardware/software branches visible | Use AvPlayer; no standalone public recipe yet |
| ATRAC9 | Encoder binding and host-side asset converter available | `At9Enc` or `ps5-at9-converter`; runtime decoder path remains open |
| PCM output | High-level and raw AudioOut paths | `AudioOutDevice` for stereo signed-16 blocks |
| Spatial/object output | AudioOut2 and Audio3d raw bindings | Use only when application-owned spatial routing is required |
| DualSense microphone | Runtime-proven through user-routed AudioIn purposes `0`, `1`, and `5` | Use `libSceAudioIn` on a capture worker; keep the controller powered and the orange mute light off |

## Quick start

Clone the repository:

```sh
git clone git@github.com:blackbearreloaded/ps5-audio-decoding-research.git
cd ps5-audio-decoding-research
```

The canonical app-facing examples are native C++20 overlays for
[`ps5-native-app-boilerplate`](https://github.com/blackbearreloaded/ps5-native-app-boilerplate).
Start from a fresh boilerplate project, copy the selected source and shared
header, and build in WSL:

```bash
mkdir -p <boilerplate>/include
cp examples/native-audio-poc/include/native_audio.hpp <boilerplate>/include/
cp examples/native-audio-poc/aac/src/main.cpp <boilerplate>/src/main.cpp
cd <boilerplate>
make APP_INCLUDE_PATHS=include
```

Create a compatible ADTS test file:

```powershell
ffmpeg -i input.wav -ar 48000 -ac 2 -c:a aac -f adts aac-hw-poc.aac
```

Copy it to `/data/aac-hw-poc.aac` on the target. The native probe loads
`AudioDec` (`0x0088`), decodes AAC-LC through `libSceAudiodec` codec type `3`,
and sends signed-16 PCM to a 48 kHz stereo AudioOut port.

For the direct Opus source, copy the `native-audio-poc/opus/src/main.cpp`
variant instead and place one raw packet at `/data/opus-hw-poc.packet`. The
standalone source exercises the general codec-21 decoder; the production Ogg
framing, TOC dispatch, and codec-16 CELT retry are in PSRadio.

For native MP3, copy `native-audio-poc/mp3/src/main.cpp` and place a 48 kHz
mono or stereo file at `/data/mp3-hw-poc.mp3`. All runnable examples in this
repository are C++20 source overlays for the boilerplate application.

For controller microphone capture, copy
`native-audio-poc/microphone/src/main.cpp`, link the AudioIn import, and launch
without Remote Play when validating a controller connected locally to the
console. See [Audio input](docs/AUDIO-INPUT.md) for mute, routing, silence-state,
and privacy requirements.

Load a native payload or complete title with the workflow appropriate to the
boilerplate and target loader. See [Native C++ integration](docs/NATIVE-C.md).

## Documentation

| Document | Purpose |
| --- | --- |
| [Architecture](docs/ARCHITECTURE.md) | Data flow, ownership, threading, timing, and hardware wording |
| [API matrix](docs/API-MATRIX.md) | Library/module/wrapper maturity and codec coverage |
| [Compressed decoding](docs/DECODING.md) | Native AAC/MP3/Opus APIs, bounded Vorbis/FLAC fallbacks, ADTS/Ogg streaming, buffering, and reset behavior |
| [AvPlayer](docs/MEDIA.md) | Local M4A/MP4/MOV/WebM playback and decoded audio frames |
| [Audio output](docs/AUDIO-OUTPUT.md) | AudioOut, AudioOut2, Audio3d, formats, grains, and queue semantics |
| [Audio input](docs/AUDIO-INPUT.md) | Microphone capture and silent-state handling |
| [Advanced audio](docs/ADVANCED.md) | Mixing, DSP, NGS2, spatial audio, AAC, and ATRAC9 encoding |
| [AJM](docs/AJM.md) | Device-backed audio jobs, codec registration, and open research |
| [Native C++ integration](docs/NATIVE-C.md) | Boilerplate-aligned application patterns, build, and deployment |
| [Controller integration](docs/CONTROLLER-INTEGRATION.md) | Integrating working joystick input with audio threads |
| [Validation checklist](docs/VALIDATION.md) | Target-console test matrix and expected evidence |
| [Audio release validation](docs/AUDIO_RELEASE_VALIDATION.md) | Final PSRadio v0.2.0 decoder, hardening, fault-matrix, and release evidence |
| [Vorbis validation](docs/VORBIS_VALIDATION.md) | Native-route boundary, pinned `stb_vorbis`, and device evidence |
| [FLAC validation](docs/FLAC_VALIDATION.md) | Native-route boundary, pinned `dr_flac`, and device evidence |
| [HLS validation](docs/HLS_VALIDATION.md) | Bounded HLS/AAC-LC/MPEG-TS implementation and explicit exclusions |
| [Source map](docs/SOURCE-MAP.md) | Companion workspace implementations and tests |
| [Examples](examples/README.md) | Short recipes and links to complete examples |
| [Static evidence](STATIC-EVIDENCE.md) | Function addresses, strings, and recovered call chains |
| [Codec roadmap](FOLLOW-UP.md) | Final codec decisions, HLS scope, fallback candidates, and next gates |

## Repository layout

```text
README.md                         Research overview and headline results
INVESTIGATION.md                  Original codec/offload investigation
STATIC-EVIDENCE.md                Static addresses, symbols, and call chains
FOLLOW-UP.md                      Open questions and next experiments
docs/ARCHITECTURE.md              Data flow and ownership rules
docs/API-MATRIX.md                Library and codec capability matrix
docs/DECODING.md                  Native/fallback codec APIs and streaming guide
docs/MEDIA.md                     AvPlayer container playback
docs/AUDIO-OUTPUT.md              AudioOut, AudioOut2, and Audio3d
docs/AUDIO-INPUT.md               Microphone capture
docs/ADVANCED.md                  Mixing, DSP, NGS2, spatial audio, encoders
docs/AJM.md                       Low-level job manager notes
docs/NATIVE-C.md                  Native C++ integration and deployment
docs/CONTROLLER-INTEGRATION.md    Controller/audio architecture
docs/VALIDATION.md                Runtime test checklist
docs/AUDIO_RELEASE_VALIDATION.md  Final PSRadio v0.2.0 release evidence
docs/VORBIS_VALIDATION.md         Vorbis native boundary and CPU fallback
docs/FLAC_VALIDATION.md            FLAC native boundary and CPU fallback
docs/HLS_VALIDATION.md             HLS/AAC-LC/MPEG-TS scope and evidence
docs/SOURCE-MAP.md                Workspace implementations and tests
examples/native-audio-poc/        Boilerplate-aligned native C++ source overlays
examples/README.md                Short runnable recipes
LICENSE                            GPL-3.0-or-later
```

## Methodology and evidence labels

Every conclusion is kept within the evidence available:

- **Runtime validated:** observed on a target console and recorded in a log or test result.
- **Static call-chain confirmed:** recovered through static library inspection with meaningful calls,
  branches, or device operations.
- **Binding available:** a local SDK wrapper or native declaration exists, but runtime behavior may still
  need a payload test.
- **Export/symbol only:** a library or symbol is present, but the application workflow is not established.
- **Open question:** plausible, but intentionally not presented as a conclusion.

The final PSRadio codec investigation and release validation are recorded in
the companion [`psradio` commit `5c25b46`](https://github.com/blackbearreloaded/psradio/tree/5c25b4651efdcfb034431054e2b7e0309d4c88d2),
especially its [`docs/CODEC_INVESTIGATION.md`](https://github.com/blackbearreloaded/psradio/blob/5c25b4651efdcfb034431054e2b7e0309d4c88d2/docs/CODEC_INVESTIGATION.md)
and [`docs/AUDIO_RELEASE_VALIDATION.md`](https://github.com/blackbearreloaded/psradio/blob/5c25b4651efdcfb034431054e2b7e0309d4c88d2/docs/AUDIO_RELEASE_VALIDATION.md).
The earlier [`cba75ee`](https://github.com/blackbearreloaded/psradio/tree/cba75ee)
production CELT-routing milestone and [`84771ef`](https://github.com/blackbearreloaded/psradio/tree/84771ef)
investigation milestone remain useful historical checkpoints. The device
evidence summarized here was collected on firmware 6.02.

No proprietary binaries or analysis database files are included. Functional
observations are summarized in [STATIC-EVIDENCE.md](STATIC-EVIDENCE.md)
without image addresses, private import identifiers, or numeric kernel request
values.

## Companion workspace projects

The research was developed alongside these applications and SDK components:

- `workspace/dev/ps5-native-app-boilerplate` — canonical native C/C++20 app
  build and deployment template.
- `workspace/dev/psradio` — native AAC radio, AudioOut, and controller implementation.
- `workspace/dev/ps5-radio-browser` — RmlUi radio application with the same audio path.
- `workspace/dev/ps5-moonlight-client` — Moonlight audio output and CPU Opus baseline.
- `workspace/dev/ps5-at9-converter` — Windows ATRAC9 asset converter.

The two related Codex tasks are [Zero-Copy-GPU-Decoding](codex://threads/01a0272d-fb7e-7981-89fa-1f50a3f5d9df)
and [Radio-App-RmlUi](codex://threads/01a02cc0-e8bc-7b91-9b6a-ecc20035b294).

## Scope and limitations

This repository publishes independently written documentation, examples, and
technical research notes. It does not contain Sony SDK files, firmware
modules, retail application binaries, signing material, credentials, or
proprietary test assets. It does not claim compatibility beyond the tested
platform interfaces, firmware baseline, and runtime experiments explicitly
recorded here.

The repository's mandatory content boundaries and pending independent-review
checklist are documented in [PUBLICATION-POLICY.md](PUBLICATION-POLICY.md).
This project is not represented as legally cleared by counsel.

PlayStation and PS5 are trademarks of Sony Interactive Entertainment. This
project is independent and is not affiliated with or endorsed by Sony.

## License

Repository-authored documentation and examples are licensed under
GPL-3.0-or-later. See [LICENSE](LICENSE).
