# Audio examples

The canonical app-facing examples are native C++20 source overlays for a
project created from
[`ps5-native-app-boilerplate`](https://github.com/blackbearreloaded/ps5-native-app-boilerplate).
They use direct PS5 imports, bounded `std::array`/`std::span` buffers, explicit
RAII cleanup, and no exceptions or RTTI in the payload.

## Native AAC

Use [the native AAC probe](native-audio-poc/README.md). It reads a local ADTS
file, calls `libSceAudiodec` codec type `3`, and sends signed-16 PCM to
`AudioOut`. The complete source is
[`native-audio-poc/aac/src/main.cpp`](native-audio-poc/aac/src/main.cpp).

## Native Opus

Use [the native Opus probe](native-audio-poc/README.md). It reads one raw Opus
packet, calls the general `libSceOpusDec` codec-21 decoder, and sends signed-16
PCM to `AudioOut`. The production Ogg framing, TOC dispatch, and codec-16 CELT
retry remain in PSRadio; the standalone source is intentionally only a small
ABI probe:
[`native-audio-poc/opus/src/main.cpp`](native-audio-poc/opus/src/main.cpp).

## Native MP3

Use [the native MP3 probe](native-audio-poc/README.md). It frames a bounded
48 kHz MP3 file, calls `libSceAudiodec` codec type `2`, and sends mono/stereo
signed-16 PCM to `AudioOut`. The complete source is
[`native-audio-poc/mp3/src/main.cpp`](native-audio-poc/mp3/src/main.cpp).

## Native controller microphone

Use [the native controller-microphone probe](native-audio-poc/README.md). It
captures ten seconds from the initial user's `libSceAudioIn` route as bounded
16 kHz signed-16 mono PCM, writes a standard WAV file under `/download0`, and
records the open result, per-block silence state, and peak amplitude. The
controller must be powered on and its orange microphone-mute light must be off.
The complete source is
[`native-audio-poc/microphone/src/main.cpp`](native-audio-poc/microphone/src/main.cpp).

## Native production path

For a complete network player, start with
`workspace/dev/ps5-radio-browser/src/radio_service.c` and its C++20 app entry
point. It already combines cancellable HTTP, bounded buffering, ADTS/Ogg/HLS
framing, native AAC/MP3/Opus, bounded CPU Vorbis/FLAC, channel normalization,
resampling, and AudioOut ownership.

All examples in this repository use native C++20 and follow
[`docs/NATIVE-C.md`](../docs/NATIVE-C.md). Host-side scripting may orchestrate
the boilerplate build, but the payload remains C++20.
