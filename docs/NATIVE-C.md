# Native C++ integration, build, and deployment

The app payload examples in this repository now follow the native application
contract used by
[`ps5-native-app-boilerplate`](https://github.com/blackbearreloaded/ps5-native-app-boilerplate):
native C++20 under `src/`, direct platform imports, explicit ownership, and
the WSL/Clang build pipeline. Host-side tools are not part of the native
application definition.

## Native examples

The small source overlays are:

- `examples/native-audio-poc/aac/src/main.cpp` — ADTS AAC through
  `libSceAudiodec` codec type `3` to signed-16 `AudioOut`;
- `examples/native-audio-poc/mp3/src/main.cpp` — bounded MP3 framing through
  `libSceAudiodec` codec type `2` to signed-16 `AudioOut`;
- `examples/native-audio-poc/opus/src/main.cpp` — one raw packet through
  `libSceOpusDec` codec `21` to signed-16 `AudioOut`;
- `examples/native-audio-poc/include/native_audio.hpp` — bounded target-file
  reads and an RAII stereo AudioOut sink shared by both probes.

They intentionally contain no Sony runtime code, FFmpeg, libopus, or software
AAC decoder. The native Opus probe exercises the general decoder only; the
production CELT TOC dispatch and retry path is in
`workspace/dev/ps5-radio-browser/src/opus_decoder.c`.

## Create and build a native project

Create a project from the boilerplate template, or use a local checkout. Copy
one probe's `src/main.cpp` and the shared header into the corresponding
`src/`/`include/` directories. The boilerplate build discovers C and C++ files
under `src/`; add the include directory when invoking Make:

```bash
mkdir -p include
make APP_INCLUDE_PATHS=include
```

The direct imports must resolve from the installed public PS5 payload SDK
stubs. The AAC variant needs `libSceSysmodule`, `libSceAudiodec`, and
`libSceAudioOut`; the Opus variant additionally needs `libSceOpusDec`. If a
firmware-specific import archive is generated during interface research, it
is link metadata only and must not be treated as a Sony runtime module or
packaged into the title.

The boilerplate emits the native title folder and supports its normal folder,
FSELF, packaging, and deployment workflows. A standalone `.elf` is suitable
for the existing `hbldr`/`elfldr` workflow when the loader expects a payload;
the complete native title workflow should stage the full output folder.

## ABI pattern

Keep platform declarations at the boundary and application ownership in C++:

```cpp
struct AacControl;
extern "C" int sceAudiodecDecode(int handle, AacControl *control);

class Decoder final
{
  public:
    Decoder() noexcept;
    ~Decoder() noexcept;
    Decoder(const Decoder &) = delete;
    Decoder &operator=(const Decoder &) = delete;
};
```

Use `std::array` or an explicitly capped allocation for compressed and PCM
buffers. Pass views with `std::span`; validate every native byte count before
turning it into a sample span. Destructors must destroy decoder state, terminate
the decoder library, close AudioOut, and unload only modules owned by that
object. Do not let a platform call escape through a throwing destructor.

## Production streaming pattern

For a network application, reuse the proven native PSRadio architecture rather
than putting blocking work in a render or controller callback:

```text
UI/controller thread -> command and state ownership
network thread       -> cancellable compressed-byte input
decoder thread       -> native or bounded CPU decode
AudioOut consumer    -> normalized 48 kHz stereo blocks
```

`workspace/dev/ps5-radio-browser/src/radio_service.c` demonstrates the full
path. Its decoded PCM ring is bounded to two seconds, startup primes about
one second, underrun re-primes about half a second, stop/error aborts the
active HTTP request and discards queued PCM, and decoder reset does not tear
down AudioOut. The same output sink handles native AAC/MP3/Opus and the bounded
Vorbis/FLAC CPU fallbacks.

## Example policy

Runnable payload examples in this repository are native C++20 source overlays.
Host-side scripts may drive the boilerplate toolchain, but examples must not
require a second application project format.
