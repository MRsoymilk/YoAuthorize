#ifndef POSTGRESQL_DRIVER
#define POSTGRESQL_DRIVER

#include "isqldriver.h"

class PostgresqlDriver : public ISqlDriver {
 public:
  PostgresqlDriver();
  ~PostgresqlDriver();
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
};

#endif  // !POSTGRESQL_DRIVER
