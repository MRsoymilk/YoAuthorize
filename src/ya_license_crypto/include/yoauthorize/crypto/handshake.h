#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include "yoauthorize/crypto/crypto.h"

namespace yoauthorize::crypto {

struct HandshakeTranscript {
  std::uint16_t min_version = 0;
  std::uint16_t max_version = 0;
  std::uint16_t selected_version = 0;
  Key client_nonce{};
  Key server_nonce{};
  Key client_public_key{};
  Key server_public_key{};
  std::string_view service_key_id;
};

std::vector<std::uint8_t> encodeTranscript(
    const HandshakeTranscript& transcript);
CryptoResult<Sha256Digest> hashTranscript(
    const HandshakeTranscript& transcript);
Nonce makeRecordNonce(const std::array<std::uint8_t, 4>& salt,
                      std::uint64_t sequence);

}  // namespace yoauthorize::crypto
