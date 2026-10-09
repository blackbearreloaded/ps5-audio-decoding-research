# Device validation

This is the device test record for the internet-radio test application and
the small test titles. Everything below was observed on the console, on the
tested system software, unless it is marked as a host result. The release
record is in [AUDIO_RELEASE_VALIDATION.md](AUDIO_RELEASE_VALIDATION.md).

"Platform decoder" means the decoder supplied by the console's system
software. The tests do not show where that decoding physically runs.

## Decoder and playback results

| Scenario | Result |
|---|---|
| AAC | The platform decoder handled ADTS and produced signed-16 PCM that reached the audio output. |
| MP3 | The platform decoder turned a live 48 kHz stereo stream into 4,608 signed-16 PCM bytes per frame; the audio output was reached. |
| Opus packet | A 20 ms, 48 kHz stereo packet produced 3,840 signed-16 PCM bytes with nonzero energy. |
| Opus lifecycle | Playback stopped in 67 ms, then switched to a second 48 kHz stereo stream without a crash. |
| Opus soak | A live session ran for ten minutes and stopped cleanly. |
| Opus CELT-only packets | Routing by TOC selected the CELT-only decoder, repeated packets decoded to PCM, and the title did not crash. |
| Opus errors | Repeated decode failures were handled without an application crash. |
| Codec transitions | AAC to Opus and back completed without a crash. |
| Cross-format playback | HLS/AAC, MP3, Opus, and Vorbis each played and stopped; Vorbis then switched directly back to HLS/AAC and stopped. |
| FLAC | A 44.1 kHz stereo stream played, stopped, and switched to AAC. |
| Platform FLAC decoder | An attempt to load one failed. |
| Ogg-FLAC soak | Playback stayed active at 44.1 kHz stereo for more than eleven minutes without an error or rebuffer. |
| Vorbis soak | A 44.1 kHz stereo stream played for about eleven minutes without a rebuffer or error. |
| HE-AAC | With HE-AAC enabled: nonzero 48 kHz mono PCM with energy above 12 kHz. Disabled: the 24 kHz mono AAC core. Parametric Stereo came out as one channel in every configuration tested. |
| Fault injection | Reconnect, underrun, malformed Ogg, HLS discontinuity, ICY metadata, chained Vorbis, MP3, FLAC, and rapid switching completed without a title crash or a stuck title. |
| Local container playback | Opening a local source through the platform media player failed identically for Vorbis/WebM, Opus/WebM, and an AAC/M4A control, so this test says nothing about codec support. |
| Controller microphone | Two runs captured valid 16 kHz signed-16 mono PCM on the voice chat, general, and voice recognition purposes. |

## Controller microphone

The test title opened the platform audio input for the initial user, read
256-frame blocks, and wrote WAV files. The controller was connected directly
to the console.

In the first run, each of the three purposes produced exactly 128,000 PCM data
bytes: four seconds of 16 kHz mono signed-16 audio. Peak amplitudes were
31,989, 30,354, and 11,830. No silence was indicated. A second run again
produced nonzero PCM on all three and showed brief silence indications at
start or end without an error.

The controller must be powered on and its orange microphone-mute light must be
off. With the controller off, the input still opened and returned correctly
timed zero-filled blocks, so a successful open does not prove an active
microphone. Applications remain responsible for visible recording controls,
consent, and lawful handling of captured audio.

## Live behavior and retry policy

The test application uses:

- a bounded two-second decoded-PCM ring;
- about one second of initial prime and about 0.5 seconds of underrun
  re-prime;
- a cancellable HTTP request, including on stop and station switch;
- immediate PCM discard on stop or error;
- decoder reset without closing the output; and
- three consecutive attempts with 250/500/1,000 ms backoff.

The retry budget is renewed only after real output progress and at least 30
seconds of playback. The release matrix exercised three forced AAC reconnects,
delayed Opus delivery, malformed Ogg retry exhaustion, rapid switching, ICY
metadata stripping, and three live HLS discontinuity cycles.

Some streams paused for less than one second, and some stop/switch operations
took several seconds. Audible gaps and exact stop/switch latency were not
measured.

Opus decode failures were intermittent and depended on the live packets: the
same stream and build succeeded in another run. Do not confuse them with the
application's own Ogg sequence error. A captured Icecast stream supplied
`OpusHead` and `OpusTags` at page sequences 0 and 1, then joined live audio at
a much later sequence. The parser accepts exactly one such jump between the
tags and the first complete audio page and rejects a second jump or any later
gap.

## Release asset check

The downloaded release asset was copied to the console, launched, showed the
expected version, played and switched live stations, reached MP3 at 48 kHz
stereo, and closed cleanly.

## Host results

The host test suite passes:

- AAC timing and resampling;
- MP3 framing;
- PCM queue and retry policy;
- controller input;
- M3U/PLS playlist handling;
- HLS playlist parsing;
- MPEG-TS PAT/PMT/PES and ADTS AAC extraction;
- incremental Ogg Opus handling;
- bounded Vorbis decoding;
- FLAC and Ogg-FLAC decoding;
- catalog JSON; and
- text/UI checks.

ASan/UBSan also pass for the retry, Vorbis, and FLAC checks. Host tests show
parser, framing, and resource behavior only; decoder and output behavior on
the console comes from the device results above.

## Checklist for future changes

For a new decoder or recovery change, record:

- decoder creation, reset, and destruction results;
- first and repeated decode results, consumed input, produced PCM bytes, rate,
  and channels;
- output open, audible result, output errors, and final state;
- stop while connecting, while waiting in a read, and while holding queued PCM;
- direct switching across every enabled codec;
- reconnect, underrun, and automatic-recovery behavior; and
- build hashes, system software version, and toolchain versions.

For microphone changes, also record the input purpose, rate, block size,
format, per-block silence indication, nonzero sample count, peak level, and
controller power/mute state.
