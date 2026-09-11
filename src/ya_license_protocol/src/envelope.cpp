#include "yoauthorize/protocol/envelope.h"

#include <flatbuffers/verifier.h>

#include "protocol_generated.h"

namespace yoauthorize::protocol {

EnvelopeError verifyEnvelope(std::span<const std::uint8_t> bytes,
                             std::size_t max_size, std::uint32_t max_depth,
                             std::uint32_t max_tables) {
  if (bytes.empty()) {
    return EnvelopeError::Empty;
  }
  if (bytes.size() > max_size) {
    return EnvelopeError::TooLarge;
  }
  if (bytes.size() <
          sizeof(flatbuffers::uoffset_t) + flatbuffers::kFileIdentifierLength ||
      !EnvelopeBufferHasIdentifier(bytes.data())) {
    return EnvelopeError::InvalidIdentifier;
  }

  flatbuffers::Verifier::Options options;
  options.max_depth = max_depth;
  options.max_tables = max_tables;
  flatbuffers::Verifier verifier(bytes.data(), bytes.size(), options);
  return VerifyEnvelopeBuffer(verifier) ? EnvelopeError::None
                                        : EnvelopeError::InvalidBuffer;
}

}  // namespace yoauthorize::protocol
