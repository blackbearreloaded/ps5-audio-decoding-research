# PS5 Audio Decoding Research

[![License: GPL-3.0-or-later](https://img.shields.io/badge/license-GPL--3.0--or--later-blue.svg)](LICENSE)

Notes and device test records on compressed-audio decoding, PCM output, and
controller-microphone capture in applications written for the PS5.

The records come from an internet-radio test application and from small test
titles, all run on one console. The documents call its system software "the
tested system software".

## Results on the tested system software

| Area | Device result |
| --- | --- |
| AAC-LC (ADTS) | Decoded by the platform AAC decoder; signed-16 PCM reached the audio output |
| HE-AAC | SBR reconstruction observed (48 kHz mono with energy above 12 kHz); Parametric Stereo came out as one channel in every configuration tested |
| MP3 | Decoded by the platform MP3 decoder; a live 48 kHz stereo stream produced 4,608 signed-16 PCM bytes per frame and reached the audio output |
| Opus | Decoded by the platform Opus decoders; a 20 ms 48 kHz stereo packet produced 3,840 PCM bytes |
| Ogg Vorbis | Decoded in the application with [`stb_vorbis`](https://github.com/nothings/stb/blob/master/stb_vorbis.c); platform decoding was not tested conclusively |
| FLAC and Ogg-FLAC | Decoded in the application with [`dr_flac`](https://github.com/mackron/dr_libs/blob/master/dr_flac.h); an attempt to load a platform FLAC decoder failed |
| HLS | Unencrypted audio-only MPEG-TS carrying AAC-LC played through the platform AAC decoder |
| PCM output | 48 kHz stereo signed-16 blocks through the platform audio output |
| Controller microphone | 16 kHz mono signed-16 PCM captured from the controller's built-in microphone on three input purposes |

"Platform decoder" means the decoder supplied by the console's system
software, as opposed to a decoder compiled into the application. These tests
do not show where that decoding physically runs.

System software interfaces and behavior can change. The results apply to the
tested system software, streams, and title configuration only.

## Documentation

| Document | Purpose |
| --- | --- |
| [Compressed decoding](docs/DECODING.md) | Decoder routes, framing, buffering, and recovery behavior |
| [Audio output](docs/AUDIO-OUTPUT.md) | Output block format and queue behavior |
| [Audio input](docs/AUDIO-INPUT.md) | Controller microphone capture results |
| [Validation](docs/VALIDATION.md) | Device test record |
| [Release validation](docs/AUDIO_RELEASE_VALIDATION.md) | Decoder, hardening, and release record of the test application |
| [Vorbis validation](docs/VORBIS_VALIDATION.md) | Vorbis decoder limits and device record |
| [FLAC validation](docs/FLAC_VALIDATION.md) | FLAC decoder limits and device record |
| [HLS validation](docs/HLS_VALIDATION.md) | Supported HLS subset and device record |
| [Follow-up](FOLLOW-UP.md) | Remaining scope |
| [Third-party notices](THIRD_PARTY_NOTICES.md) | Credits |

## Evidence labels

- **Device result:** observed on the console and recorded in a log or test
  result.
- **Host result:** observed in a test built and run on a development PC.
- **Open:** not tested, or tested without a conclusive result.

## License and notices

Copyright © 2026 BlackBearReloaded. Licensed under GPL-3.0-or-later; see [LICENSE](LICENSE).

This is a personal hobby project. It is not affiliated with, endorsed by or sponsored by Sony Interactive Entertainment or any other company named here. Product names are trademarks of their owners and are used only to describe compatibility. "PlayStation", "PS5" and related marks are trademarks of Sony Interactive Entertainment Inc.

Provided as is, without warranty. Nothing here is legal advice.
