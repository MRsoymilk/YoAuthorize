#ifndef NODE_DEF_H
#define NODE_DEF_H

#include <format>
#include <sstream>
#include <string>

namespace ya {

struct NodeStatus {
  std::string id;       // Unique node identifier
  std::string address;  // URL (e.g., tls+tcp://127.0.0.1:5555)
  long long uptime;     // Seconds since node started
  std::string info;     // Additional info (e.g., load, version)

  std::string to_string() const;
};

inline std::string NodeStatus::to_string() const {
  std::stringstream ss;
  ss << std::format("Node(id={}, address={}, uptime={}s, info={})", id, address,
                    uptime, info);
  return ss.str();
}

class CommException : public std::runtime_error {
 public:
  CommException(const std::string& msg) : std::runtime_error(msg) {}
};

} // namespace ya

#endif // !NODE_DEF_H
