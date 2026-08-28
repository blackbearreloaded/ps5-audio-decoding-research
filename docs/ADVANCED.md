# Mixing, DSP, NGS2, spatial audio, and encoding

The simplest path is to decode or synthesize PCM, mix it in the application, and submit stereo blocks to
the shared C++20 `AudioOut` sink. The lower-level libraries exist when the application needs a custom voice graph,
object-based routing, or platform encoding.

## Audio assets and application DSP

### WAV and PCM

Represent application PCM as interleaved `std::int16_t` samples plus explicit
sample-rate and channel metadata. This repository does not bundle a second WAV
framework; use a small RIFF reader for PCM assets or convert them to the final
48 kHz mono/stereo shape ahead of time.

### VAG effect assets

VAG/ADPCM-style assets are useful for compact effects, but this repository does
not publish a standalone native VAG codec example. Convert assets ahead of time
or add a bounded C++20 decoder only when the application requires runtime VAG.

Convert assets ahead of time where possible. VAG names are limited to the format's short ASCII header;
the decoder stops at the end marker rather than playing padding as audio.

### Mixing

A native mixer owns a bounded voice list, accumulates into a wider integer or
float buffer, clamps once to signed-16, and submits complete blocks through
`ps5::native_audio::AudioOut`. Keep looping state, gain, and resampler phase in
each voice rather than allocating or recreating DSP objects per block.

### Procedural effects and filters

`ToneGenerator` supports sine, square, triangle, sawtooth, and noise waveforms. `BiquadFilter` supports
low-pass, high-pass, band-pass, and notch shapes. `AdsrEnvelope` supplies attack, decay, sustain, and
release gain for notes. Each DSP object carries state across blocks; do not recreate it every block.

For an interleaved stereo block, keep one filter state per channel, process
left and right independently, then use `std::lround` and `std::clamp` before
converting each result to `std::int16_t`.

## NGS2: custom synthesis and render graphs

NGS2 is a low-level voice/mixing engine. The common lifecycle is:

```text
sceNgs2SystemResetOption
    -> sceNgs2SystemQueryBufferSize
    -> allocate aligned system buffer
    -> sceNgs2SystemCreate
    -> sceNgs2RackQueryBufferSize / RackCreate
    -> sceNgs2RackGetVoiceHandle
    -> VoiceControl / VoiceRunCommands
    -> sceNgs2SystemRender
    -> RackDestroy / SystemDestroy
```

Observed rack types include `Sampler`, `Submixer`, `Reverb`, `Mastering`, and
custom rack variants. A render buffer can be signed-16 or float and 1, 2, 6, or 8 channels. Set
`SceNgs2RenderBufferInfo.WaveformType` to `PcmI16L` and `NumChannels` to 2 when the render output is
going directly to the native `AudioOut` sink.

NGS2 has its own waveform parsing helpers, stream objects, command lists, report callbacks, and job
scheduler. It is appropriate for a game-like sound graph, not for a simple radio player.

## Spatial audio options

There are three progressively lower-level choices:

1. `AudioMixer` plus two-channel gain/panning in application code.
2. NGS2 geometry/panning (`sceNgs2Geom*`, `sceNgs2Pan*`) feeding voice matrices.
3. `AudioOut2`/`Audio3d` object ports and speaker-array processing.

NGS2 geometry works from listener/source positions, velocity, direction cones, rolloff, Doppler factor,
and speaker volume matrices. `Audio3d` can reserve objects, set object attributes, write beds, and query
speaker coefficients. `AudioOut2` exposes object ports with position, spread, gain, passthrough, and
ambisonic attributes. Start at level 1 unless the application really needs platform spatial routing.

## AAC encoding

`libSceM4aacEnc` exposes AAC-LC encoding for 48 kHz mono/stereo in the observed
28,000–320,000 bit/s range. A native implementation loads module `0x00BC`,
queries work memory, creates an encoder over application-owned aligned storage,
submits exactly 1024 samples per channel per frame, flushes, destroys the
encoder, and unloads the module. This guide does not present that sequence as a
runnable C++20 example until its complete structures and cleanup path are
target-validated.

Use ADTS output when each frame must be independently recognizable by the AAC decoder. Use raw output
only when the consumer has the out-of-band configuration.

Load system module `0x00BC` before creating the encoder.

## ATRAC9 encoding

`At9Enc` is a raw binding. The application supplies `SceAt9EncParam`, calls
`sceAt9EncQueryMemSize`, allocates an 8-byte-aligned work area, creates the encoder into that area,
then calls `sceAt9EncEncode`/`Flush`. The encoder returns four configuration bytes that a container or
asset header must preserve for decoding.

There is no high-level `At9Audio` asset wrapper in the current guide. Treat it as an explicit toolchain
or asset-pipeline task until a target-validated container workflow is added.

For a ready host-side asset workflow, see
`workspace/dev/ps5-at9-converter`. It decodes MP3/WAV/M4A/AAC/WMA through Windows Media Foundation,
converts to 48 kHz stereo PCM, and writes the fixed PS5 `snd0.at9` profile. Its
`docs/atrac9-profile.md` documents the RIFF/AT9 layout and its 2 MiB asset-size constraint. This is a
desktop encoder, not the PS5 runtime decoder path.

## Libraries present but not yet documented as application APIs

`libSceAudioPropagation`, `libSceAudioSystem`, and `libSceCustomMusicAudioOut` appear in the inspected inventory.
Their presence is not enough to document a safe call sequence. Keep them in the inventory, but do not
build a new application around them without first recovering parameter structures and running a probe.
