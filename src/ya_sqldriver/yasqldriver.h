#ifndef YA_SQLDRIVER_H
#define YA_SQLDRIVER_H

#include <memory>
#include <vector>
#include <map>

#include "isqldriver.h"

class YaSqlDriver {
 public:
  bool loadDriver(const std::string& yype);
  bool connect(const std::string& uri);

  bool insert(const std::string& table,
              const std::map<std::string, std::string>& data);
  bool update(const std::string& table,
              const std::map<std::string, std::string>& data,
              const std::string& where);
  bool remove(const std::string& table, const std::string& where);
  std::vector<std::map<std::string, std::string>> query(const std::string& sql);

 private:
  std::unique_ptr<ISqlDriver> m_driver;
};

#endif  // !YA_SQLDRIVER_H
