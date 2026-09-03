# Native C++20 audio probes

These are the canonical app-facing examples for this repository. They follow
the native application contract used by
[`ps5-native-app-boilerplate`](https://github.com/blackbearreloaded/ps5-native-app-boilerplate):
the PS5 payload is C++20, ownership is explicit, and buffers are bounded.

The examples are source overlays rather than a second copy of the boilerplate
toolchain. Start from a fresh boilerplate project, copy the selected
`src/main.cpp` and, where used, `include/native_audio.hpp`, then add the imported PS5
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

The AAC, MP3, Opus, and controller-microphone overlays are the complete
runnable example set for this repository.

## Controller microphone

Copy `microphone/src/main.cpp` to the boilerplate `src/main.cpp`. The probe
opens the initial user's General `AudioIn` route as 16 kHz signed-16 mono,
captures ten seconds, and writes these persistent files:

```text
/download0/dualsense-microphone.wav
/download0/dualsense-microphone.log
```

Power on the controller before launch. The orange microphone-button light
means muted, so it must be off; no separate recording light appeared in the
validated captures. Speak when prompted, close the title cleanly, and retrieve
both persistent files with UFS2Tool. The probe uses the user-routed input; if
another microphone is selected for that user, the system may route that device
instead of the controller microphone.

Validate a locally connected controller without Chiaki-ng or another Remote
Play client, because Remote Play can change controller ownership and audio
routing. Two direct firmware-6.02 runs produced valid PCM on VoiceChat (`0`),
General (`1`), and VoiceRecognition (`5`). A transient nonzero silence mask can
occur while the route activates or changes; inspect the samples and per-block
state rather than rejecting the whole recording. The tested Game-category
title needed no additional microphone package declaration or system consent
prompt, but production applications still need explicit recording UI and an
appropriate privacy/consent design.

### Bring the WAV to a development PC

Wait for the completion notification or close the title cleanly so it is no
longer writing the file. Under the shared console lock, retrieve the title's
download-data image from:

```text
/user/download/<TITLE_ID>/download0.dat
```

If the local development protocol provides `Get-Ps5DownloadData.ps1`, use that
lock-aware helper rather than issuing an unlocked FTP request. Then extract the
single recording with [UFS2Tool](https://github.com/SvenGDK/UFS2Tool):

```powershell
ufs2tool extract download0.dat .\recording /dualsense-microphone.wav
ffplay -autoexit .\recording\dualsense-microphone.wav
```

The extracted file is an ordinary PCM WAV and can also be opened in a desktop
media player or audio editor. Keep recordings and `download0.dat` outside Git;
they may contain personal audio and private runtime data.

Some public SDK snapshots omit a `libSceAudioIn` import stub. In that case,
build `microphone/link-stubs/audio_in_link_stub.cpp` as a temporary shared
provider with SONAME `libSceAudioIn.prx`, include it in the native link and
module-conversion inputs, and do not copy it into the application. Its bodies
exist only to provide link-time symbols; the final executable imports the
resident system library.

## Host regression check

The shared output helper has a small platform-independent regression check:

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude tests/native_audio_check.cpp -o /tmp/native_audio_check
/tmp/native_audio_check
```
