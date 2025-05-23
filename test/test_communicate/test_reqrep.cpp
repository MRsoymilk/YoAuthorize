#include <gtest/gtest.h>

#include "ya_communicate/module/requester.h"
#include "ya_communicate/module/responder.h"

TEST(TestModuleReqRep, Communicate) {
  const std::string url = "tcp://127.0.0.1:19000";
  std::string server_received;
  std::string client_reply;

  std::thread server_thread([&]() {
    try {
      ya::module::Reponder server(url);
      server_received = server.receive();
      server.send("pong");
    } catch (const std::exception& ex) {
      FAIL() << "Server exception: " << ex.what();
    }
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  try {
    ya::module::Requester client(url);
    client_reply = client.request("ping");
  } catch (const std::exception& ex) {
    FAIL() << "Client exception: " << ex.what();
  }

  server_thread.join();

  ASSERT_EQ(server_received, "ping");
  ASSERT_EQ(client_reply, "pong");
}
