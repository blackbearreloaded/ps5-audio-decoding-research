# Static evidence map

This summary retains callable API names, behavior, and runtime conclusions but
intentionally omits image addresses, private import identifiers, and numeric
kernel request values.

## `libSceAvPlayer.native.sprx`

| Component | Evidence |
|---|---|
| Decoder factory | Contains `Create AudioDecHw(AAC)`, `Create AudioDecSw(AAC)`, `Create AudioDecHw(Opus)`, `Create AudioDecHw(AC3)`, and `Create AudioDecSw(Eac3)` branches, with no Vorbis or FLAC decoder branch observed. |
| Common decoder construction | Creates codec-specific decoder objects and selects hardware/software mode from decoder parameters. |
| Hardware AAC initialization | Calls `sceAudiodecInitialize`, `sceAudiodecRegisterCodec`, and `sceAudiodecCreateDecoderEx`. |
| Hardware AAC decoding | Calls `sceAudiodecDecodeEx`. |
| Software AAC initialization | Calls the `sceAudiodecCpuInternal*` setup family. |
| Software AAC decoding | Calls `sceAudiodecCpuInternalDecode`. |
| Other observed branches | Hardware Opus and AC-3 constructors and a software E-AC-3 constructor share the common decoder framework. |

## `libSceAudiodec.native.sprx`

| Function | Evidence |
|---|---|
| `sceAudiodecInitialize` / `sceAudiodecTerminate` | Own the decoder context lifecycle. |
| `sceAudiodecRegisterCodec` | Dispatches codec registration through AJM/AJMI or `sceAjmModuleRegister`. |
| `sceAudiodecCreateDecoderEx` | Dispatches codec-specific decoder creation. |
| `sceAudiodecDecodeWithPriorityEx` | Obtains the AJM codec type and dispatches decoding through AJMI. |
| `sceAudiodecDecode2WithPriorityEx` | Provides an alternate decode entrypoint. |
| `sceAudiodecDecodeEx` | Is used by the hardware AAC AvPlayer path. |
| `sceAudiodecClearContextEx` | Cleans up AJM-backed codec context state. |

The production native AAC path uses codec type `3` (`AUDIODEC_AAC`). The
production native MP3 path uses codec type `2`; a live 48 kHz stereo stream
decoded to 4,608 bytes of signed-16 PCM and reached AudioOut.
These codec numbers are runtime evidence for this firmware/application
combination, not portable public constants.

## `libSceAudiodecCpu.sprx`

Static inspection shows a reference to `libSceAudiodecCpuFlac.prx` through
internal sysmodule ID `0x80000053`. This is a CPU decoder plug-in reference,
not AJM or hardware/offload evidence. The plug-in was absent from the
inspected firmware inventory, and a runtime probe confirmed that
`sceSysmoduleLoadModuleInternal(0x80000053)` failed with `-2141581312`
(`0x805a1000`). No usable native FLAC route exists on this baseline.

## `libSceAjm.native.sprx`

| Function | Evidence |
|---|---|
| `sceAjmInitialize` | Opens `/dev/ajm` and initializes the AJM context through a kernel-facing request. |
| `sceAjmModuleRegister` / `sceAjmInstanceCreate` | Submit codec-module and instance setup. |
| `sceAjmBatchInitialize` | Initializes a caller-provided batch buffer. |
| `sceAjmBatchStart` / `sceAjmBatchWait` / `sceAjmBatchCancel` | Submit, synchronize, and cancel batches. |
| `sceAjmBatchJobInitialize` | Builds a codec initialization job record. |
| `sceAjmBatchJobDecode` / `sceAjmBatchJobDecodeSingle` | Build bitstream-to-PCM decode jobs. |
| `sceAjmBatchJobRun` | Builds a generic AJM run job. |
| `sceAjmBatchJobGetCodecInfo` | Builds a codec information query job. |

## `libSceOpusDec.sprx`

| Function | Evidence |
|---|---|
| `sceOpusDecInitialize` | Initializes a caller-owned AJMI context and registers AJM codec 21. |
| `sceOpusDecGetSize` | Returns 640 bytes for one or two channels. |
| `sceOpusDecCreate` / `sceOpusDecCreateEx` | Create instances using library-global or caller-owned context; the extended form accepts 8, 12, 16, 24, or 48 kHz and one or two channels. |
| `sceOpusDecDecode` | Sends one compressed packet and a signed-16 PCM buffer through the common AJMI batch path; the output-capacity argument and successful return are byte counts. |
| `sceOpusDecDecodeFloat` | Uses the same AJMI path, then expands signed-16 output to float in place. |
| `sceOpusDecDestroy` | Destroys the AJMI codec instance and clears its state marker. |

The complete decoder state is 640 bytes. The direct wrapper is therefore the
preferred runtime probe; raw AJM setup is unnecessary for ordinary packet
decoding.

Two system consumers independently confirm the decode byte units:

- A system WebRTC consumer uses an 11,520-byte output capacity, then divides
  the successful return by `2 * channels` to obtain frames.
- A retail system consumer uses a 1,280-byte capacity, bounds the return
  against that value, then divides by `2 * channels`.

