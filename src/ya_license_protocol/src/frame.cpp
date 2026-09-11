#include "yoauthorize/protocol/frame.h"

#include <algorithm>

namespace yoauthorize::protocol {
namespace {

constexpr std::array<std::byte, 4> kMagic{std::byte{'Y'}, std::byte{'A'},
                                          std::byte{'L'}, std::byte{'1'}};

void writeU16(std::span<std::byte> bytes, std::uint16_t value) {
  bytes[0] = static_cast<std::byte>(value >> 8U);
  bytes[1] = static_cast<std::byte>(value);
}

void writeU32(std::span<std::byte> bytes, std::uint32_t value) {
  for (std::size_t i = 0; i < 4; ++i) {
    bytes[i] = static_cast<std::byte>(value >> (24U - 8U * i));
  }
}

void writeU64(std::span<std::byte> bytes, std::uint64_t value) {
  for (std::size_t i = 0; i < 8; ++i) {
    bytes[i] = static_cast<std::byte>(value >> (56U - 8U * i));
  }
}

std::uint16_t readU16(std::span<const std::byte> bytes) {
  return (std::to_integer<std::uint16_t>(bytes[0]) << 8U) |
         std::to_integer<std::uint16_t>(bytes[1]);
}

std::uint32_t readU32(std::span<const std::byte> bytes) {
  std::uint32_t value = 0;
  for (const auto byte : bytes.first<4>()) {
    value = (value << 8U) | std::to_integer<std::uint32_t>(byte);
  }
  return value;
}

std::uint64_t readU64(std::span<const std::byte> bytes) {
  std::uint64_t value = 0;
  for (const auto byte : bytes.first<8>()) {
    value = (value << 8U) | std::to_integer<std::uint64_t>(byte);
  }
  return value;
}

}  // namespace

std::array<std::byte, kFrameHeaderSize> encodeHeader(
    const FrameHeader& header) {
  std::array<std::byte, kFrameHeaderSize> bytes{};
  std::ranges::copy(kMagic, bytes.begin());
  writeU16(std::span(bytes).subspan<4, 2>(), header.version);
  writeU16(std::span(bytes).subspan<6, 2>(), header.flags);
  writeU64(std::span(bytes).subspan<8, 8>(), header.sequence);
  writeU32(std::span(bytes).subspan<16, 4>(), header.payload_size);
  return bytes;
}

FrameDecodeResult decodeHeader(std::span<const std::byte> bytes,
                               std::uint32_t max_payload_size) {
  if (bytes.size() < kFrameHeaderSize) {
    return {.error = FrameError::InsufficientData};
  }
  if (!std::ranges::equal(kMagic, bytes.first<4>())) {
    return {.error = FrameError::InvalidMagic};
  }

  FrameHeader header{
      .version = readU16(bytes.subspan<4, 2>()),
      .flags = readU16(bytes.subspan<6, 2>()),
      .sequence = readU64(bytes.subspan<8, 8>()),
      .payload_size = readU32(bytes.subspan<16, 4>()),
  };
  if (header.version != kFrameVersion) {
    return {.error = FrameError::UnsupportedVersion};
  }
  if ((header.flags & ~kEncryptedFlag) != 0) {
    return {.error = FrameError::UnknownFlags};
  }
  if (header.payload_size == 0) {
    return {.error = FrameError::EmptyPayload};
  }
  if (header.payload_size > max_payload_size) {
    return {.error = FrameError::PayloadTooLarge};
  }
  return {.header = header};
}

FrameDecodeResult decodeFrame(std::span<const std::byte> bytes,
                              std::uint32_t max_payload_size) {
  auto result = decodeHeader(bytes, max_payload_size);
  if (!result) {
    return result;
  }
  if (bytes.size() != kFrameHeaderSize + result.header.payload_size) {
    return {.error = FrameError::LengthMismatch};
  }
  result.payload = bytes.subspan(kFrameHeaderSize);
  return result;
}

}  // namespace yoauthorize::protocol
