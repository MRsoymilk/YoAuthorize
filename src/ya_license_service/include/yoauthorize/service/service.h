#pragma once

#include <atomic>
#include <memory>

#include "yoauthorize/core/license.h"
#include "yoauthorize/core/session.h"
#include "yoauthorize/service/config.h"
#include "yoauthorize/service/key_store.h"
#include "yoauthorize/transport/unix_socket.h"

namespace yoauthorize::service {

enum class ServiceError {
  None,
  Config,
  Keys,
  LicenseFile,
  MachineIdentity,
  InvalidLicense,
  Listen,
  Transport,
};

template <typename T>
struct ServiceResult {
  ServiceError error = ServiceError::None;
  T value{};

  explicit operator bool() const { return error == ServiceError::None; }
};

class AuthorizationService {
 public:
  static ServiceResult<std::unique_ptr<AuthorizationService>> create(
      const std::filesystem::path& config_path);

  ServiceError run(const std::atomic_bool& stop_requested);
  void stop() noexcept;

 private:
  AuthorizationService(ServiceConfig config, LoadedKeys keys,
                       core::LicenseSnapshot license);
  ServiceError serve(transport::UnixSocketTransport& client,
                     std::string peer_identity);

  ServiceConfig config_;
  LoadedKeys keys_;
  core::LicenseSnapshot license_;
  core::SessionManager sessions_;
  transport::UnixSocketListener listener_;
};

}  // namespace yoauthorize::service
