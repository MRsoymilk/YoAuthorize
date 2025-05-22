#include "yashell.h"

#include <iostream>
#include <sstream>

namespace ya {

YaShell::YaShell(std::istream& in, std::ostream& out)
    : m_running(true), m_in(in), m_out(out) {}

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
    m_out << "bye..." << std::endl;
    m_running = false;
  } else if (cmd == "echo") {
    for (size_t i = 1; i < tokens.size(); ++i) {
      m_out << tokens[i] << " ";
    }
    m_out << std::endl;
  } else if (cmd == "help") {
    m_out << "ya-net\n"
          << "Usage:\n"
          << "  help        Print help\n"
          << "  echo [args] print args\n"
          << "  exit/quit   Exit shell\n";
  } else {
    m_out << "Unknown: " << cmd << "\n";
  }
}

void YaShell::run() {
  std::string line;
  while (m_running) {
    m_out << "ya-net> " << std::flush;
    if (!std::getline(m_in, line)) {
      m_out << "\nExit." << std::endl;
      break;
    }
    auto tokens = parseCommand(line);
    executeCommand(tokens);
  }
}

}  // namespace ya
