#include "yoauthorize/crypto/handshake.h"

#include <algorithm>
#include <limits>
#include <span>
#include <string_view>
#include <utility>

namespace yoauthorize::crypto {
namespace {

constexpr std::string_view kDomain = "YOAUTHORIZE-SERVICE-HANDSHAKE-V1";

void appendU16(std::vector<std::uint8_t>& output, std::uint16_t value) {
  output.push_back(static_cast<std::uint8_t>(value >> 8U));
  output.push_back(static_cast<std::uint8_t>(value));
}

template <typename Range>
void appendBytes(std::vector<std::uint8_t>& output, const Range& bytes) {
  output.insert(output.end(), bytes.begin(), bytes.end());
}

void appendSizedString(std::vector<std::uint8_t>& output,
                       std::string_view value) {
  appendU16(output, static_cast<std::uint16_t>(value.size()));
  output.insert(output.end(), value.begin(), value.end());
}

std::span<const std::uint8_t> bytes(std::string_view value) {
  return {reinterpret_cast<const std::uint8_t*>(value.data()), value.size()};
}

CryptoResult<std::vector<std::uint8_t>> derive(
    const Key& shared_secret, const Sha256Digest& transcript_hash,
    std::string_view label, std::size_t size) {
  return hkdfSha256(shared_secret, transcript_hash, bytes(label), size);
}

}  // namespace

CryptoResult<std::vector<std::uint8_t>> encodeTranscript(
    const HandshakeTranscript& transcript) {
  if (transcript.service_key_id.empty() ||
      transcript.service_key_id.size() >
          std::numeric_limits<std::uint16_t>::max()) {
    return {.error = CryptoError::InvalidInput};
  }
  std::vector<std::uint8_t> output;
  output.reserve(kDomain.size() + 3 * sizeof(std::uint16_t) + 4 * kKeySize +
                 sizeof(std::uint16_t) + transcript.service_key_id.size());
  output.insert(output.end(), kDomain.begin(), kDomain.end());
  appendU16(output, transcript.min_version);
  appendU16(output, transcript.max_version);
  appendU16(output, transcript.selected_version);
  appendBytes(output, transcript.client_nonce);
  appendBytes(output, transcript.server_nonce);
  appendBytes(output, transcript.client_public_key);
  appendBytes(output, transcript.server_public_key);
  appendSizedString(output, transcript.service_key_id);
  return {.value = std::move(output)};
}

CryptoResult<Sha256Digest> hashTranscript(
    const HandshakeTranscript& transcript) {
  const auto encoded = encodeTranscript(transcript);
  if (!encoded) {
    return {.error = encoded.error};
  }
  return sha256(encoded.value);
}

CryptoResult<SessionKeys> deriveSessionKeys(
    const Key& shared_secret, const Sha256Digest& transcript_hash) {
  const auto c2s_key =
      derive(shared_secret, transcript_hash, "YA-V1-C2S-KEY", kKeySize);
  const auto s2c_key =
      derive(shared_secret, transcript_hash, "YA-V1-S2C-KEY", kKeySize);
  const auto c2s_nonce =
      derive(shared_secret, transcript_hash, "YA-V1-C2S-NONCE", 4);
  const auto s2c_nonce =
      derive(shared_secret, transcript_hash, "YA-V1-S2C-NONCE", 4);
  if (!c2s_key || !s2c_key || !c2s_nonce || !s2c_nonce) {
    return {.error = CryptoError::DerivationFailed};
  }

  SessionKeys keys;
  std::ranges::copy(c2s_key.value, keys.client_to_service.key.begin());
  std::ranges::copy(s2c_key.value, keys.service_to_client.key.begin());
  std::ranges::copy(c2s_nonce.value, keys.client_to_service.nonce_salt.begin());
  std::ranges::copy(s2c_nonce.value, keys.service_to_client.nonce_salt.begin());
  return {.value = keys};
}

Nonce makeRecordNonce(const std::array<std::uint8_t, 4>& salt,
                      std::uint64_t sequence) {
  Nonce nonce{};
  std::ranges::copy(salt, nonce.begin());
  for (std::size_t i = 0; i < sizeof(sequence); ++i) {
    nonce[4 + i] = static_cast<std::uint8_t>(sequence >> (56U - 8U * i));
  }
  return nonce;
}

}  // namespace yoauthorize::crypto
