#include "postgresqldriver.h"

namespace ya {

PostgresqlDriver::PostgresqlDriver() {}

PostgresqlDriver::~PostgresqlDriver() {}

bool PostgresqlDriver::connect(const std::string &uri) { return true; }

bool PostgresqlDriver::insert(const std::string &table,
                              const std::map<std::string, std::string> &data) {
  return true;
}

bool PostgresqlDriver::update(const std::string &table,
                              const std::map<std::string, std::string> &data,
                              const std::string &where) {
  return true;
}

bool PostgresqlDriver::remove(const std::string &table,
                              const std::string &where) {
  return true;
}

std::vector<std::map<std::string, std::string> > PostgresqlDriver::query(
    const std::string &sql) {
  return {};
}

}  // namespace ya
