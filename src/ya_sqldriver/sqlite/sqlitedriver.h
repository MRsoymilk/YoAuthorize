#ifndef SQLITE_DRIVER
#define SQLITE_DRIVER

#include "isqldriver.h"

class SQLiteDriver : public ISqlDriver {
 public:
  SQLiteDriver();
  ~SQLiteDriver();
  // ISqlDriver interface
 public:
  bool connect(const std::string& uri) override;
  bool insert(const std::string& table,
              const std::map<std::string, std::string>& data) override;
  bool update(const std::string& table,
              const std::map<std::string, std::string>& data,
              const std::string& where) override;
  bool remove(const std::string& table, const std::string& where) override;
  std::vector<std::map<std::string, std::string>> query(
      const std::string& sql) override;

 private:
  void* db;  // sqlite3*
};

#endif  // !SQLITE_DRIVER
