#ifndef YA_SHELL_H
#define YA_SHELL_H

#include <string>
#include <vector>

namespace ya {

class YaShell {
 public:
  YaShell();
  ~YaShell();
  void run();

 private:
  std::vector<std::string> parseCommand(const std::string& line);
  void executeCommand(const std::vector<std::string>& tokens);

 private:
  bool m_running;
};

}  // namespace ya

#endif  // !YA_SHELL_H
