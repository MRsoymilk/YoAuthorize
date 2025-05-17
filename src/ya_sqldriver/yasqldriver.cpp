#include "yasqldriver.h"

#include "mongodb/mongodbdriver.h"
#include "mysql/mysqldriver.h"
#include "postgresql/postgresqldriver.h"
#include "sqlite/sqlitedriver.h"

bool YaSqlDriver::loadDriver(const std::string& dbType) {
    if (dbType == "sqlite") {
      m_driver = std::make_unique<SQLiteDriver>();
    } else if (dbType == "mysql") {
      m_driver = std::make_unique<MysqlDriver>();
    } else if (dbType == "mongodb") {
      m_driver = std::make_unique<MongodbDriver>();
    } else if (dbType == "postgresql") {
      m_driver = std::make_unique<PostgresqlDriver>();
    } else {
      return false;
    }
    return true;
}

bool YaSqlDriver::connect(const std::string& uri) {
  return m_driver && m_driver->connect(uri);
}

bool YaSqlDriver::insert(const std::string& table, const std::map<std::string, std::string>& data) {
  return m_driver && m_driver->insert(table, data);
}

bool YaSqlDriver::update(const std::string& table, const std::map<std::string, std::string>& data, const std::string& where) {
  return m_driver && m_driver->update(table, data, where);
}

bool YaSqlDriver::remove(const std::string& table, const std::string& where) {
  return m_driver && m_driver->remove(table, where);
}

std::vector<std::map<std::string, std::string>> YaSqlDriver::query(const std::string& sql) {
  if (m_driver) {
    return m_driver->query(sql);
  }
    return {};
}

