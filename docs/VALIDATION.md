# Runtime validation

This record separates device-proven behavior from static analysis
and remaining scope. The finalized production source is [PSRadio v0.2.0 commit
`5c25b46`](https://github.com/blackbearreloaded/psradio/tree/5c25b4651efdcfb034431054e2b7e0309d4c88d2),
with the detailed release record in
[`AUDIO_RELEASE_VALIDATION.md`](AUDIO_RELEASE_VALIDATION.md).
The target evidence below was collected on firmware 6.02.

Use **hardware/firmware audio offload** or **AJM-backed native decoding** for
the native AAC, MP3, and Opus paths. The tests do not identify a particular
physical DSP or fixed-function decoder block.

## Final device evidence

| Scenario | Result |
|---|---|
| Native AAC | `libSceAudiodec` codec `3` handled ADTS and produced signed-16 PCM that reached AudioOut. |
| Native MP3 | Codec `2` decoded a live 48 kHz stereo stream to 4,608 signed-16 PCM bytes; AudioOut was reached. |
| Opus lifecycle | A production probe stopped in 67 ms, then switched to a second 48 kHz stereo stream without a crash. |
| Opus soak | A live session completed a ten-minute soak and stopped cleanly. |
| Cross-format playback | The production matrix played and stopped HLS/AAC, MP3, Opus, and Vorbis, then switched Vorbis directly back to HLS/AAC and stopped. |
| Native FLAC | A 44.1 kHz stereo stream played, stopped, and switched to AAC. |
| Ogg-FLAC soak | Playback remained active at 44.1 kHz stereo for more than eleven minutes without a later error or rebuffer. |
| Vorbis soak | A 44.1 kHz stereo stream played for about eleven minutes without a rebuffer/error transition, then released runtime layers normally. |

The final production build has zero unresolved imports and runs as a
Game-category application.

## Additional device evidence

| Scenario | Result |
|---|---|
| Direct Opus packet | A known 20 ms, 48 kHz stereo packet produced 3,840 signed-16 PCM bytes with nonzero energy through codec 21. |
| Codec transitions | AAC-to-Opus and reverse transitions completed without a crash. |
| Error handling | Repeated native decoder-error handling returned `-502` without an application crash. |
| Isolated CELT lifecycle | Module load, size query, initialize, create, and repeated live-packet decode succeeded and produced PCM after an unrelated unsafe diagnostic call was removed. |
| Production CELT routing | TOC dispatch selected CELT, every codec-16 setup stage completed, repeated packets decoded, PCM was produced, and the title stayed crash-free. |
| AAC regression | The final dual-Opus-decoder build opened native AAC, produced PCM, and remained crash-free for the observation window. |
| FLAC module load | Loading internal FLAC plug-in sysmodule `0x80000053` returned `-2141581312` (`0x805a1000`). |
| HE-AAC | HE enabled produced nonzero 48 kHz mono PCM with energy above 12 kHz; HE disabled produced the 24 kHz mono AAC core. Valid public PS configurations exposed one channel. |
| Fault injection | Reconnect, underrun, malformed Ogg, discontinuity, ICY, chained Vorbis, MP3, FLAC, and rapid switching completed without a title crash, fatal signal, loader failure, or stuck runtime layer. |

Several discarded harness runs called `sceKernelDebugOutText` immediately
after stream selection and jumped through a null libkernel target before
decoder-stage telemetry. They are not decoder-crash evidence. Removing that
optional call eliminated the SIGSEGV and exposed the successful decoder
lifecycle.

## AvPlayer probe boundary

The recovered AvPlayer ABI initialized successfully, but `sceAvPlayerAddSource`
rejected Vorbis/WebM, Opus/WebM, and an AAC/M4A positive control identically with
`-2140536829` (`0x806a0003`) at `stage=source`. These probes cannot discriminate
codec support and do not runtime-prove that AvPlayer lacks Vorbis.

The narrower static conclusion is that the decoder factory exposes AAC
hardware/software, Opus hardware, AC-3 hardware, and E-AC-3 software branches,
but no Vorbis or FLAC branch. The inspected AJM Vorbis helper prepares/parses
headers but exposes no PCM decode route. No callable native Vorbis route was
found.

## Live behavior and retry policy

The production path uses:

- a bounded two-second decoded-PCM ring;
- approximately one second of initial prime and approximately 0.5 seconds of
  underrun re-prime;
- a cancellable active HTTP request, including stop and station switching;
- immediate PCM discard on stop/error;
- decoder reset without tearing down AudioOut; and
- three consecutive attempts with 250/500/1,000 ms cancellation-aware
  backoff.

The retry budget is renewed only after real `sceAudioOutOutput` progress and at
least 30 seconds of active playback. This prevents lifetime-budget exhaustion
on long streams without creating unlimited persistent-failure retries. The
release matrix exercised three forced AAC reconnects, delayed Opus delivery,
malformed Ogg retry exhaustion, rapid switching, ICY metadata stripping, and
three live HLS discontinuity cycles.

Some streams paused for less than one second, and some stop/switch operations
took several seconds. The fault-injected matrix demonstrates bounded recovery
and runtime stability; audible gap characterization and exact stop/switch
latency across every possible live stream remain useful measurements.

Native Opus result `-502` (`0xfffffe0a`) is intermittent and live-packet-
dependent on some live variants. The same stream and build succeeded in another
run, and repeated `-502` attempts did not crash the app. Treat this as a
stream-data/recovery problem, not evidence that the station or CELT is
permanently unsupported.

Do not conflate that decoder result with Ogg parser error `-5`
(`OGG_OPUS_ERR_SEQUENCE`). A captured Icecast stream supplied `OpusHead` and
`OpusTags` at page sequences 0 and 1, then joined live audio at a much later
sequence. The corrected parser accepts exactly one jump between validated tags
and the first complete audio page, including bounded discard of an orphan
continuation. It continues to reject a second pre-audio jump and all later
sequence gaps. Host regressions passed multiple captured live streams while
preserving those strict rejections.

## HE-AAC result and boundary

A controlled probe decoded a 24-frame HE-AAC v2 ADTS fixture. Public 24-byte
parameters with HE enabled produced nonzero 48 kHz mono PCM with energy above
12 kHz, confirming SBR reconstruction. HE disabled produced the 24 kHz mono
AAC core, and a valid public 28-byte extended form also produced 48 kHz mono.
Every valid public HE-AAC v2 Parametric Stereo configuration exposed one
channel. Recovered AvPlayer-style word-size/configuration variants did not
provide a callable public two-channel PS path; the 16-bit word-size
configuration was rejected.

Production retains native AAC offload and the timing-safe AAC-core fallback for
PS-intended HLS, then normalizes or duplicates mono to the 48 kHz stereo
AudioOut contract. Do not claim native PS stereo on firmware 6.02. This is a
fidelity boundary, not evidence that AAC support is missing.

## Release asset smoke validation

The downloaded release asset was verified by upload and download round trips,
launched as a Game-category application, displayed the expected version,
played and switched live stations, reached MP3 at 48 kHz stereo, and closed
cleanly. Runtime monitoring confirmed normal resource release.

## Host validation

The full host suite passes:

- AAC timing and resampling;
- MP3 framing;
- PCM queue and retry policy;
- controller input;
- M3U/PLS playlist handling;
- HLS playlist parsing;
- MPEG-TS PAT/PMT/PES and ADTS AAC extraction;
- incremental Ogg Opus handling;
- bounded Vorbis decoding;
- native and Ogg-FLAC decoding;
- catalog JSON; and
- text/UI checks.

ASan/UBSan also pass for the retry, Vorbis, and FLAC checks. Keep host tests
separate from PS5 claims: they establish deterministic parser, framing, and
resource behavior, while only the device records above establish runtime
decoder and AudioOut behavior.

## Target checklist for future changes

For a new payload or recovery change, capture:

- loader, module load/unload, decoder initialization/creation/reset/destruction;
- first and repeated decode results, consumed input, produced PCM bytes, rate,
  and channels;
- AudioOut open, audible result, output errors, and terminal state;
- stop while connecting, blocked in a read, and holding queued PCM;
- direct switching across every enabled codec;
- reconnect, underrun, and automatic-recovery behavior; and
- exact ELF/package hashes, firmware, and toolchain versions.

Before expanding HLS beyond the current subset, require separate evidence for
any encryption, byte-range, fMP4/CMAF, LL-HLS, alternate-rendition,
multi-program, or non-AAC change.
