/*
 * ps5-audio-decoding-research - Native Opus hardware/offload proof of concept.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Build this source as the src/main.cpp of a project created from
 * ps5-native-app-boilerplate. The application reads one raw Opus packet,
 * decodes it through libSceOpusDec, and feeds signed-16 PCM to AudioOut.
 */

#include "native_audio.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <span>

namespace
{
constexpr std::uint32_t kOpusModule = 0x80000069;
constexpr int kSampleRate = 48000;
constexpr int kChannels = 2;
constexpr std::size_t kPacketCapacity = 8192;
constexpr std::size_t kMaximumFrameSamples = 5760;
constexpr std::size_t kStateAlignment = 64;

extern "C"
{
    int sceSysmoduleLoadModuleInternal(std::uint32_t module_id);
    int sceSysmoduleUnloadModuleInternal(std::uint32_t module_id);
    int sceOpusDecInitialize(std::uint32_t *context);
    int sceOpusDecTerminate(std::uint32_t *context);
    int sceOpusDecGetSize(int channels);
    int sceOpusDecCreateEx(std::uint32_t *context, void *state, int sample_rate, int channels);
    int sceOpusDecDecode(void *state, const std::uint8_t *packet, int packet_bytes,
                         std::int16_t *pcm, int pcm_capacity_bytes);
    int sceOpusDecDestroy(void *state);
}

class OpusDecoder final
{
  public:
    OpusDecoder() noexcept = default;
    OpusDecoder(const OpusDecoder &) = delete;
    OpusDecoder &operator=(const OpusDecoder &) = delete;

    ~OpusDecoder() noexcept
    {
        close();
    }

    bool open() noexcept
    {
        if (sceSysmoduleLoadModuleInternal(kOpusModule) < 0)
        {
            return false;
        }
        module_loaded_ = true;

        const int state_size = sceOpusDecGetSize(kChannels);
        if (state_size <= 0)
        {
            return false;
        }
        allocation_ = std::malloc(static_cast<std::size_t>(state_size) + kStateAlignment - 1);
        if (allocation_ == nullptr)
        {
            return false;
        }
        const auto address = reinterpret_cast<std::uintptr_t>(allocation_);
        state_ = reinterpret_cast<void *>((address + kStateAlignment - 1) & ~(kStateAlignment - 1));

        if (sceOpusDecInitialize(&context_) < 0)
        {
            return false;
        }
        initialized_ = true;
        if (sceOpusDecCreateEx(&context_, state_, kSampleRate, kChannels) < 0)
        {
            return false;
        }
        created_ = true;
        return true;
    }

    int decode(std::span<const std::uint8_t> packet, std::span<std::int16_t> pcm) noexcept
    {
        constexpr std::size_t kMaxInt = static_cast<std::size_t>(std::numeric_limits<int>::max());
        if (!created_ || packet.empty() || packet.size() > kMaxInt || pcm.size_bytes() > kMaxInt)
        {
            return -1;
        }
        return sceOpusDecDecode(state_, packet.data(), static_cast<int>(packet.size()), pcm.data(),
                                static_cast<int>(pcm.size_bytes()));
    }

  private:
    void close() noexcept
    {
        if (created_)
        {
            sceOpusDecDestroy(state_);
            created_ = false;
        }
        if (initialized_)
        {
            sceOpusDecTerminate(&context_);
            initialized_ = false;
        }
        std::free(allocation_);
        allocation_ = nullptr;
        state_ = nullptr;
        if (module_loaded_)
        {
            sceSysmoduleUnloadModuleInternal(kOpusModule);
            module_loaded_ = false;
        }
    }

    std::uint32_t context_ = 0;
    void *allocation_ = nullptr;
    void *state_ = nullptr;
    bool module_loaded_ = false;
    bool initialized_ = false;
    bool created_ = false;
};
} // namespace

int main() noexcept
{
    std::array<std::uint8_t, kPacketCapacity> packet{};
    const std::size_t packet_size =
        ps5::native_audio::read_file("/data/opus-hw-poc.packet", std::span{packet});
    if (packet_size == 0)
    {
        return 1;
    }

    OpusDecoder decoder;
    if (!decoder.open())
    {
        return 2;
    }

    std::array<std::int16_t, kMaximumFrameSamples * kChannels> pcm{};
    const int produced = decoder.decode(std::span{packet}.first(packet_size), std::span{pcm});
    if (produced <= 0 || (produced & 1) != 0 || static_cast<std::size_t>(produced) > sizeof(pcm))
    {
        return 3;
    }

    ps5::native_audio::AudioOut audio;
    if (!audio.open() ||
        !audio.push(std::span{pcm}.first(static_cast<std::size_t>(produced) / sizeof(std::int16_t)),
                    kChannels) ||
        !audio.flush())
    {
        return 4;
    }
    return 0;
}