The named decode core accepts nonnegative packet and output byte counts. No
explicit packet-size maximum was observed in that path; a separate
unidentified export family has an 8,011-byte limit and must not be conflated
with `sceOpusDecDecode`.

## `libSceOpusCeltDec.sprx`

The CELT wrapper mirrors the 640-byte state layout and AJMI lifecycle but registers AJM codec 16.
Its retained lifecycle imports are:

```text
sceOpusCeltDecInitialize
sceOpusCeltDecTerminate
sceOpusCeltDecGetSize
sceOpusCeltDecCreateEx
sceOpusCeltDecDecode
sceOpusCeltDecDestroy
```

It is a separate CELT-only path, not the default route for standard Opus
packets. The companion PSRadio source checks in `libSceOpusCeltDec_stub.a` as
generated import metadata only, not Sony runtime code. Its SHA-256 is
`AFCEAAD3A442CC87412FC96E30716082081AEC846AA1697608910B9FD1F3F51D`, and no
Sony runtime module ships in the application.

## `libSceAjmi.sprx`

The wrapper imports `open`, `ioctl`, and `close` and constructs AJM batches used by both Opus decoder
libraries. This strengthens the device-backed hardware/firmware-offload attribution without identifying
the exact physical decoder block.

## `libSceAudioIn.sprx`

| Function | Evidence |
|---|---|
| `sceAudioInOpen` | Opens a user-routed input for a purpose, fixed grain, sample rate, and PCM format. The accepted core settings include 128/256-frame grains and 16/48 kHz. |
| `sceAudioInInput` | Waits for and copies one complete PCM block into application-owned memory. |
| `sceAudioInGetSilentState` | Returns an instantaneous silence-reason mask; it is not a single permanent boolean result. |
| `sceAudioInClose` | Releases the application input handle. |

The application route is associated with a signed-in user and input purpose.
Signed-16 mono format `0` and stereo format `2` are accepted; a legacy mono
alias is also present. The inspected float path was not needed for the runtime
controller proof.

Static inspection of the audio-routing service shows that controller
microphones are managed as per-controller audio devices and fed into the normal
AudioIn routing system. The app-facing path does not require direct Bluetooth
or HID PCM access. The privileged audio-system service is routing
infrastructure, not a library that ordinary applications should call for raw
capture.

Two firmware-6.02 runs then corroborated the path at runtime: VoiceChat,
General, and VoiceRecognition each produced valid 16 kHz signed-16 mono PCM
from a powered-on, unmuted DualSense. See
[controller microphone input](docs/AUDIO-INPUT.md) for the bounded public
recipe and evidence limits.

## `libSceAvPlayerStreaming.sprx`

The library exposes HTTP and transport-stream streaming support (`MvpHttp*`, `sceTs*`). It is relevant
to HLS/TS delivery but is not required for raw Opus packet decoding.

## Important strings and symbols

Observed during static inspection:

- `Create AudioDecHw(AAC)`
- `Create AudioDecSw(AAC)`
- `Create AudioDecHw(Opus)`
- `SceVseAacDecoder`
- `SceVseOpusDecoder`
- `AudioDecoderHw`
- `AudioDecoderSw`
- `/dev/ajm`
- `MP3 Decoder`
- `sceAjmDecMp3ParseFrame`

The inspected AJM Vorbis helper contains header-building/parsing support but
no callable PCM decode route. Combined with the absence of a Vorbis or FLAC
branch in the recovered AvPlayer decoder factory, this is the static basis for
the “no callable native Vorbis route found” conclusion. It is not a claim that
the failed AvPlayer `AddSource` probes prove Vorbis absence. Those probes all
failed at `stage=source` with `-2140536829` (`0x806a0003`), including the AAC/M4A
positive control.

## Production corroboration

The static record is corroborated by the production PSRadio implementation:

- [final audio implementation at PSRadio v0.2.0 `5c25b46`](https://github.com/blackbearreloaded/psradio/tree/5c25b4651efdcfb034431054e2b7e0309d4c88d2)
  dispatches AAC/MP3, native and Ogg-FLAC, Vorbis, and HLS/AAC through the
  shared bounded PCM/AudioOut path;
- [native audio service at `cba75ee`](https://github.com/blackbearreloaded/psradio/blob/cba75ee/src/radio_service.c)
  established the production CELT routing change;
- [Opus wrapper at `5c25b46`](https://github.com/blackbearreloaded/psradio/blob/5c25b4651efdcfb034431054e2b7e0309d4c88d2/src/opus_decoder.c)
  owns both general codec 21 and CELT codec 16 lifecycles;
- the companion [codec investigation](https://github.com/blackbearreloaded/psradio/blob/5c25b4651efdcfb034431054e2b7e0309d4c88d2/docs/CODEC_INVESTIGATION.md)
  records the final FLAC, Vorbis, HLS, and AvPlayer probe boundaries.

The final device matrix also recorded native-container FLAC, sustained
Ogg-FLAC playback, HLS/AAC, MP3, Opus, and Vorbis cross-format playback, and
an approximately eleven-minute Vorbis run. See
[runtime validation](docs/VALIDATION.md) for the behavioral evidence.

These runtime results are application evidence on firmware 6.02. They support
the term **hardware/firmware audio offload** or **AJM-backed native decoding**;
they do not identify a particular physical DSP or fixed-function block.
