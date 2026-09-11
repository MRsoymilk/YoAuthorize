#pragma once

#include <array>
#include <cstdint>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "yoauthorize/core/status.h"

namespace yoauthorize::core {

using SessionId = std::array<std::uint8_t, 16>;

struct Session {
  SessionId id{};
  std::string product_id;
  std::string peer_identity;
  std::vector<std::string> features;
  std::uint64_t expire_time = 0;
  std::uint64_t last_heartbeat_ms = 0;
};

class SessionManager {
 public:
  Result<Session> create(std::string product_id, std::string peer_identity,
                         std::vector<std::string> features,
                         std::uint64_t expire_time,
                         std::uint64_t now_monotonic_ms,
                         std::uint32_t max_sessions);
  ErrorCode heartbeat(const SessionId& id, std::uint64_t now_monotonic_ms);
  bool close(const SessionId& id);
  std::size_t expireInactive(std::uint64_t now_monotonic_ms,
                             std::uint64_t timeout_ms);
  std::size_t size() const;

 private:
  struct SessionIdHash {
    std::size_t operator()(const SessionId& id) const noexcept;
  };

  mutable std::mutex mutex_;
  std::unordered_map<SessionId, Session, SessionIdHash> sessions_;
};

}  // namespace yoauthorize::core
