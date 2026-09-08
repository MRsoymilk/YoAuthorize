#include "yoauthorize/crypto/handshake.h"

#include <algorithm>
#include <string_view>

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

}  // namespace

std::vector<std::uint8_t> encodeTranscript(
    const HandshakeTranscript& transcript) {
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
  return output;
}

CryptoResult<Sha256Digest> hashTranscript(
    const HandshakeTranscript& transcript) {
  const auto encoded = encodeTranscript(transcript);
  return sha256(encoded);
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
