# PCM output: AudioOut and AudioOut2

## `AudioOut`: the default choice

The shared C++20 `AudioOut` helper wraps a main `libSceAudioOut` port configured
for stereo signed-16 PCM and owns the handle with RAII.

```cpp
#include "native_audio.hpp"

std::array<std::int16_t, ps5::native_audio::kAudioOutGrain * 2> block{};
fill_interleaved_stereo(block);

ps5::native_audio::AudioOut audio;
if (!audio.open() || !audio.push(block, 2) || !audio.flush())
{
    return 1;
}
```

### Accepted configuration

| Parameter | Current C++20 helper |
|---|---|
| `grain` | Whole multiple of 256 from 256 through 2048 frames per channel |
| `sampleRate` | 48000 or 192000 Hz |
| format | Signed 16-bit stereo, interleaved |
| output block | `grain * 2` shorts: left, right, left, right, ... |
| user | Defaults to `SceUser.System`; pass a user ID when needed |

The raw enum also exposes mono, eight-channel, and float formats. Those are available through
`AudioOut.sceAudioOutOpen`, but the high-level helper intentionally keeps the common stereo path small.

The native form uses:

```cpp
extern "C" {
int sceAudioOutInit();
int sceAudioOutOpen(int user_id, int type, int index, std::uint32_t grain,
                    std::uint32_t frequency, std::uint32_t format);
int sceAudioOutSetVolume(int handle, int flags, const int *volumes);
int sceAudioOutOutput(int handle, const void *samples);
int sceAudioOutClose(int handle);
}

sceAudioOutInit();
const int handle = sceAudioOutOpen(user_id, type, index, grain, frequency, format);
sceAudioOutSetVolume(handle, flags, volumes);
sceAudioOutOutput(handle, samples);
sceAudioOutOutput(handle, nullptr); // drain
sceAudioOutClose(handle);
```

The radio and Moonlight examples use a 256-frame stereo signed-16 block and an 0 dB volume array.

### Queue semantics

`Output` submits one complete block. The call blocks until the output queue has room, so it naturally
paces an audio producer. It does not accept a short final block; hold the tail and zero-fill it before
the final output. A null/native `Output` or `Drain` waits for queued audio to finish before closing.

Because output is blocking, run the audio loop on its own thread. The controller and UI threads should
send commands such as play/pause/volume through a small synchronized state or queue.

### Volume

Use volume values between zero and 0 dB (`0x8000`). The raw call takes channel
flags and an array large enough for the service's channel indexing. The C++20
helper supplies eight entries and selects left and right.

## Pairing decoded audio with output

Decoded frames do not usually line up with the output grain. The shared helper
already accumulates mono or stereo samples, duplicates mono, emits complete
blocks, and zero-fills only the final tail:

```cpp
bool consume_pcm(ps5::native_audio::AudioOut &audio,
                 std::span<const std::int16_t> samples,
                 std::size_t channels) noexcept
{
    return audio.push(samples, channels);
}
```

For mono input, duplicate each sample into the left and right positions before calling this helper. For
other rates, resample to the output rate first; do not rely on the port to change pitch or sample count.

## Raw `AudioOut2`

`AudioOut2` is a separate context/port API, not a convenience wrapper over `AudioOutDevice`. It supports:

- main, BGM, voice, personal, auxiliary, controller-speaker, and vibration ports;
- mono, stereo, eight-channel, and floating-point data formats;
- object ports with position, spread, gain, priority, passthrough, and ambisonic attributes;
- explicit context queue depth and grain count;
- synchronous or asynchronous push;
- speaker information and mastering controls.

The normal sequence is:

```text
sceAudioOut2Initialize
    -> ContextResetParam
    -> ContextQueryMemory
    -> ContextCreate
    -> PortCreate
    -> PortSetAttributes(Pcm/Gain/Position/...)
    -> ContextAdvance
    -> ContextPush
    -> PortDestroy
    -> ContextDestroy
```

The current guide records the lifecycle and discovered entrypoints, but does
not provide a target-validated C++20 `AudioOut2` wrapper. Add one only after a
specific attribute and object-port setup has device evidence.

## Audio3d output

`Audio3d` exposes another object/bed-oriented path:

```text
sceAudio3dInitialize
    -> sceAudio3dPortOpen / PortCreate
    -> ObjectReserve / ObjectSetAttributes or BedWrite
    -> PortAdvance
    -> PortPush
    -> PortClose / PortDestroy
    -> sceAudio3dTerminate
```

It also provides speaker-array memory sizing, speaker-array creation, and position-to-speaker
coefficient queries. Use this when the application owns a scene or wants the platform to position sound
objects. For an ordinary radio stream, it is unnecessary overhead.
