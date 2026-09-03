# Controller microphone input

`libSceAudioIn` is the application-facing raw PCM capture API. A title opens
an input route for a signed-in user and pulls one fixed-size block at a time.
The call to `sceAudioInInput` blocks until the next block is available, so it
belongs on a dedicated capture thread rather than the render, input, or UI
thread.

The DualSense microphone is exposed through this normal user-routed AudioIn
path. Applications do not need to read controller HID or Bluetooth reports to
obtain microphone samples, and the inspected Pad/HID libraries do not expose a
parallel PCM stream. `libSceAudioSystem` manages device routing internally; it
is not the app-facing capture API.

## Runtime-validated result

Two direct, Chiaki-free title runs on firmware 6.02 captured the powered-on,
unmuted DualSense microphone through all three useful input purposes. Each
route produced a valid four-second RIFF/WAVE file containing 64,000 mono
signed-16 samples at 16 kHz.

| Purpose | Type | Peak | RMS | Silence state during the first clean run |
|---|---:|---:|---:|---|
| Voice chat | `0` | 31,989 | 5,701.53 | `0` throughout |
| General | `1` | 30,354 | 4,909.37 | `0` throughout |
| Voice recognition | `5` | 11,830 | 1,790.67 | `0` throughout |

A repeat run again produced nonzero PCM for every purpose. It also observed
brief route-transition intervals before or after otherwise valid captures.
Treat a nonzero silence-state result as an instantaneous reason mask, not as a
permanent failure and not as proof that an entire recording is silent.

The tested Game-category title required no additional microphone package
declaration or application consent prompt. That is evidence for this firmware
and title configuration only. Controller mute, user audio settings, selected
input device, parental/privacy controls, and later firmware behavior can still
gate capture.

## Required controller state

- The controller must be powered on, connected, and associated with the user
  passed to `sceAudioInOpen`.
- The orange microphone-button light means the controller microphone is
  muted. For capture, the orange light must be off.
- No separate recording light appeared in the validated runs; an unlit mute
  button does not mean the microphone is inactive.
- If the controller is off, `sceAudioInOpen` can still succeed and timed input
  blocks can still arrive, but those blocks may contain only zeros and the
  silence-state mask can remain nonzero.
- A user-routed open follows the user's selected input. If another USB or
  wireless microphone is selected, the samples may come from that device
  instead of the controller.

Remote Play can change controller and audio routing. The conclusive validation
launched the title directly through the loader control service with Chiaki-ng
absent. Use the same arrangement when the goal is to validate the microphone
on a controller connected locally to the console.

## Purpose and format values

The application-facing open call is:

```cpp
extern "C" std::int32_t sceAudioInOpen(
    std::int32_t user_id,
    std::int32_t type,
    std::int32_t index,
    std::uint32_t grain,
    std::uint32_t frequency,
    std::uint32_t format);
```

Use one of these user-routed purposes:

| Purpose | Value | Application use |
|---|---:|---|
| Voice chat | `0` | Voice communication path |
| General | `1` | Recording, analysis, or an app-owned speech pipeline |
| Voice recognition | `5` | Speech-recognition purpose |

All three produced controller PCM at runtime. Choose by application purpose;
do not select a value based only on the amplitude from one spoken test.

The inspected implementation accepts these core settings:

| Setting | Accepted values | Recommended controller probe |
|---|---|---|
| index | `0` | `0` |
| grain | 128 or 256 frames | 256 frames |
| sample rate | 16 kHz or 48 kHz | 16 kHz |
| signed-16 mono format | `0` (a legacy mono alias `1` is also accepted) | `0` |
| signed-16 stereo format | `2` | Use only when stereo is required |

The static format path also contains float variants selected by the `0x10`
format flag. They were not needed for the controller proof and are not part of
the runtime claim here. The example deliberately uses canonical format `0`,
signed-16 mono.

## Native C++20 pattern

```cpp
#include <array>
#include <cstdint>
#include <span>

extern "C" {
std::int32_t sceAudioInOpen(std::int32_t user_id, std::int32_t type,
                            std::int32_t index, std::uint32_t grain,
                            std::uint32_t frequency, std::uint32_t format);
std::int32_t sceAudioInInput(std::int32_t handle, void* destination);
std::int32_t sceAudioInGetSilentState(std::int32_t handle);
std::int32_t sceAudioInClose(std::int32_t handle);
}

enum class InputPurpose : std::int32_t {
    voice_chat = 0,
    general = 1,
    voice_recognition = 5,
};

constexpr std::uint32_t grain = 256;
std::array<std::int16_t, grain> block{};
const auto handle = sceAudioInOpen(
    initial_user_id,
    static_cast<std::int32_t>(InputPurpose::general),
    0, grain, 16'000, 0);
if (handle < 0) {
    return 1;
}

while (recording()) {
    if (sceAudioInInput(handle, block.data()) < 0) {
        break;
    }
    const auto silence_state = sceAudioInGetSilentState(handle);
    process_microphone(std::span{block}, silence_state);
}
sceAudioInClose(handle);
```

The complete example writes ten seconds of PCM and a standard WAV header:
[`examples/native-audio-poc/microphone/src/main.cpp`](../examples/native-audio-poc/microphone/src/main.cpp).

## Lifecycle

1. Initialize UserService, tolerating an already-initialized service.
2. Resolve the initial or intended signed-in user ID.
3. Open one AudioIn route for that user.
4. Pull exactly one `grain` block per `sceAudioInInput` call.
5. Inspect the silence-state result for diagnostics, but inspect PCM energy as
   the authoritative recording result.
6. Close the AudioIn handle when capture stops.
7. Terminate UserService only if the application initialized it.

The AudioIn handle can survive route activation and rerouting. Initial zero
blocks are therefore not necessarily fatal. If a route remains silent for the
application's bounded grace period, tell the user to power on and unmute the
controller and check the selected input. Close and reopen only after an API
error or a deliberate user/device change that the application chooses not to
handle in place.

## Low-latency design

At 16 kHz, a 256-frame block represents 16 ms. At 48 kHz, 128 frames represent
about 2.67 ms, although that does not prove the controller's end-to-end device
latency or that every route operates natively at that rate. Measure the full
application path before making a latency claim.

- Give capture its own worker thread.
- Size the destination as `grain * channels * bytes_per_sample`.
- Copy, queue, or process each returned block immediately; do not hold the
  capture thread while encoding, writing a large file, or updating UI.
- Use a bounded queue and an explicit overflow policy for downstream work.
- Keep AudioIn and AudioOut on separate threads when implementing monitoring;
  both APIs pace against device clocks, and acoustic feedback needs its own
  policy.
- Count active and silent blocks. OR-ing every silence mask is useful for
  diagnostics, but one transient bit must not invalidate later nonzero PCM.

## Privacy and product behavior

The proof of concept demonstrates technical access, not permission to record
people without notice or consent. A production application should make capture
obvious, provide an explicit start/stop control, minimize retention, protect
stored audio, and follow the laws and platform rules applicable to its users
and jurisdictions. The absence of a firmware prompt in this test is not a
substitute for the application's own consent and privacy design.
