#include "yoauthorize/core/session.h"

#include <algorithm>

#include "yoauthorize/crypto/crypto.h"

namespace yoauthorize::core {

std::size_t SessionManager::SessionIdHash::operator()(
    const SessionId& id) const noexcept {
  std::size_t hash = 0;
  for (const auto byte : id) {
    hash = (hash * 131U) ^ byte;
  }
  return hash;
}

Result<Session> SessionManager::create(std::string product_id,
                                       std::string peer_identity,
                                       std::vector<std::string> features,
                                       std::uint64_t expire_time,
                                       std::uint64_t now_monotonic_ms,
                                       std::uint32_t max_sessions) {
  if (product_id.empty() || peer_identity.empty() || max_sessions == 0) {
    return {.error = ErrorCode::InvalidLicense};
  }

  std::scoped_lock lock(mutex_);
  const auto active =
      std::ranges::count_if(sessions_, [&product_id](const auto& entry) {
        return entry.second.product_id == product_id;
      });
  if (active >= max_sessions) {
    return {.error = ErrorCode::SessionLimitExceeded};
  }

  Session session{
      .product_id = std::move(product_id),
      .peer_identity = std::move(peer_identity),
      .features = std::move(features),
      .expire_time = expire_time,
      .last_heartbeat_ms = now_monotonic_ms,
  };
  do {
    if (crypto::randomBytes(session.id) != crypto::CryptoError::None) {
      return {.error = ErrorCode::InternalError};
    }
  } while (sessions_.contains(session.id));
  sessions_.emplace(session.id, session);
  return {.value = std::move(session)};
}

ErrorCode SessionManager::heartbeat(const SessionId& id,
                                    std::uint64_t now_monotonic_ms) {
  std::scoped_lock lock(mutex_);
  const auto session = sessions_.find(id);
  if (session == sessions_.end()) {
    return ErrorCode::InvalidSession;
  }
  session->second.last_heartbeat_ms = now_monotonic_ms;
  return ErrorCode::None;
}

bool SessionManager::close(const SessionId& id) {
  std::scoped_lock lock(mutex_);
  return sessions_.erase(id) == 1;
}

std::size_t SessionManager::expireInactive(std::uint64_t now_monotonic_ms,
                                           std::uint64_t timeout_ms) {
  std::scoped_lock lock(mutex_);
  return std::erase_if(sessions_, [=](const auto& entry) {
    const auto heartbeat = entry.second.last_heartbeat_ms;
    return now_monotonic_ms >= heartbeat &&
           now_monotonic_ms - heartbeat >= timeout_ms;
  });
}

std::size_t SessionManager::size() const {
  std::scoped_lock lock(mutex_);
  return sessions_.size();
}

}  // namespace yoauthorize::core
