# Investigation Report

## Question

Does the PS5 have hardware capability to decode audio such as MP3, AAC, or audio carried in MP4, or is decoding performed by the application CPU?

For the broader API and implementation guide, see [README.md](README.md). The codec-specific usage
examples are in [docs/DECODING.md](docs/DECODING.md), while the output and media paths are documented
in [docs/AUDIO-OUTPUT.md](docs/AUDIO-OUTPUT.md) and [docs/MEDIA.md](docs/MEDIA.md).

## Terminology

MP4 is a media container rather than an audio codec. An MP4 file may contain AAC, Opus, AC-3, E-AC-3, or another stream. The codec inside the container determines the decoder path.

## Evidence boundary

The strongest accurate description for the native paths is **hardware/firmware
audio offload** or **AJM-backed native decoding**. The recovered call chains
and runtime results establish submission through the PS5 audio services and AJM
for AAC, MP3, and Opus; they do not identify a particular physical DSP or
fixed-function decoder block. Vorbis and FLAC are application CPU fallbacks in
the production implementation.

The finalized production source is [PSRadio v0.2.0 commit `5c25b46`](https://github.com/blackbearreloaded/psradio/tree/5c25b4651efdcfb034431054e2b7e0309d4c88d2),
with the detailed source record in
[`docs/CODEC_INVESTIGATION.md`](https://github.com/blackbearreloaded/psradio/blob/5c25b4651efdcfb034431054e2b7e0309d4c88d2/docs/CODEC_INVESTIGATION.md)
and [`docs/AUDIO_RELEASE_VALIDATION.md`](https://github.com/blackbearreloaded/psradio/blob/5c25b4651efdcfb034431054e2b7e0309d4c88d2/docs/AUDIO_RELEASE_VALIDATION.md).
Runtime records below are firmware 6.02 evidence from game-category packages.

## High-level architecture found

```text
libSceAvPlayer
    |-- explicit AudioDecHw / AudioDecSw selection
    |
    |-- hardware AAC path
    |       `-- libSceAudiodec
    |               `-- libSceAjmi / libSceAjm
    |                       `-- /dev/ajm + ioctl + batch jobs
    |
    `-- CPU software AAC path
            `-- sceAudiodecCpuInternal* functions
```

This is a real split in the binary, not an inference based only on library names. AvPlayer contains separate constructors and decode methods for hardware and software decoder objects. The safest description is hardware/firmware audio offload or AJM-backed native decoding; the evidence does not identify a physical DSP block.

## AAC: hardware path confirmed

The `libSceAvPlayer.native.sprx` decoder factory contains the literal log strings:

```text
Create AudioDecHw(AAC)
Create AudioDecSw(AAC)
```

The hardware AAC constructor's initialization path calls:

```text
sceAudiodecInitialize
sceAudiodecRegisterCodec
sceAudiodecCreateDecoderEx
```

The hardware AAC decode method calls:

```text
sceAudiodecDecodeEx
```

This proves that AvPlayer has an AAC decode path through the platform audio decoder service rather than only through a CPU codec implementation.

The production PSRadio path uses `AudioDec = 0x0088`, codec type `3`, and
network ADTS framing. On firmware 6.02 it decoded signed-16 PCM to AudioOut,
survived AAC-to-Opus and reverse transitions, and passed immediate AAC stop.
The final dual-Opus-decoder build also opened an AAC stream and produced PCM.
When stream metadata or output characteristics require it, the application
recreates the decoder with HE-AAC disabled.

## AAC: CPU software path confirmed

The software decoder initialization path calls:

```text
sceAudiodecCpuInternalQueryMemSize
sceAudiodecCpuInternalInitDecoder
sceAudiodecCpuInternalClearContext
```

The software decode path calls:

```text
sceAudiodecCpuInternalDecode
```

Therefore the PS5 supports both an offloaded AAC path and a CPU software fallback. Which path is selected depends on the decoder construction parameters and the AvPlayer decision logic.

## Opus: hardware path explicitly present

The AvPlayer decoder factory contains:

```text
Create AudioDecHw(Opus)
```

The Opus object creation path constructs `SceVseOpusDecoder` and enters the
common decoder constructor with hardware selection enabled. Its hardware
constructor shares base initialization with the hardware AAC and AC-3 paths.

The standalone `libSceOpusDec` database now establishes the simpler application-facing route. Its
`sceOpusDecInitialize` path initializes AJMI and registers codec 21; `sceOpusDecCreateEx` creates a
stateful instance; `sceOpusDecDecode` submits packet and PCM buffers through AJMI; and destroy/terminate
release the instance and module. The production PSRadio implementation uses
this general decoder first and retries one native `-502` through
`libSceOpusCeltDec` codec 16 when the packet TOC configuration is 16-31. Both
general and CELT lifecycles are runtime-proven on firmware 6.02. This removes
the need for an application to construct raw AJM batches.

## MP3: native path runtime-proven

The AJM database contains:

- `MP3 Decoder` string data.
- `sceAjmDecMp3ParseFrame` entrypoint.
- Generic batch decode job construction capable of submitting bitstream input and receiving PCM output.

The production native path uses `libSceAudiodec` codec type `2` and is
runtime-proven on firmware 6.02: a live 48 kHz stereo stream produced 4,608
signed-16 PCM bytes and reached AudioOut. The function-level AvPlayer factory
trace remains useful ABI follow-up, but it is no longer a blocker for native
MP3 use.

## MP4

There is no single “MP4 decoder.” The container is demultiplexed, then the contained audio codec is decoded. For example:

- MP4/AAC uses the AAC hardware or CPU path.
- MP4/Opus uses the Opus path.
- MP4/AC-3 uses the AC-3 path.

## Vorbis and FLAC boundary

The recovered AvPlayer factory exposes AAC hardware/software, Opus hardware,
AC-3 hardware, and E-AC-3 software branches, but no Vorbis or FLAC branch. The
inspected AJM Vorbis helper builds/parses headers but exposes no PCM decode
route. This establishes that no callable native Vorbis route was found; it
does not claim that every AvPlayer container path lacks Vorbis.

`libSceAudiodecCpu.sprx` references an internal CPU FLAC plug-in,
`libSceAudiodecCpuFlac.prx`, at module `0x80000053`. The plug-in was absent
from the inspected firmware inventory, and a runtime load attempt returned
`-2141581312` (`0x805a1000`). No usable native FLAC route exists on the tested
baseline.

AvPlayer ABI probes all rejected AddSource at `stage=source` with
`-2140536829` (`0x806a0003`) for Vorbis/WebM, Opus/WebM, and an AAC/M4A
positive control. Because the positive control failed identically, these
probes cannot discriminate codec support and must not be used as runtime proof
of Vorbis absence.

## AJM: device-backed offload evidence

In `libSceAudiodec.native.sprx`:

- `sceAudiodecRegisterCodec` dispatches into AJM/AJMI registration functions, including `sceAjmModuleRegister` for codec modules.
- `sceAudiodecDecodeWithPriorityEx` obtains the AJM codec type and dispatches through AJMI.
- `sceAudiodecClearContextEx` follows the same AJM-backed codec context path.

In `libSceAjm.native.sprx`:

- `sceAjmInitialize` opens `/dev/ajm`.
- `sceAjmModuleRegister` submits a codec module registration ioctl.
- `sceAjmInstanceCreate` submits codec instance creation through ioctl.
- `sceAjmBatchJobDecode` builds a job containing bitstream input, PCM output, sizes, and result storage.
- `sceAjmBatchStart` submits the batch through the kernel-facing AJM device interface.
- `sceAjmBatchWait` waits for completion through the AJM device.

This is the key hardware/firmware boundary. The decode request is represented as an AJM device job and submitted through the kernel-facing device interface.

## Relevance to Moonlight

Moonlight normally receives raw Opus packets. The practical choices are now:

1. Use the validated native general/CELT Opus path first.
2. Keep the existing CPU libopus path as the compatibility baseline.
3. Use raw AJM only if the wrapper lacks a required control or batching feature.

Static analysis established that the direct wrapper was the smallest native
offload experiment; production PSRadio now supplies runtime evidence for both
native Opus routes.

## Scope and confidence

High confidence:

- AAC has distinct hardware and CPU paths.
- Hardware AAC reaches `sceAudiodecDecodeEx`.
- CPU AAC reaches `sceAudiodecCpuInternalDecode`.
- AJM is a device-backed audio job path.
- AvPlayer has an explicit hardware Opus path.
- Native MP3 codec 2 reaches AJM and produced PCM through AudioOut.
- Native Opus codec 21 and CELT codec 16 both decode on firmware 6.02.
- No callable native Vorbis route was found in the inspected static surfaces.
- The internal FLAC CPU plug-in reference is absent/unloadable on the tested baseline.
- Bounded `stb_vorbis` CPU decoding is implemented and validated on PS5.
- Bounded `dr_flac` CPU decoding is implemented and validated for native and
  Ogg-FLAC on PS5.
- The bounded unencrypted audio-only HLS/MPEG-TS/AAC-LC path is implemented
  and validated through the native AAC decoder.

Not established by this investigation:

- The exact physical audio engine or DSP block used internally.
- Whether every title/application context exposes the same codec modules without additional privileges or service setup.
- Gap-free audible behavior and exact stop/switch latency for every possible
  live stream shape; the release proves bounded recovery and stability, not an
  absence of every audible gap.
- Native two-channel HE-AAC Parametric Stereo on firmware 6.02. SBR
  reconstruction is proven, but valid public PS configurations exposed one
  channel and recovered AvPlayer-style two-channel variants were not callable.
- Behavior beyond the tested firmware, codecs, stream shapes, and explicitly
  bounded HLS feature set.

## Native Opus probe evidence

Early packaged-probe runs that failed during registration, runtime loading, or
optional diagnostic output are excluded from decoder evidence because they did
not reach the decode lifecycle. After removing those harness issues, a known
310-byte, 20 ms, 48 kHz stereo packet produced the expected 3,840 bytes of
signed-16 PCM with nonzero energy through the native codec-21 path. The title
remained alive for the observation window and closed cleanly.

## Final PSRadio production validation

The finalized implementation extends the earlier native evidence with bounded
CPU fallbacks, HLS/AAC delivery, and release hardening. The production source
is v0.2.0 commit
[`5c25b46`](https://github.com/blackbearreloaded/psradio/tree/5c25b4651efdcfb034431054e2b7e0309d4c88d2).

| Format/path | Device result on firmware 6.02 |
|---|---|
| Native MP3 | A live 48 kHz stereo stream decoded through `libSceAudiodec` codec `2` to 4,608 signed-16 PCM bytes and reached AudioOut. |
| Native Opus | A lifecycle probe measured 67 ms stop-to-stopped and switched to a second 48 kHz stereo stream without a crash; a separate live session completed a ten-minute soak. |
| HLS/AAC, MP3, Opus, Vorbis | The cross-format matrix played and stopped every listed format, then switched Vorbis directly back to HLS/AAC and stopped. |
| Native FLAC | A 44.1 kHz stereo native-container stream played, stopped, and switched to AAC. |
| Ogg-FLAC | Playback remained active at 44.1 kHz stereo for more than eleven minutes without a later error or rebuffer. |
| Vorbis | A 44.1 kHz stereo stream played for about eleven minutes without a rebuffer/error transition and released runtime layers normally. |
| HE-AAC | A controlled probe confirmed SBR reconstruction with nonzero 48 kHz mono PCM and energy above 12 kHz; HE-disabled output was the 24 kHz mono AAC core. Native public two-channel PS was not established. |
| Fault-injected hardening | The matrix covered reconnect, underrun, malformed Ogg, HLS discontinuity, ICY metadata, chained Vorbis, MP3, FLAC, and rapid cross-codec switching without a title crash, fatal signal, loader failure, or stuck runtime layer. |

The final PSRadio build has zero unresolved imports as a Game-category app.

The published release asset was verified by download and upload round trips,
launched as a Game-category application, displayed the expected version,
played and switched live streams, reached MP3 at 48 kHz stereo, and closed
cleanly. Runtime monitoring confirmed normal resource release. The full release record is in
[docs/AUDIO_RELEASE_VALIDATION.md](docs/AUDIO_RELEASE_VALIDATION.md).

This validates the implemented baseline and the completed release hardening
matrix. SBR reconstruction is confirmed, while native public two-channel PS
remains unestablished on firmware 6.02; neither that fidelity boundary nor
broader untested format/transport behavior should be presented as a missing
AAC, MP3, Opus, Vorbis, or FLAC implementation.
