# AvPlayer and container playback

Use AvPlayer when the application wants the platform to read a local media container and return decoded
frames. Use the codec-specific native decoders when the application already
owns the compressed frames or network protocol.

## Observed local container path

The local-file path accepts an absolute path and has been investigated for:

- `.mp4`
- `.m4v`
- `.m4a`
- `.mov`
- `.webm`

The application supplies memory allocators, adds the source, waits for stream
discovery, enables the selected streams, and starts playback. This is separate
from AvPlayer's network-source initialization path.

## Native C++20 playback shape

A boilerplate-aligned implementation should wrap the AvPlayer handle, its
allocator callbacks, the loaded module, and the `AudioOut` handle in C++20
owners. Poll audio frames on a worker thread, immediately copy or submit the
AvPlayer-owned PCM, normalize channels and sample rate, and let the shared
`AudioOut` sink accumulate 256-frame stereo blocks. No standalone AvPlayer
overlay is published here because the recovered source probe could not
discriminate container/codec support; PSRadio remains the target-validated
network playback reference.

## Lifecycle and stream selection

The high-level sequence is:

```text
sceSysmoduleLoadModule(AvPlayer)
    -> sceAvPlayerInit(allocators)
    -> sceAvPlayerAddSource(path)
    -> sceAvPlayerStart()
        -> wait for stream count
        -> sceAvPlayerGetStreamInfo for each stream
        -> sceAvPlayerEnableStream for selected streams
    -> sceAvPlayerGetAudioData / sceAvPlayerGetVideoDataEx
    -> stop / pause / resume / jump / looping calls
    -> sceAvPlayerClose
    -> sceSysmoduleUnloadModule(AvPlayer)
```

The frame data points into AvPlayer-owned memory and stays valid only until the
next frame request. Copy it or submit it immediately. Audio frame details
report timestamp, sample rate, channel count, and interleaved signed-16 PCM.

If a file contains multiple language or angle streams, enumerate the native
stream information and enable the desired stream explicitly.

## Where codec selection happens

AvPlayer's recovered factory contains these branches:

```text
Create AudioDecHw(AAC)
Create AudioDecSw(AAC)
Create AudioDecHw(Opus)
Create AudioDecHw(AC3)
Create AudioDecSw(Eac3)
```

Therefore AvPlayer is the most convenient route when the container is more important than direct codec
control, and it is also the static proof that the platform maintains separate hardware/software decoder
objects. The observed public path does not expose a “force hardware” selector.

The recovered AvPlayer ABI was initialized successfully in the firmware 6.02
source probes, but `sceAvPlayerAddSource` rejected Vorbis/WebM, Opus/WebM, and
an AAC/M4A positive control identically with `-2140536829`
(`0x806a0003`) at `stage=source`. This path therefore cannot discriminate
codec support. Do not use those failures to claim that AvPlayer lacks Vorbis;
the “no callable native Vorbis route found” conclusion comes from static
decoder-factory and AJM evidence instead.

## Audio/video synchronization

AvPlayer provides timestamps for both audio and video. If the application renders video itself, use the
timestamps to decide when to display a video frame while the audio output queue supplies the clock. Do
not busy-spin on `TryGetAudioFrame`; yield or sleep briefly when no frame is ready.

## Local file limitation

The local path uses AvPlayer's own file reader. It is not a URL/network API. For
network radio or Moonlight, own the transport and use a codec-specific decoder or an external CPU codec.
