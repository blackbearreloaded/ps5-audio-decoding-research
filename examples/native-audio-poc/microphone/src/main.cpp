/*
 * ps5-audio-decoding-research - Native controller-microphone proof of concept.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Build this source as the src/main.cpp of a project created from
 * ps5-native-app-boilerplate. It captures bounded user-routed AudioIn PCM and
 * writes an ordinary WAV file to the title's persistent download volume.
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <string_view>
#include <unistd.h>

extern "C" {

std::int32_t sceAudioInClose(std::int32_t handle);
std::int32_t sceAudioInGetSilentState(std::int32_t handle);
std::int32_t sceAudioInInput(std::int32_t handle, void* destination);
std::int32_t sceAudioInOpen(std::int32_t user_id, std::int32_t type,
                            std::int32_t index, std::uint32_t grain,
                            std::uint32_t frequency, std::uint32_t format);
std::int32_t sceKernelSendNotificationRequest(
    std::uint32_t device, void* request, std::size_t size,
    std::int32_t blocking);
std::int32_t sceKernelUsleep(std::uint32_t microseconds);
std::int32_t sceUserServiceGetInitialUser(std::int32_t* user_id);
std::int32_t sceUserServiceInitialize(void* parameters);
std::int32_t sceUserServiceTerminate();

}

namespace {

constexpr std::string_view kWavPath = "/download0/dualsense-microphone.wav";
constexpr std::string_view kLogPath =
    "/download0/dualsense-microphone.log";
constexpr std::uint32_t kSampleRate = 16'000;
constexpr std::uint32_t kFramesPerBlock = 256;
constexpr std::uint32_t kCaptureSeconds = 10;
constexpr std::uint32_t kBlockCount =
    kSampleRate * kCaptureSeconds / kFramesPerBlock;
constexpr std::int32_t kGeneralInput = 1;
constexpr std::uint32_t kSigned16Mono = 0;

struct NotificationRequest {
    std::array<std::uint8_t, 45> reserved{};
    std::array<char, 3075> message{};
};

#pragma pack(push, 1)
struct WavHeader {
    std::array<char, 4> riff{'R', 'I', 'F', 'F'};
    std::uint32_t riff_size{};
    std::array<char, 4> wave{'W', 'A', 'V', 'E'};
    std::array<char, 4> fmt{'f', 'm', 't', ' '};
    std::uint32_t fmt_size{16};
    std::uint16_t encoding{1};
    std::uint16_t channels{1};
    std::uint32_t sample_rate{kSampleRate};
    std::uint32_t bytes_per_second{kSampleRate * sizeof(std::int16_t)};
    std::uint16_t block_alignment{sizeof(std::int16_t)};
    std::uint16_t bits_per_sample{16};
    std::array<char, 4> data{'d', 'a', 't', 'a'};
    std::uint32_t data_size{};
};
#pragma pack(pop)

static_assert(sizeof(WavHeader) == 44);

struct Result {
    std::int32_t user_service{-1};
    std::int32_t initial_user{-1};
    std::int32_t user_id{-1};
    std::int32_t audio_open{-1};
    std::int32_t error{};
    std::uint32_t blocks{};
    std::uint32_t bytes{};
    std::uint32_t nonzero_samples{};
    std::uint32_t active_blocks{};
    std::uint32_t silent_blocks{};
    std::uint32_t silent_mask{};
    std::int32_t peak{};
};

NotificationRequest notification{};

void notify(const std::string_view message) noexcept
{
    const auto count = message.size() < notification.message.size() - 1
                           ? message.size()
                           : notification.message.size() - 1;
    for (std::size_t index = 0; index < count; ++index) {
        notification.message[index] = message[index];
    }
    notification.message[count] = '\0';
    static_cast<void>(sceKernelSendNotificationRequest(
        0, &notification, sizeof(notification), 0));
}

[[nodiscard]] bool write_all(const int descriptor, const void* source,
                             const std::size_t size) noexcept
{
    const auto* bytes = static_cast<const std::uint8_t*>(source);
    std::size_t written = 0;
    while (written < size) {
        const auto count = write(descriptor, bytes + written, size - written);
        if (count <= 0) {
            return false;
        }
        written += static_cast<std::size_t>(count);
    }
    return true;
}

void update_statistics(Result& result,
                       const std::array<std::int16_t, kFramesPerBlock>& block)
    noexcept
{
    for (const auto sample : block) {
        const auto value = static_cast<std::int32_t>(sample);
        const auto magnitude = value < 0 ? -value : value;
        if (magnitude != 0) {
            ++result.nonzero_samples;
        }
        if (magnitude > result.peak) {
            result.peak = magnitude;
        }
    }
}

void capture(Result& result) noexcept
{
    result.user_service = sceUserServiceInitialize(nullptr);
    result.initial_user = sceUserServiceGetInitialUser(&result.user_id);
    if (result.initial_user < 0) {
        result.error = result.initial_user;
        return;
    }

    result.audio_open =
        sceAudioInOpen(result.user_id, kGeneralInput, 0, kFramesPerBlock,
                       kSampleRate, kSigned16Mono);
    if (result.audio_open < 0) {
        result.error = result.audio_open;
        return;
    }

    const int descriptor =
        open(kWavPath.data(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (descriptor < 0) {
        result.error = descriptor;
        static_cast<void>(sceAudioInClose(result.audio_open));
        return;
    }

    WavHeader header{};
    if (!write_all(descriptor, &header, sizeof(header))) {
        result.error = -1;
    } else {
        notify("DualSense microphone: unmute and speak for 10 seconds");
        std::array<std::int16_t, kFramesPerBlock> block{};
        for (; result.blocks < kBlockCount; ++result.blocks) {
            const auto input_result =
                sceAudioInInput(result.audio_open, block.data());
            if (input_result < 0) {
                result.error = input_result;
                break;
            }
            const auto silent = sceAudioInGetSilentState(result.audio_open);
            if (silent >= 0) {
                result.silent_mask |= static_cast<std::uint32_t>(silent);
                if (silent == 0) {
                    ++result.active_blocks;
                } else {
                    ++result.silent_blocks;
                }
            }
            update_statistics(result, block);
            if (!write_all(descriptor, block.data(), sizeof(block))) {
                result.error = -1;
                break;
            }
            result.bytes += sizeof(block);
        }
    }

    header.data_size = result.bytes;
    header.riff_size = 36 + result.bytes;
    if (lseek(descriptor, 0, SEEK_SET) >= 0) {
        static_cast<void>(write_all(descriptor, &header, sizeof(header)));
    }
    static_cast<void>(close(descriptor));
    static_cast<void>(sceAudioInClose(result.audio_open));
}

void write_log(const Result& result) noexcept
{
    std::array<char, 384> line{};
    const int length = std::snprintf(
        line.data(), line.size(),
        "userService=%d initialUser=%d userId=%d audioOpen=%d error=%d "
        "blocks=%u bytes=%u peak=%d nonzero=%u activeBlocks=%u "
        "silentBlocks=%u silentMask=0x%08x\n",
        result.user_service, result.initial_user, result.user_id,
        result.audio_open, result.error, result.blocks, result.bytes,
        result.peak, result.nonzero_samples, result.active_blocks,
        result.silent_blocks, result.silent_mask);
    if (length <= 0) {
        return;
    }
    const int descriptor =
        open(kLogPath.data(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (descriptor >= 0) {
        const auto size = static_cast<std::size_t>(length) < line.size()
                              ? static_cast<std::size_t>(length)
                              : line.size() - 1;
        static_cast<void>(write_all(descriptor, line.data(), size));
        static_cast<void>(close(descriptor));
    }
}

} // namespace

int main()
{
    notify("Power on DualSense; orange mic light must be OFF; speak soon");
    static_cast<void>(sceKernelUsleep(2'000'000));
    Result result;
    capture(result);
    write_log(result);
    if (result.error == 0 && result.peak > 0) {
        notify("DualSense microphone captured; retrieve WAV with UFS2Tool");
    } else if (result.error == 0) {
        notify("Microphone capture completed but samples were silent");
    } else {
        notify("Microphone probe failed; retrieve the log with UFS2Tool");
    }
    if (result.user_service == 0) {
        static_cast<void>(sceUserServiceTerminate());
    }
    for (;;) {
        static_cast<void>(sceKernelUsleep(1'000'000));
    }
}
