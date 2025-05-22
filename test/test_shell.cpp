#include <gtest/gtest.h>

#include <sstream>

#include "yashell.h"

TEST(YaShellTest, ParseCommand) {
  ya::YaShell shell;
  auto tokens = shell.parseCommand("echo hello world");
  ASSERT_EQ(tokens.size(), 3);
  EXPECT_EQ(tokens[0], "echo");
  EXPECT_EQ(tokens[1], "hello");
  EXPECT_EQ(tokens[2], "world");
}

TEST(YaShellTest, ExecuteCommandEcho) {
  std::istringstream input("echo test\nexit\n");
  std::ostringstream output;
  ya::YaShell shell(input, output);
  shell.run();

  std::string out = output.str();
  EXPECT_NE(out.find("test"), std::string::npos);
}

TEST(YaShellTest, ExecuteCommandHelp) {
  std::istringstream input("help\nexit\n");
  std::ostringstream output;
  ya::YaShell shell(input, output);
  shell.run();

  std::string out = output.str();
  EXPECT_NE(out.find("Usage"), std::string::npos);
}

TEST(YaShellTest, ExecuteUnknownCommand) {
  std::istringstream input("foobar\nexit\n");
  std::ostringstream output;
  ya::YaShell shell(input, output);
  shell.run();

  std::string out = output.str();
  EXPECT_NE(out.find("Unknown"), std::string::npos);
}
