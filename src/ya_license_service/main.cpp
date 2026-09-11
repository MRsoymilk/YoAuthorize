#include <atomic>
#include <csignal>
#include <iostream>

#include "yoauthorize/service/service.h"

namespace {

std::atomic_bool stop_requested = false;

void handleSignal(int) {
  stop_requested.store(true, std::memory_order_relaxed);
}

}  // namespace

int main(int argc, char** argv) {
  const std::filesystem::path config =
      argc > 1 ? argv[1] : "/etc/yoauthorize/service.toml";
  std::signal(SIGINT, handleSignal);
  std::signal(SIGTERM, handleSignal);

  auto service = yoauthorize::service::AuthorizationService::create(config);
  if (!service) {
    std::cerr << "failed to initialize authorization service\n";
    return 1;
  }
  if (service.value->run(stop_requested) !=
      yoauthorize::service::ServiceError::None) {
    std::cerr << "authorization service stopped with an error\n";
    return 1;
  }
  return 0;
}
