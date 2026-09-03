/*
 * ps5-audio-decoding-research - Optional AudioIn link-time stub.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Link-time declarations for SDKs that do not ship a libSceAudioIn stub.
 * The executable's module-linking step replaces these fallback bodies with
 * native imports; this shared provider must never be packaged with the app.
 */

#include <cstdint>

extern "C" {

std::int32_t sceAudioInOpen(std::int32_t, std::int32_t, std::int32_t,
                            std::uint32_t, std::uint32_t, std::uint32_t)
{
    return -1;
}

std::int32_t sceAudioInInput(std::int32_t, void*) { return -1; }

std::int32_t sceAudioInGetSilentState(std::int32_t) { return -1; }

std::int32_t sceAudioInClose(std::int32_t) { return -1; }

} // extern "C"
