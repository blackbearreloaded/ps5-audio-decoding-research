# Controller-driven audio applications

The supplied application threads establish that full joystick/controller input is working in two apps.
That is valuable integration context, but it is a separate subsystem from audio.

## Working applications

- PSRadio native/RmlUi source: `workspace/dev/psradio`
- PS5 Radio Browser source: `workspace/dev/ps5-radio-browser`
- [Zero-Copy-GPU-Decoding task](codex://threads/01a0272d-fb7e-7981-89fa-1f50a3f5d9df)
- [Radio-App-RmlUi task](codex://threads/01a02cc0-e8bc-7b91-9b6a-ecc20035b294)
- Controller input investigation: `workspace/dev/ps5-input-investigation/README.md`

The two radio applications share the useful native input adapter pattern in `src/radio_input.c`:

1. open a pad handle;
2. call `scePadRead` with a capacity greater than one;
3. process every returned sample for button edges;
4. use the final sample as the current state;
5. turn edges into application actions.

The input investigation recommends `scePadRead` over `scePadReadState` for latency-sensitive code because
one read can return multiple timestamped reports. `scePadReadState` is convenient when only the latest
state matters.

## Recommended audio integration

Do not let a controller callback perform decode or blocking output directly. Convert input into commands:

```text
scePadRead / SDL input
        -> edge events: Play, Pause, Next, VolumeUp, VolumeDown
        -> playback state/command queue
        -> network/decode/audio thread
        -> AudioOut output
```

For PSRadio, the UI maps controller actions to station selection, favorite, refresh, and playback. A
similar media player can map Cross to play/pause, Circle to back, L1/R1 to sections, and D-pad/analog
movement to navigation without coupling those actions to the decoder internals.

## Shared-state rules

- Keep the decoder and AudioOut handle owned by the playback thread.
- The controller thread writes commands or atomically updates desired state.
- The playback thread acknowledges state changes after it has applied them.
- Keep volume as a target value and apply it at a safe output boundary.
- On stop, signal the playback thread, drain/close AudioOut, then destroy the decoder.
- On a stream switch, stop the old decoder, clear/reset state, and only then start the new stream.

This arrangement is especially important for AAC because `sceAudiodecDecode` and `sceAudioOutOutput`
may both block or depend on service-owned state.
