#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include "yoauthorize/sdk/client.h"

namespace {

yoauthorize::crypto::CryptoResult<yoauthorize::crypto::Key> loadPublicKey(
    const std::filesystem::path& path) {
  yoauthorize::crypto::Key key{};
  std::ifstream input(path, std::ios::binary);
  if (!input.read(reinterpret_cast<char*>(key.data()), key.size())) {
    return {.error = yoauthorize::crypto::CryptoError::InvalidInput};
  }
  char extra = 0;
  if (input.read(&extra, 1)) {
    return {.error = yoauthorize::crypto::CryptoError::InvalidInput};
  }
  return {.value = key};
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 5 || argc > 6) {
    std::cerr << "usage: yoauthorize-test-app <product-id> <socket> "
                 "<service-key-id> <service-public-key> [seconds]\n";
    return 2;
  }
  const auto public_key = loadPublicKey(argv[4]);
  if (!public_key) {
    std::cerr << "failed to load the 32-byte service public key\n";
    return 2;
  }
  int run_seconds = 5;
  if (argc == 6) {
    try {
      run_seconds = std::stoi(argv[5]);
    } catch (const std::exception&) {
      std::cerr << "seconds must be a positive integer\n";
      return 2;
    }
    if (run_seconds <= 0) {
      std::cerr << "seconds must be a positive integer\n";
      return 2;
    }
  }

  yoauthorize::sdk::LicenseClient client;
  const yoauthorize::sdk::ClientConfig config{
      .product_id = argv[1],
      .endpoint = argv[2],
      .trusted_service_keys = {{argv[3], public_key.value}},
  };
  const auto initialized = client.initialize(config);
  if (!initialized) {
    std::cerr << "authorization failed: " << initialized.message << '\n';
    return 1;
  }

  const auto initial = client.snapshot();
  std::cout << "authorization valid\n";
  for (const auto& feature : initial.features) {
    std::cout << "feature: " << feature << '\n';
  }
  for (int elapsed = 0; elapsed < run_seconds; ++elapsed) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (client.snapshot().state != yoauthorize::sdk::ClientState::Valid) {
      std::cerr << "authorization became invalid\n";
      return 1;
    }
  }
  client.shutdown();
  return 0;
}
