#ifndef YA_SHELL_H
#define YA_SHELL_H

#include <iostream>
#include <string>
#include <vector>

namespace ya {

class YaShell {
 public:
  YaShell(std::istream& in = std::cin, std::ostream& out = std::cout);
  ~YaShell();
  void run();

  std::vector<std::string> parseCommand(const std::string& line);
  void executeCommand(const std::vector<std::string>& tokens);

 private:
  bool m_running;
  std::istream& m_in;
  std::ostream& m_out;
};

}  // namespace ya

#endif  // !YA_SHELL_H
