# HLS validation and scope

HLS is a delivery protocol and container/segment workflow, not an audio
codec. The first PSRadio slice is deliberately narrow: unencrypted,
audio-only MPEG-TS carrying one AAC-LC ADTS elementary stream, submitted to the
validated native `libSceAudiodec` codec `3` path.

## Reused implementation surface

The existing `workspace/dev/ps5-iptv-client` HLS parser is the companion
reference. Its bounded design provides:

- master and media playlist parsing;
- relative URL resolution;
- lowest-bandwidth audio-only variant selection;
- live reload and media-sequence tracking;
- bounded segment retries;
- stale-segment suppression;
- discontinuity handling;
- MPEG-TS PAT/PMT/PES parsing; and
- ADTS AAC extraction.

PSRadio reuses those design boundaries while sending complete ADTS frames to
its native AAC decoder and shared PCM/AudioOut path. HLS does not require or
introduce a separate decoder.

The protocol reference is [RFC 8216](https://www.rfc-editor.org/rfc/rfc8216.html).

## Supported baseline

| Layer | Supported behavior |
|---|---|
| Playlist | Bounded master/media playlists and relative URLs |
| Live behavior | Media reload, stale-segment suppression, and sequence tracking |
| Timeline | Discontinuity reset |
| Segment container | MPEG-TS |
| Elementary stream | One audio-only AAC-LC ADTS stream |
| Decoder | Native `libSceAudiodec` codec `3` / AJM-backed native path |
| Output | Shared channel normalization, 48 kHz resampling, PCM ring, AudioOut |

## Explicitly rejected

The first slice rejects the following before decoder dispatch:

- encryption;
- byte ranges;
- fMP4/CMAF initialization maps;
- LL-HLS parts and preload hints;
- alternate rendition groups;
- video variants and video elementary streams;
- multiprogram transport streams; and
- non-AAC audio.

These are bounded product decisions, not claims that the PS5 cannot support
the formats or delivery features in another implementation.

## MPEG-TS regression boundary

A captured live segment once returned the generic native AudioDec failure
`0x807f0000`. The root cause was transport assembly: a repeated PMT arrived
while an AAC PES was active, and resetting PES state for every PMT truncated the
AAC access unit. The parser now resets PES only when the announced AAC PID
changes. The host extraction is byte-identical to FFmpeg; the captured output
SHA-256 is
`B24AD0B0F398E2E73E2B296C4B3E5F1C15A72471D5C36DD7753E530226D64FC8`.

This is a container/parser caveat, not evidence of an AAC decoder limitation.

## Device evidence and caveats

Device testing played HLS across playlist reloads, stopped, restarted,
remained active for the final observation, and released runtime layers on
normal close. A cross-format test then played and stopped HLS/AAC using a
tested `mp4a.40.29` stream. The stream used a timing-safe 24 kHz mono AAC-core
fallback normalized to 48 kHz stereo. A separate probe confirmed native SBR
reconstruction with HE enabled, but valid public HE-AAC v2 Parametric Stereo
configurations exposed one channel; native public PS stereo is not established
on firmware 6.02.

The same production matrix then played MP3, Opus, and Vorbis, switched Vorbis
directly back to HLS/AAC, and stopped.

The fault-injected matrix completed three hardware live-discontinuity cycles, transitioning
from 44.1 kHz stereo to 48 kHz mono with fresh output geometry. The same
fault-injected matrix covered three forced AAC reconnects, delayed delivery,
and rapid cross-codec switching without a title crash or stuck runtime layer.
Malformed playlist/segment shapes and broader HLS features remain outside this
bounded slice; they do not represent missing AAC or HLS baseline support.
