#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "yoauthorize/crypto/crypto.h"

namespace yoauthorize::protocol {

struct RecordKey {
  crypto::Key key{};
  std::array<std::uint8_t, 4> nonce_salt{};
};

enum class RecordError {
  None,
  InvalidFrame,
  UnencryptedFrame,
  InvalidSequence,
  SequenceExhausted,
  AuthenticationFailed,
};

struct RecordResult {
  RecordError error = RecordError::None;
  std::vector<std::uint8_t> bytes;

  explicit operator bool() const { return error == RecordError::None; }
};

class RecordWriter {
 public:
  explicit RecordWriter(RecordKey key);
  RecordResult seal(std::span<const std::uint8_t> plaintext);
  std::uint64_t nextSequence() const { return next_sequence_; }

 private:
  RecordKey key_;
  std::uint64_t next_sequence_ = 1;
};

class RecordReader {
 public:
  explicit RecordReader(RecordKey key);
  RecordResult open(std::span<const std::uint8_t> frame);
  std::uint64_t nextSequence() const { return next_sequence_; }

 private:
  RecordKey key_;
  std::uint64_t next_sequence_ = 1;
};

}  // namespace yoauthorize::protocol
