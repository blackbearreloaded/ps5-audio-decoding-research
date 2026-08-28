# Native C++20 audio probes

These are the canonical app-facing examples for this repository. They follow
the native application contract used by
[`ps5-native-app-boilerplate`](https://github.com/blackbearreloaded/ps5-native-app-boilerplate):
the PS5 payload is C++20, ownership is explicit, and buffers are bounded.

The examples are source overlays rather than a second copy of the boilerplate
toolchain. Start from a fresh boilerplate project, copy the selected
`src/main.cpp` and `include/native_audio.hpp`, then add the imported PS5
libraries supplied by the public payload SDK. The boilerplate build discovers
the C++ source under `src/`, compiles it with exceptions and RTTI disabled,
and emits the native title output.

## AAC

Copy:

```text
native-audio-poc/include/native_audio.hpp -> <boilerplate>/include/native_audio.hpp
native-audio-poc/aac/src/main.cpp         -> <boilerplate>/src/main.cpp
```

The probe reads `/data/aac-hw-poc.aac` as a bounded ADTS stream, loads
`0x0088`, initializes `libSceAudiodec` codec type `3`, decodes signed-16 PCM,
and writes 48 kHz mono/stereo audio as stereo `AudioOut` blocks. It uses the
same `sceAudiodec*` control layout as the production PSRadio path.

## Opus

Copy:

```text
native-audio-poc/include/native_audio.hpp -> <boilerplate>/include/native_audio.hpp
native-audio-poc/opus/src/main.cpp        -> <boilerplate>/src/main.cpp
```

Place one raw Opus packet at `/data/opus-hw-poc.packet`. The probe loads
internal module `0x80000069`, creates the general `libSceOpusDec` codec-21
decoder, validates signed-16 PCM, and sends it to `AudioOut`. It intentionally
does not parse Ogg or implement the production CELT retry; use PSRadio for the
full incremental Ogg/TOC dispatch path.

## MP3

Copy:

```text
native-audio-poc/include/native_audio.hpp -> <boilerplate>/include/native_audio.hpp
native-audio-poc/mp3/src/main.cpp         -> <boilerplate>/src/main.cpp
```

Place a 48 kHz mono or stereo MP3 at `/data/mp3-hw-poc.mp3`. The probe parses
MPEG Layer III frame headers with fixed bounds, loads `libSceAudiodec` codec
type `2`, validates the returned signed-16 byte count, and sends PCM to
`AudioOut`. Other source rates require resampling before the fixed 48 kHz port.

## Build and link

From the boilerplate repository in WSL:

```bash
mkdir -p include
make APP_INCLUDE_PATHS=include
```

The direct imports must be available from the installed public PS5 payload SDK
stubs, including `libSceSysmodule`, `libSceAudioOut`, `libSceAudiodec`, and
the Opus decoder library for the Opus variant. Import archives contain symbol
metadata only; do not package Sony runtime modules in the application. The
generated `.elf`/FSELF is launched with the same `hbldr`, `elfldr`, or native
title workflow documented by the boilerplate.

The AAC, MP3, and Opus overlays are the complete runnable example set for this
repository.

## Host regression check

The shared output helper has a small platform-independent regression check:

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude tests/native_audio_check.cpp -o /tmp/native_audio_check
/tmp/native_audio_check
```
