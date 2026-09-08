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

struct DirectionalKey {
  Key key{};
  std::array<std::uint8_t, 4> nonce_salt{};
};

struct SessionKeys {
  DirectionalKey client_to_service;
  DirectionalKey service_to_client;
};

CryptoResult<std::vector<std::uint8_t>> encodeTranscript(
    const HandshakeTranscript& transcript);
CryptoResult<Sha256Digest> hashTranscript(
    const HandshakeTranscript& transcript);
CryptoResult<SessionKeys> deriveSessionKeys(
    const Key& shared_secret, const Sha256Digest& transcript_hash);
Nonce makeRecordNonce(const std::array<std::uint8_t, 4>& salt,
                      std::uint64_t sequence);

}  // namespace yoauthorize::crypto
