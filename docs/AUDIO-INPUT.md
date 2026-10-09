# Controller microphone input

A test title captured the built-in microphone of a wireless controller through
the platform audio input, opened for the signed-in user. The capture call
waits for each block, so the title ran it on a dedicated thread.

## Device result

Two runs on the tested system software, with the controller connected
directly to the console, captured a powered-on, unmuted controller microphone
on three input purposes. Each produced a valid four-second WAV file containing
64,000 mono signed-16 samples at 16 kHz, read in 256-frame blocks.

| Purpose | Peak | RMS | Silence indication during the first run |
|---|---:|---:|---|
| Voice chat | 31,989 | 5,701.53 | none |
| General | 30,354 | 4,909.37 | none |
| Voice recognition | 11,830 | 1,790.67 | none |

A repeat run again produced nonzero audio for every purpose. It also showed
brief silence indications before or after otherwise valid captures. A silence
indication is therefore momentary; it does not mean the whole recording is
silent.

Controller mute, user audio settings, the selected input device, privacy
controls, and other system software versions can change the result.

## Controller state

- The controller must be powered on, connected, and assigned to the user the
  input was opened for.
- The orange microphone-button light means the controller microphone is
  muted. For capture, the orange light must be off.
- No separate recording light appeared in the test runs.
- With the controller off, the input still opened and timed blocks still
  arrived, but they contained only zeros and the silence indication stayed
  on.
- The input follows the user's selected device. If another microphone is
  selected, the samples may come from that device.

## Capture thread notes

At 16 kHz, a 256-frame block is 16 ms. End-to-end latency was not measured.

- Give capture its own thread.
- Copy, queue, or process each block immediately; do not hold the capture
  thread while encoding, writing a large file, or updating UI.
- Use a bounded queue and an explicit overflow policy for downstream work.
- Count active and silent blocks. One momentary silence indication must not
  invalidate later nonzero audio.

## Privacy

The test shows technical access, not permission to record people without
notice or consent. An application that records should make capture obvious,
provide an explicit start/stop control, keep as little audio as possible,
protect stored audio, and follow the laws and platform rules that apply to its
users.
