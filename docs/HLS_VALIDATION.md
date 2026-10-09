# HLS validation and scope

HLS is a delivery protocol, not an audio codec. The test application's
implementation is deliberately narrow: unencrypted, audio-only MPEG-TS
carrying one AAC-LC ADTS elementary stream, decoded by the platform AAC
decoder.

The protocol reference is [RFC 8216](https://www.rfc-editor.org/rfc/rfc8216.html).

## Supported subset

| Layer | Supported behavior |
|---|---|
| Playlist | Bounded master/media playlists and relative URLs |
| Variant choice | Lowest-bandwidth audio-only variant |
| Live behavior | Media reload, stale-segment suppression, sequence tracking, bounded segment retries |
| Timeline | Discontinuity reset |
| Segment container | MPEG-TS (PAT/PMT/PES parsing) |
| Elementary stream | One audio-only AAC-LC ADTS stream |
| Decoder | Platform AAC decoder |
| Output | Shared channel normalization, 48 kHz resampling, PCM ring, audio output |

## Rejected before decoding

- encryption;
- byte ranges;
- fMP4/CMAF initialization maps;
- LL-HLS parts and preload hints;
- alternate rendition groups;
- video variants and video elementary streams;
- multi-program transport streams; and
- non-AAC audio.

These are product decisions for this implementation.

## MPEG-TS regression

A captured live segment once made the AAC decoder fail. The cause was
transport assembly: a repeated PMT arrived while an AAC PES was active, and
resetting PES state for every PMT cut the AAC access unit short. The parser
now resets PES state only when the announced AAC PID changes. The host
extraction is byte-identical to FFmpeg; the captured output SHA-256 is
`B24AD0B0F398E2E73E2B296C4B3E5F1C15A72471D5C36DD7753E530226D64FC8`.

This was a parser fault, not a decoder limit.

## Device results

HLS played across playlist reloads, stopped, restarted, stayed active for the
final observation, and closed normally. A cross-format test then played and
stopped HLS/AAC using an `mp4a.40.29` stream, played from the 24 kHz mono AAC
core and normalized to 48 kHz stereo (see the HE-AAC result in
[AUDIO_RELEASE_VALIDATION.md](AUDIO_RELEASE_VALIDATION.md)).

The same matrix then played MP3, Opus, and Vorbis, switched Vorbis directly
back to HLS/AAC, and stopped.

The fault-injected matrix completed three live discontinuity cycles, going
from 44.1 kHz stereo to 48 kHz mono with a reopened output each time. It also
covered three forced AAC reconnects, delayed delivery, and rapid cross-codec
switching without a title crash or a stuck title.
