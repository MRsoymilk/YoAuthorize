#ifndef I_SQL_DRIVER_H
#define I_SQL_DRIVER_H

#include <map>
#include <string>
#include <vector>

namespace ya {

class ISqlDriver {
 public:
  virtual ~ISqlDriver() = default;

  virtual bool connect(const std::string& uri) = 0;
  virtual bool insert(const std::string& table,
                      const std::map<std::string, std::string>& data) = 0;
  virtual bool update(const std::string& table,
                      const std::map<std::string, std::string>& data,
                      const std::string& where) = 0;
  virtual bool remove(const std::string& table, const std::string& where) = 0;
  virtual std::vector<std::map<std::string, std::string>> query(
      const std::string& sql) = 0;
};

}  // namespace ya

#endif  // !I_SQL_DRIVER_H
