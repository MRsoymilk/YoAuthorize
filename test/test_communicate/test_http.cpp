#include <gtest/gtest.h>

#include "ya_communicate/module/http.h"

std::string exec(const char* cmd) {
  std::array<char, 128> buffer;
  std::string result;

  std::unique_ptr<FILE, int (*)(FILE*)> pipe(popen(cmd, "r"), pclose);
  if (!pipe) {
    return "error";
  }

  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

TEST(TestModuleHttp, Api) {
  ya::module::Http http;
  http.start(18000);
  // /status
  std::string res_status = exec("curl -s http://127.0.0.1:18000/status");
  std::cout << "/status: " << res_status << std::endl;
  // /nodes
  std::string res_nodes = exec("curl -s http://127.0.0.1:18000/nodes");
  std::cout << "/nodes: " << res_nodes << std::endl;
}
