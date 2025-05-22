#include "yashell.h"

#include <iostream>
#include <sstream>

YaShell::YaShell() : m_running(true) {}

YaShell::~YaShell() {}

std::vector<std::string> YaShell::parseCommand(const std::string& line) {
  std::vector<std::string> tokens;
  std::istringstream iss(line);
  std::string token;
  while (iss >> token) {
    tokens.push_back(token);
  }
  return tokens;
}

void YaShell::executeCommand(const std::vector<std::string>& tokens) {
  if (tokens.empty()) {
    return;
  }

  const std::string& cmd = tokens[0];

  if (cmd == "q" || cmd == "quit" || cmd == "exit") {
    std::cout << "bye..." << std::endl;
    m_running = false;
  } else if (cmd == "echo") {
    for (size_t i = 1; i < tokens.size(); ++i) {
      std::cout << tokens[i] << " ";
    }
    std::cout << std::endl;
  } else if (cmd == "help") {
    std::cout << "ya-net" << std::endl;
    std::cout << "Usage:\n"
              << "  help        Print help\n"
              << "  echo [args] print args\n"
              << "  exit/quit   Exit shell\n";
  } else {
    std::cout << "Unknown: " << cmd << "\n";
  }
}

void YaShell::run() {
  std::string line;
  while (m_running) {
    std::cout << "ya-net> " << std::flush;
    if (!std::getline(std::cin, line)) {
      std::cout << "\nExit." << std::endl;
      break;
    }
    auto tokens = parseCommand(line);
    executeCommand(tokens);
  }
}
