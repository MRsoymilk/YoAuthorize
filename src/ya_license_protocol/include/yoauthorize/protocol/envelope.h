#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace yoauthorize::protocol {

enum class EnvelopeError {
  None,
  Empty,
  TooLarge,
  InvalidIdentifier,
  InvalidBuffer,
};

EnvelopeError verifyEnvelope(std::span<const std::uint8_t> bytes,
                             std::size_t max_size = 1024 * 1024,
                             std::uint32_t max_depth = 16,
                             std::uint32_t max_tables = 256);

}  // namespace yoauthorize::protocol
