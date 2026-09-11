#include "yoauthorize/protocol/record.h"

#include <cstddef>
#include <limits>
#include <utility>

#include "yoauthorize/crypto/handshake.h"
#include "yoauthorize/protocol/frame.h"

namespace yoauthorize::protocol {
namespace {

std::span<const std::uint8_t> asBytes(std::span<const std::byte> bytes) {
  return {reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()};
}

std::span<const std::byte> asBytes(std::span<const std::uint8_t> bytes) {
  return {reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()};
}

}  // namespace

RecordWriter::RecordWriter(RecordKey key) : key_(std::move(key)) {}

RecordWriter::~RecordWriter() {
  crypto::cleanse(key_.key);
  crypto::cleanse(key_.nonce_salt);
}

RecordResult RecordWriter::seal(std::span<const std::uint8_t> plaintext) {
  if (next_sequence_ == std::numeric_limits<std::uint64_t>::max()) {
    return {.error = RecordError::SequenceExhausted};
  }
  if (plaintext.empty() ||
      plaintext.size() > kDefaultMaxPayloadSize - crypto::kTagSize) {
    return {.error = RecordError::InvalidFrame};
  }

  const FrameHeader header{
      .flags = kEncryptedFlag,
      .sequence = next_sequence_,
      .payload_size =
          static_cast<std::uint32_t>(plaintext.size() + crypto::kTagSize),
  };
  const auto encoded_header = encodeHeader(header);
  const auto nonce = crypto::makeRecordNonce(key_.nonce_salt, next_sequence_);
  const auto encrypted = crypto::sealChaCha20Poly1305(
      key_.key, nonce, plaintext, asBytes(encoded_header));
  if (!encrypted) {
    return {.error = RecordError::AuthenticationFailed};
  }

  RecordResult result;
  result.bytes.reserve(encoded_header.size() + encrypted.value.size());
  for (const auto byte : encoded_header) {
    result.bytes.push_back(std::to_integer<std::uint8_t>(byte));
  }
  result.bytes.insert(result.bytes.end(), encrypted.value.begin(),
                      encrypted.value.end());
  ++next_sequence_;
  return result;
}

RecordReader::RecordReader(RecordKey key) : key_(std::move(key)) {}

RecordReader::~RecordReader() {
  crypto::cleanse(key_.key);
  crypto::cleanse(key_.nonce_salt);
}

RecordResult RecordReader::open(std::span<const std::uint8_t> frame) {
  if (next_sequence_ == std::numeric_limits<std::uint64_t>::max()) {
    return {.error = RecordError::SequenceExhausted};
  }
  const auto decoded = decodeFrame(asBytes(frame));
  if (!decoded) {
    return {.error = RecordError::InvalidFrame};
  }
  if (decoded.header.flags != kEncryptedFlag) {
    return {.error = RecordError::UnencryptedFrame};
  }
  if (decoded.header.sequence != next_sequence_) {
    return {.error = RecordError::InvalidSequence};
  }

  const auto nonce = crypto::makeRecordNonce(key_.nonce_salt, next_sequence_);
  const auto plaintext = crypto::openChaCha20Poly1305(
      key_.key, nonce, asBytes(decoded.payload), frame.first(kFrameHeaderSize));
  if (!plaintext) {
    return {.error = RecordError::AuthenticationFailed};
  }
  ++next_sequence_;
  return {.bytes = std::move(plaintext.value)};
}

}  // namespace yoauthorize::protocol
