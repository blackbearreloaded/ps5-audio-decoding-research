# Microphone input

`libSceAudioIn` mirrors the block-oriented output model. Open a port for a signed-in user, then pull one
block at a time. Each input call blocks until the block is captured.

## Native C++20 example

```cpp
#include <array>
#include <cstdint>
#include <span>

extern "C" {
int sceAudioInOpen(int user_id, int type, int index,
                   std::uint32_t grain, std::uint32_t frequency,
                   std::uint32_t format);
int sceAudioInInput(int handle, void *destination);
int sceAudioInGetSilentState(int handle);
int sceAudioInClose(int handle);
}

std::array<std::int16_t, 256> block{};
const int handle = sceAudioInOpen(initial_user_id, 0, 0, 256, 16000, 1);
if (handle < 0)
{
    return 1;
}

while (recording())
{
    if (sceAudioInInput(handle, block.data()) < 0)
    {
        break;
    }
    process_microphone(std::span{block}, sceAudioInGetSilentState(handle) != 0);
}
sceAudioInClose(handle);
```

The current helper supports:

| Setting | Values |
|---|---|
| sample format | signed 16-bit mono or stereo |
| sample rate | 16 kHz or 48 kHz |
| grain | 128 or 256 frames |
| purpose | `General` or `VoiceChat` |
| user | signed-in user ID |

`VoiceChat` selects the system voice-processing purpose. `General` is appropriate for recording,
analysis, or an application-owned speech pipeline.

## Raw ABI

The underlying calls are:

```cpp
extern "C" {
int sceAudioInOpen(int user_id, int type, int index,
                   std::uint32_t grain, std::uint32_t frequency,
                   std::uint32_t format);
int sceAudioInInput(int handle, void *destination);
int sceAudioInGetSilentState(int handle);
int sceAudioInClose(int handle);
}
```

The observed formats include signed-16 and float mono/stereo. The example keeps
the application-facing path to bounded signed-16 mono capture.

## Design notes

- Do not read the microphone on the render/UI thread; the call is blocking.
- Record the actual `Grain` and channel count used to size buffers.
- Write a standard PCM WAV header around captured samples if the goal is a
  local recording.
- Check `IsSilent` separately from the samples; silence may be caused by hardware mute or system state.
- Keep input and output work on separate threads if implementing monitoring, because both calls can pace
  against their own device clocks.
