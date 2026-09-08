#include "yoauthorize/protocol/handshake.h"

#include <algorithm>
#include <utility>

#include "yoauthorize/protocol/messages.h"

namespace yoauthorize::protocol {
namespace {

RecordKey recordKey(const crypto::DirectionalKey& key) {
  return {.key = key.key, .nonce_salt = key.nonce_salt};
}

crypto::HandshakeTranscript transcriptFor(const ClientHelloMessage& client,
                                          const ServerHelloMessage& server) {
  return {
      .min_version = client.min_version,
      .max_version = client.max_version,
      .selected_version = server.selected_version,
      .client_nonce = client.nonce,
      .server_nonce = server.nonce,
      .client_public_key = client.public_key,
      .server_public_key = server.public_key,
      .service_key_id = server.service_key_id,
  };
}

}  // namespace

ClientHandshake::ClientHandshake(
    std::unordered_map<std::string, crypto::Key> trusted_keys,
    std::string sdk_version, std::uint64_t request_id,
    std::uint16_t min_version, std::uint16_t max_version)
    : trusted_keys_(std::move(trusted_keys)),
      sdk_version_(std::move(sdk_version)),
      request_id_(request_id),
      min_version_(min_version),
      max_version_(max_version) {}

void ClientHandshake::fail() {
  crypto::cleanse(ephemeral_.private_key);
  writer_.reset();
  reader_.reset();
  state_ = HandshakeState::Failed;
}

HandshakeResult<std::vector<std::uint8_t>> ClientHandshake::start() {
  if (state_ != HandshakeState::Initial || request_id_ == 0 ||
      min_version_ == 0 || min_version_ > max_version_) {
    fail();
    return {.error = HandshakeError::InvalidState};
  }
  if (crypto::randomBytes(nonce_) != crypto::CryptoError::None) {
    fail();
    return {.error = HandshakeError::InternalError};
  }
  const auto key_pair = crypto::generateX25519KeyPair();
  if (!key_pair) {
    fail();
    return {.error = HandshakeError::KeyExchangeFailed};
  }
  ephemeral_ = key_pair.value;
  const auto encoded = encodeClientHello({
      .request_id = request_id_,
      .min_version = min_version_,
      .max_version = max_version_,
      .nonce = nonce_,
      .public_key = ephemeral_.public_key,
      .sdk_version = sdk_version_,
  });
  if (!encoded) {
    fail();
    return {.error = HandshakeError::InvalidMessage};
  }
  state_ = HandshakeState::HelloSent;
  return {.value = encoded.value};
}

HandshakeResult<std::vector<std::uint8_t>> ClientHandshake::handleServerHello(
    const std::vector<std::uint8_t>& message) {
  if (state_ != HandshakeState::HelloSent) {
    fail();
    return {.error = HandshakeError::InvalidState};
  }
  const auto server = decodeServerHello(message);
  if (!server || server.value.request_id != request_id_) {
    fail();
    return {.error = HandshakeError::InvalidMessage};
  }
  if (server.value.selected_version < min_version_ ||
      server.value.selected_version > max_version_) {
    fail();
    return {.error = HandshakeError::UnsupportedVersion};
  }
  const auto trusted_key = trusted_keys_.find(server.value.service_key_id);
  if (trusted_key == trusted_keys_.end()) {
    fail();
    return {.error = HandshakeError::UnknownServiceKey};
  }
  const ClientHelloMessage client{
      .request_id = request_id_,
      .min_version = min_version_,
      .max_version = max_version_,
      .nonce = nonce_,
      .public_key = ephemeral_.public_key,
      .sdk_version = sdk_version_,
  };
  const auto transcript = transcriptFor(client, server.value);
  const auto encoded_transcript = crypto::encodeTranscript(transcript);
  const auto transcript_hash = crypto::hashTranscript(transcript);
  if (!encoded_transcript || !transcript_hash ||
      crypto::verifyEd25519(trusted_key->second, encoded_transcript.value,
                            server.value.signature) !=
          crypto::CryptoError::None) {
    fail();
    return {.error = HandshakeError::InvalidServiceSignature};
  }
  auto shared =
      crypto::deriveX25519(ephemeral_.private_key, server.value.public_key);
  auto keys =
      shared ? crypto::deriveSessionKeys(shared.value, transcript_hash.value)
             : crypto::CryptoResult<crypto::SessionKeys>{
                   .error = crypto::CryptoError::DerivationFailed};
  if (!keys) {
    fail();
    return {.error = HandshakeError::KeyExchangeFailed};
  }
  writer_.emplace(recordKey(keys.value.client_to_service));
  reader_.emplace(recordKey(keys.value.service_to_client));
  crypto::cleanse(ephemeral_.private_key);
  crypto::cleanse(shared.value);
  crypto::cleanse(keys.value.client_to_service.key);
  crypto::cleanse(keys.value.service_to_client.key);
  selected_version_ = server.value.selected_version;

  const auto finished =
      encodeClientFinished({.request_id = request_id_,
                            .protocol_version = selected_version_,
                            .transcript_hash = transcript_hash.value});
  if (!finished) {
    fail();
    return {.error = HandshakeError::InvalidMessage};
  }
  auto encrypted = writer_->seal(finished.value);
  if (!encrypted) {
    fail();
    return {.error = HandshakeError::InternalError};
  }
  state_ = HandshakeState::Established;
  return {.value = std::move(encrypted.bytes)};
}

RecordResult ClientHandshake::seal(std::span<const std::uint8_t> plaintext) {
  return state_ == HandshakeState::Established && writer_
             ? writer_->seal(plaintext)
             : RecordResult{.error = RecordError::InvalidFrame};
}

RecordResult ClientHandshake::open(std::span<const std::uint8_t> frame) {
  return state_ == HandshakeState::Established && reader_
             ? reader_->open(frame)
             : RecordResult{.error = RecordError::InvalidFrame};
}

ServerHandshake::ServerHandshake(std::string service_key_id,
                                 crypto::Key identity_private_key,
                                 std::uint16_t min_version,
                                 std::uint16_t max_version)
    : service_key_id_(std::move(service_key_id)),
      identity_private_key_(identity_private_key),
      min_version_(min_version),
      max_version_(max_version) {}

void ServerHandshake::fail() {
  crypto::cleanse(identity_private_key_);
  writer_.reset();
  reader_.reset();
  state_ = HandshakeState::Failed;
}

HandshakeResult<std::vector<std::uint8_t>> ServerHandshake::handleClientHello(
    const std::vector<std::uint8_t>& message) {
  if (state_ != HandshakeState::Initial) {
    fail();
    return {.error = HandshakeError::InvalidState};
  }
  const auto client = decodeClientHello(message);
  if (!client) {
    fail();
    return {.error = HandshakeError::InvalidMessage};
  }
  const auto lowest = std::max(min_version_, client.value.min_version);
  const auto highest = std::min(max_version_, client.value.max_version);
  if (lowest > highest) {
    fail();
    return {.error = HandshakeError::UnsupportedVersion};
  }

  crypto::Key nonce{};
  auto ephemeral = crypto::generateX25519KeyPair();
  if (!ephemeral || crypto::randomBytes(nonce) != crypto::CryptoError::None) {
    fail();
    return {.error = HandshakeError::InternalError};
  }
  ServerHelloMessage server{
      .request_id = client.value.request_id,
      .selected_version = highest,
      .nonce = nonce,
      .public_key = ephemeral.value.public_key,
      .service_key_id = service_key_id_,
  };
  const auto transcript = transcriptFor(client.value, server);
  const auto encoded_transcript = crypto::encodeTranscript(transcript);
  const auto transcript_hash = crypto::hashTranscript(transcript);
  if (!encoded_transcript || !transcript_hash) {
    fail();
    return {.error = HandshakeError::InvalidMessage};
  }
  const auto signature =
      crypto::signEd25519(identity_private_key_, encoded_transcript.value);
  if (!signature) {
    fail();
    return {.error = HandshakeError::InternalError};
  }
  server.signature = signature.value;
  crypto::cleanse(identity_private_key_);
  const auto encoded_server = encodeServerHello(server);
  auto shared = crypto::deriveX25519(ephemeral.value.private_key,
                                     client.value.public_key);
  auto keys =
      shared ? crypto::deriveSessionKeys(shared.value, transcript_hash.value)
             : crypto::CryptoResult<crypto::SessionKeys>{
                   .error = crypto::CryptoError::DerivationFailed};
  if (!encoded_server || !keys) {
    fail();
    return {.error = HandshakeError::KeyExchangeFailed};
  }
  request_id_ = client.value.request_id;
  selected_version_ = highest;
  transcript_hash_ = transcript_hash.value;
  reader_.emplace(recordKey(keys.value.client_to_service));
  writer_.emplace(recordKey(keys.value.service_to_client));
  crypto::cleanse(ephemeral.value.private_key);
  crypto::cleanse(shared.value);
  crypto::cleanse(keys.value.client_to_service.key);
  crypto::cleanse(keys.value.service_to_client.key);
  state_ = HandshakeState::AwaitingFinished;
  return {.value = encoded_server.value};
}

HandshakeError ServerHandshake::handleClientFinished(
    const std::vector<std::uint8_t>& encrypted_frame) {
  if (state_ != HandshakeState::AwaitingFinished || !reader_) {
    fail();
    return HandshakeError::InvalidState;
  }
  const auto plaintext = reader_->open(encrypted_frame);
  if (!plaintext) {
    fail();
    return HandshakeError::InvalidFinished;
  }
  const auto finished = decodeClientFinished(plaintext.bytes);
  if (!finished || finished.value.request_id != request_id_ ||
      finished.value.protocol_version != selected_version_ ||
      finished.value.transcript_hash != transcript_hash_) {
    fail();
    return HandshakeError::InvalidFinished;
  }
  state_ = HandshakeState::Established;
  return HandshakeError::None;
}

RecordResult ServerHandshake::seal(std::span<const std::uint8_t> plaintext) {
  return state_ == HandshakeState::Established && writer_
             ? writer_->seal(plaintext)
             : RecordResult{.error = RecordError::InvalidFrame};
}

RecordResult ServerHandshake::open(std::span<const std::uint8_t> frame) {
  return state_ == HandshakeState::Established && reader_
             ? reader_->open(frame)
             : RecordResult{.error = RecordError::InvalidFrame};
}

}  // namespace yoauthorize::protocol
