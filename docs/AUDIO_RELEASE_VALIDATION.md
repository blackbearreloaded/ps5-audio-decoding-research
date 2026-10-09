# Release validation

This is the release record for the internet-radio test application. Device
results were collected on the tested system software.

## Decoder routes

| Format or path | Result | Route in the application |
|---|---|---|
| AAC-LC ADTS | The platform decoder produces PCM for the audio output. | Network ADTS framing, platform AAC decoder, shared PCM/output path |
| MP3 | A live stream produced 4,608 bytes of signed-16 PCM per frame at 48 kHz stereo and reached the audio output; the cross-format matrix also played and stopped a live MP3 stream. | Platform MP3 decoder |
| Ogg Opus | SILK and hybrid packets decode through the general platform Opus decoder; CELT-only packets decode through the CELT-only one. | General decoder first; CELT-only packets retry once through the CELT-only decoder after a failure |
| Ogg Vorbis | Played through the in-application decoder. | `stb_vorbis` |
| FLAC / Ogg-FLAC | Played through the in-application decoder. An attempt to load a platform FLAC decoder failed. | `dr_flac` |
| HLS | Delivery and container handling, not a codec. | Unencrypted audio-only MPEG-TS/AAC-LC subset feeding the platform AAC decoder |

## HE-AAC result

A controlled test decoded a 24-frame HE-AAC v2 ADTS fixture.

- With HE-AAC enabled, the decoder produced nonzero 48 kHz mono PCM with
  energy above 12 kHz: SBR reconstruction works.
- With HE-AAC disabled, it produced the 24 kHz mono AAC core.
- Every configuration tested produced Parametric Stereo as one channel.

The application therefore plays such HLS streams from the mono AAC core and
duplicates mono to the 48 kHz stereo output.

## Stream and container hardening

The release includes:

- full Ogg page CRC validation before dispatch;
- bounded failure for truncated, inconsistent, and serial-invalid Ogg pages;
- Ogg logical-stream chaining, one validated header-to-first-audio sequence
  jump for Icecast live joins, and orphan continuation handling;
- Opus output gain, pre-skip, end-granule trimming, bounded padded packets, and
  clean decoder resets;
- decoder recreation for chained Vorbis streams;
- ICY metadata interval parsing and stripping before codec framing, including
  split blocks and malformed or truncated metadata;
- HLS discontinuities that drain old PCM, recreate the AAC decoder, clear ADTS
  framing, and reopen output with the new sample rate and channel count; and
- three bounded reconnect attempts with 250/500/1,000 ms backoff. The failure
  budget renews only after real output and 30 seconds of stable playback.

The shared playback path uses a cancellable HTTP request, a two-second
decoded-PCM ring, about one second of initial prime, about 0.5 seconds of
underrun re-prime, immediate PCM discard on stop or error, and decoder reset
without closing the output.

## Fault-injected device matrix

The run recorded 92 telemetry events, 16 status reads, and 12 rapid
cross-codec switches.

| Scenario | Observed result |
|---|---|
| AAC reconnect | Three forced server disconnects; each recovery reached 48 kHz stereo. |
| Opus underrun | Delayed delivery reached and kept 48 kHz stereo without a hang. |
| Malformed Ogg Opus | Exactly three retries, then an error, with no crash and a responsive UI. |
| HLS discontinuity | 44.1 kHz stereo to 48 kHz mono across three cycles, each with a reopened output. |
| ICY AAC | Metadata was stripped and 48 kHz stereo output was reached. |
| Chained Vorbis | 44.1 kHz stereo playback completed. |
| MP3 | 48 kHz stereo playback completed. |
| FLAC | 44.1 kHz stereo playback completed and the final state stopped cleanly. |

There was no title crash and no stuck title.

## Release checks

The local and CI runs passed lint, host regressions, and the console build and
packaging.

The downloaded release asset was copied to the console and launched. It
showed the expected version, played and switched live stations, reached MP3 at
48 kHz stereo, and closed cleanly.

## Limits

The results apply to the tested system software and streams. Encryption, byte
ranges, fMP4/CMAF, LL-HLS parts, alternate renditions, video or multi-program
variants, and non-AAC HLS are not implemented. Generic Radio Browser OGG
records are identified by stream signature and routed to Opus, Vorbis, or
Ogg-FLAC.
