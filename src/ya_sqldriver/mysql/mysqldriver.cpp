#include "mysqldriver.h"

MysqlDriver::MysqlDriver() {}

MysqlDriver::~MysqlDriver() {}

bool MysqlDriver::connect(const std::string &uri) {
  return true;
}

bool MysqlDriver::insert(const std::string &table,
                         const std::map<std::string, std::string> &data) {
  return true;
}

bool MysqlDriver::update(const std::string &table,
                         const std::map<std::string, std::string> &data,
                         const std::string &where) {
  
  return true;
}

bool MysqlDriver::remove(const std::string &table, const std::string &where) {
  
  return true;
}

std::vector<std::map<std::string, std::string> > MysqlDriver::query(
    const std::string &sql) {
  return {};
}
