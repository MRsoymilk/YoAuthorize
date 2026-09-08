#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "yoauthorize/core/session.h"

namespace core = yoauthorize::core;

TEST(SessionManagerTest, EnforcesQuotaAndReleasesClosedSession) {
  core::SessionManager sessions;
  auto first =
      sessions.create("product-a", "uid:1000", {"basic"}, 2000, 100, 1);
  ASSERT_TRUE(first);

  EXPECT_EQ(sessions.create("product-a", "uid:1001", {}, 2000, 100, 1).error,
            core::ErrorCode::SessionLimitExceeded);
  EXPECT_TRUE(sessions.close(first.value.id));
  EXPECT_TRUE(sessions.create("product-a", "uid:1001", {}, 2000, 100, 1));
}

TEST(SessionManagerTest, ExpiresInactiveSessions) {
  core::SessionManager sessions;
  const auto session =
      sessions.create("product-a", "uid:1000", {}, 2000, 100, 1);
  ASSERT_TRUE(session);
  EXPECT_EQ(sessions.expireInactive(149, 50), 0);
  EXPECT_EQ(sessions.heartbeat(session.value.id, 125), core::ErrorCode::None);
  EXPECT_EQ(sessions.expireInactive(175, 50), 1);
  EXPECT_EQ(sessions.size(), 0);
}

TEST(SessionManagerTest, AllocatesQuotaAtomically) {
  core::SessionManager sessions;
  std::atomic<int> successes = 0;
  std::vector<std::thread> workers;
  for (int i = 0; i < 8; ++i) {
    workers.emplace_back([&, i] {
      if (sessions.create("product-a", "uid:" + std::to_string(i), {}, 2000,
                          100, 1)) {
        ++successes;
      }
    });
  }
  for (auto& worker : workers) {
    worker.join();
  }

  EXPECT_EQ(successes, 1);
  EXPECT_EQ(sessions.size(), 1);
}
