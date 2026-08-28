# AJM: the low-level audio job manager

AJM is the device-backed job layer below the platform audio codec service. It is the most important
static evidence for the hardware/firmware-offload conclusion, but it is not the easiest
application API.

## Recovered workflow

The inspected `libSceAjm` path shows:

```text
sceAjmInitialize
    -> open /dev/ajm
    -> ioctl initialization
sceAjmMemoryRegister (when required)
sceAjmModuleRegister(codec)
sceAjmInstanceCreate(codec)
sceAjmBatchInitialize(buffer)
    -> sceAjmBatchJobInitialize / Decode / Encode / GetInfo
sceAjmBatchStart
sceAjmBatchWait
    -> inspect result structures
sceAjmInstanceDestroy
sceAjmModuleUnregister
sceAjmFinalize
```

The decode job contains a bitstream input pointer/size, a PCM output
pointer/size, an instance handle, and result storage. `sceAjmBatchStart`
reaches the kernel-facing AJM device interface; `sceAjmBatchWait` waits for
completion through the same device context.

## Local binding

The observed native API surface includes these major entrypoints:

- initialization/finalization;
- memory register/unregister;
- codec module register/unregister;
- instance create/extend/switch/destroy;
- batch initialization/start/wait/cancel;
- decode, decode-single, split-decode, encode, run, codec-info, gapless, resample, statistics, and
  control jobs.

The binding is intentionally raw: many `void*` parameters represent SDK structures whose exact sizes,
flags, and result formats are not yet wrapped into safe C++20 owner types.

## What the databases establish

In `libSceAudiodec.native.sprx`:

- `sceAudiodecRegisterCodec` dispatches into AJM/AJMI registration.
- `sceAudiodecDecodeWithPriorityEx` obtains an AJM codec type and dispatches the decode operation.
- `sceAudiodecClearContextEx` follows the AJM-backed context cleanup path.

In `libSceAjm.native.sprx`:

- `sceAjmInitialize` opens `/dev/ajm`.
- `sceAjmModuleRegister` and `sceAjmInstanceCreate` submit setup operations.
- `sceAjmBatchJobDecode` constructs the bitstream-to-PCM job.
- `sceAjmBatchStart` and `sceAjmBatchWait` submit and synchronize the job.
- AJM contains an `MP3 Decoder` string and `sceAjmDecMp3ParseFrame`.

This is stronger than finding an unused export: the implementation constructs a device job with audio
buffers and submits it through the kernel-facing interface.

The finalized [PSRadio v0.2.0 implementation at `5c25b46`](https://github.com/blackbearreloaded/psradio/tree/5c25b4651efdcfb034431054e2b7e0309d4c88d2)
corroborates the mapping on firmware 6.02:

- `libSceAudiodec` codec `2` decoded live 48 kHz stereo MP3 and delivered
  4,608 signed-16 PCM bytes to AudioOut;
- `libSceOpusDec` registers AJM codec `21` for general/SILK/hybrid Opus;
- `libSceOpusCeltDec` registers AJM codec `16` for CELT-only Opus; and
- both Opus paths completed initialization, creation, repeated decode, and
  teardown in production routing.

These are AJM-backed native decode results. They should be described as
hardware/firmware audio offload, not as proof of a particular physical DSP.
Vorbis and FLAC are different: the production app uses bounded CPU decoders
because no callable native Vorbis route was found and the internal FLAC CPU
plug-in `0x80000053` was absent/unloadable on the tested baseline.

## Why not start with direct AJM?

The difficult parts are not the function names. They are:

- obtaining the correct codec module ID for the target firmware;
- registering or locating the codec module;
- creating the correctly shaped codec instance;
- allocating/alignment/registering batch memory;
- constructing result and parameter structures;
- handling completion, cancellation, and error dumps;
- preserving lifetime of every input/output buffer until the batch completes.

`AudioDecoder` already encapsulates the public-shaped codec setup for AAC and MP3 and reaches the same
platform service. Use it first. Move to AJM only when a required codec or batching feature cannot be
reached through the higher-level wrapper.

## Direct AJM research targets

Raw AJM work is now secondary. Keep general Opus on codec 21 and use codec 16
only for TOC configurations 16-31 after the production `-502` recovery
decision. Trace the already runtime-proven MP3 codec ID from AvPlayer into
`sceAudiodecRegisterCodec` and AJM module registration only when ABI-level
completeness is needed.

The codec IDs above are firmware-specific static evidence, not portable public constants. Direct AJM
`void*` structures remain intentionally undocumented until a caller actually needs them; the higher-level
Opus wrapper avoids them.
