# Compressed audio decoding

This page describes the decoder routes used by the test application and what
was observed on the console. In every route the decoder returns PCM; the
application still resamples, mixes, and submits the PCM to the output (see
[Audio output](AUDIO-OUTPUT.md)).

| Format | Route |
|---|---|
| AAC-LC (ADTS) | Platform AAC decoder |
| MP3 | Platform MP3 decoder |
| Opus | Platform Opus decoders (general and CELT-only) |
| Ogg Vorbis | `stb_vorbis`, compiled into the application |
| FLAC, Ogg-FLAC | `dr_flac`, compiled into the application |

"Platform" means the decoder supplied by the console's system software. The
tests do not show where that decoding physically runs.

## MP3

An MP3 frame produces up to 1152 samples per channel, so it does not match a
256-frame output block. Accumulate PCM and submit complete blocks. Read the
sample rate and channel count from the validated frame header.

Device result: a live 48 kHz stereo stream produced 4,608 signed-16 PCM bytes
per frame and reached the audio output.

## AAC

Network AAC arrives as ADTS. After a successful decode the decoder reports the
stream's sample rate and channel count.

Device result: ADTS frames became signed-16 PCM and reached the audio output.
Switching from AAC to Opus and back did not crash, and stopping AAC playback
was immediate.

### Streaming ADTS

Do not assume one network read equals one AAC frame. Keep a byte buffer and:

1. Search for an ADTS sync word.
2. Parse the 13-bit frame length from the header.
3. Read until the complete frame is buffered.
4. Give the decoder exactly that frame.
5. Remove only the consumed bytes; keep the next partial frame.
6. Append the decoded PCM to an output accumulator.

Cap the amount of data scanned without a valid sync word, so a source that is
not AAC cannot cause an unbounded buffer walk.

### HE-AAC

A controlled test decoded a 24-frame HE-AAC v2 ADTS fixture:

- with HE-AAC enabled, the decoder produced nonzero 48 kHz mono PCM with
  energy above 12 kHz, which shows SBR reconstruction;
- with HE-AAC disabled, it produced the 24 kHz mono AAC core;
- every configuration tested produced Parametric Stereo as one channel.

The test application therefore duplicates mono to stereo for such streams. For
internet radio with inconsistent metadata it decodes one frame with HE-AAC
enabled, compares the stream signaling with the reported rate and channels,
and recreates the decoder with HE-AAC disabled when the stream should be
treated as ordinary AAC.

### Seeking and reset

After a seek, clear the decoder context or recreate the decoder, so samples
from the old position do not affect the new one.

## Opus

Ogg is a container: an `.opus` file or stream must be parsed into raw packets.
`OpusHead` and `OpusTags` are metadata and are not sent to the decoder.

The test application parses Ogg incrementally with a bounded demuxer. A
chained Ogg serial change closes and reopens the decoder.

Live Icecast joins can deliver fresh `OpusHead` and `OpusTags` pages with
sequence numbers 0 and 1, then continue at a much higher audio-page sequence
number. The application allows exactly one such jump between the tags and the
first complete audio page, and discards an orphan continued packet at that
point. A second jump before audio, or any gap after audio starts, is an error.

Decoder selection uses the Opus TOC byte
([RFC 6716 section 3.1](https://www.rfc-editor.org/rfc/rfc6716.html#section-3.1)):

```text
config = packet[0] >> 3
config 0..15   SILK and hybrid   -> general Opus decoder
config 16..31  CELT-only         -> general decoder first, CELT-only decoder on retry
```

The application tries the general decoder first. If decoding fails and the
packet is CELT-only, it retries once through the CELT-only decoder.

Device results:

- a 20 ms, 48 kHz stereo packet produced 3,840 signed-16 PCM bytes;
- a decoder stopped in 67 ms and playback switched to a second 48 kHz stereo
  stream without a crash;
- a live session ran for ten minutes;
- decode failures on live streams were intermittent: the same stream and
  build succeeded in another run, and repeated failures did not crash the
  application.

## Ogg Vorbis

The test application decodes Vorbis itself with the single-file
[`stb_vorbis.c`](https://github.com/nothings/stb/blob/master/stb_vorbis.c)
push-data API. Limits and the device record are in
[VORBIS_VALIDATION.md](VORBIS_VALIDATION.md).

## FLAC and Ogg-FLAC

The test application decodes FLAC itself with
[`dr_flac.h`](https://github.com/mackron/dr_libs/blob/master/dr_flac.h) and
callback reads. Limits and the device record are in
[FLAC_VALIDATION.md](FLAC_VALIDATION.md).

## HLS

HLS delivers playlists and media segments; it is not a decoder. The test
application accepts unencrypted, audio-only MPEG-TS carrying AAC-LC and gives
complete ADTS frames to the platform AAC decoder. See
[HLS_VALIDATION.md](HLS_VALIDATION.md).

A generic Radio Browser `OGG` record is ambiguous. The application inspects
the resolved stream's signature and routes it to Opus, Vorbis, or Ogg-FLAC; an
unknown Ogg payload fails before reaching a decoder.

## Buffering and cancellation

The playback worker separates network and decode work from output pacing:

- decoded PCM is held in a bounded two-second ring;
- startup primes about one second before output begins;
- an underrun re-primes about 0.5 seconds;
- stop and station switch abort the active HTTP request;
- each operation gets three consecutive attempts with 250, 500, and 1,000 ms
  backoff; the budget is renewed only after real output progress and at least
  30 seconds of playback;
- stop or error discards queued PCM immediately; and
- a decoder reset does not close the output.

This keeps a blocked network read or a blocking output call off the UI thread.
