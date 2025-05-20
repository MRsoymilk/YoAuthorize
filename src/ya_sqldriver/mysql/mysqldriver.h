#ifndef MYSQL_DRIVER_H
#define MYSQL_DRIVER_H

#include <memory>

#include "isqldriver.h"

class MysqlDriver : public ISqlDriver {
 public:
  MysqlDriver();
  ~MysqlDriver();
  // ISqlDriver interface
 public:
  bool connect(const std::string &uri);
  bool insert(const std::string &table,
              const std::map<std::string, std::string> &data);
  bool update(const std::string &table,
              const std::map<std::string, std::string> &data,
              const std::string &where);
  bool remove(const std::string &table, const std::string &where);
  std::vector<std::map<std::string, std::string> > query(
      const std::string &sql);

 private:
  class Impl;
  std::unique_ptr<Impl> pimpl;
};

#endif  // !MYSQL_DRIVER_H
