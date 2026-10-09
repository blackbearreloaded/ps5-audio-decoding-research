# PCM output

The test application sends all decoded audio to one platform audio output
opened for 48 kHz stereo signed-16 PCM.

## Block format

| Parameter | Value used |
|---|---|
| block size | 256 frames per channel |
| sample rate | 48000 Hz |
| format | Signed 16-bit stereo, interleaved |
| output block | 512 samples: left, right, left, right, ... |

## Observed queue behavior

One output call takes one complete block and waits until the queue has room,
so it paces the producer. The application holds a short final block and
zero-fills it before the last call, then waits for queued audio before
closing the output.

Because the call waits, the audio loop runs on its own thread. Controller and
UI threads send commands such as play, pause, and volume through a small
synchronized state or queue.

## Pairing decoded audio with output

Decoded frames rarely line up with the output block size. The application
accumulates decoded samples, duplicates mono to both channels, and submits
complete blocks. For other sample rates it resamples to 48 kHz first.
