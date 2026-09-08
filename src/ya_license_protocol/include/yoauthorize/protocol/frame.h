#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace yoauthorize::protocol {

inline constexpr std::size_t kFrameHeaderSize = 20;
inline constexpr std::uint16_t kFrameVersion = 1;
inline constexpr std::uint16_t kEncryptedFlag = 0x0001;
inline constexpr std::uint32_t kDefaultMaxPayloadSize = 1024 * 1024;

struct FrameHeader {
  std::uint16_t version = kFrameVersion;
  std::uint16_t flags = 0;
  std::uint64_t sequence = 0;
  std::uint32_t payload_size = 0;

  bool operator==(const FrameHeader&) const = default;
};

enum class FrameError {
  None,
  InsufficientData,
  InvalidMagic,
  UnsupportedVersion,
  UnknownFlags,
  EmptyPayload,
  PayloadTooLarge,
  LengthMismatch,
};

struct FrameDecodeResult {
  FrameError error = FrameError::None;
  FrameHeader header{};
  std::span<const std::byte> payload{};

  explicit operator bool() const { return error == FrameError::None; }
};

std::array<std::byte, kFrameHeaderSize> encodeHeader(const FrameHeader& header);

FrameDecodeResult decodeHeader(
    std::span<const std::byte> bytes,
    std::uint32_t max_payload_size = kDefaultMaxPayloadSize);

FrameDecodeResult decodeFrame(
    std::span<const std::byte> bytes,
    std::uint32_t max_payload_size = kDefaultMaxPayloadSize);

}  // namespace yoauthorize::protocol
