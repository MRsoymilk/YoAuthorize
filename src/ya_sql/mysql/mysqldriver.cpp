#include "mysqldriver.h"

#include <mysqlx/xdevapi.h>

#include <memory>

namespace ya {

class MysqlDriver::Impl {
 public:
  Impl() : session(nullptr) {}

  ~Impl() {
    if (session) {
      session->close();
      delete session;
    }
  }

  bool connect(const std::string &uri) {
    try {
      session = new mysqlx::Session(uri);
      return true;
    } catch (const mysqlx::Error &e) {
      last_error = e.what();
      return false;
    }
  }

  bool insert(const std::string &table,
              const std::map<std::string, std::string> &data) {
    if (!session || data.empty()) return false;

    try {
      mysqlx::Schema schema = session->getDefaultSchema();
      mysqlx::Table tbl = schema.getTable(table);

      // Build the row data
      mysqlx::Row row;
      std::vector<std::string> columns;
      std::vector<mysqlx::Value> values;
      for (const auto &pair : data) {
        columns.push_back(pair.first);
        values.emplace_back(pair.second);
      }

      // Perform insert
      tbl.insert(columns).values(values).execute();
      return true;
    } catch (const mysqlx::Error &e) {
      last_error = e.what();
      return false;
    }
  }

  bool update(const std::string &table,
              const std::map<std::string, std::string> &data,
              const std::string &where) {
    if (!session || data.empty()) return false;

    try {
      mysqlx::Schema schema = session->getDefaultSchema();
      mysqlx::Table tbl = schema.getTable(table);

      // Build update operation
      auto update_op = tbl.update();
      for (const auto &pair : data) {
        update_op.set(pair.first, pair.second);
      }

      // Apply where condition
      update_op.where(where).execute();
      return true;
    } catch (const mysqlx::Error &e) {
      last_error = e.what();
      return false;
    }
  }

  bool remove(const std::string &table, const std::string &where) {
    if (!session) return false;

    try {
      mysqlx::Schema schema = session->getDefaultSchema();
      mysqlx::Table tbl = schema.getTable(table);

      // Perform delete
      tbl.remove().where(where).execute();
      return true;
    } catch (const mysqlx::Error &e) {
      last_error = e.what();
      return false;
    }
  }

  std::vector<std::map<std::string, std::string>> query(
      const std::string &sql) {
    std::vector<std::map<std::string, std::string>> result;
    if (!session) return result;

    try {
      // Execute raw SQL query
      mysqlx::SqlResult res = session->sql(sql).execute();

      // Get column names
      std::vector<std::string> columns;
      for (unsigned int i = 0; i < res.getColumnCount(); ++i) {
        columns.push_back(res.getColumn(i).getColumnLabel());
      }

      // Process rows
      for (mysqlx::Row row : res) {
        std::map<std::string, std::string> row_data;
        for (size_t i = 0; i < columns.size(); ++i) {
          std::string value;
          try {
            value = row[i].isNull() ? "" : row[i].get<std::string>();
          } catch (const std::exception &e) {
            value = "";
          }
          row_data[columns[i]] = value;
        }
        result.push_back(row_data);
      }

    } catch (const mysqlx::Error &e) {
      last_error = e.what();
    }

    return result;
  }

  std::string getLastError() const { return last_error; }

 private:
  mysqlx::Session *session;
  std::string last_error;
};

MysqlDriver::MysqlDriver() : pimpl(std::make_unique<Impl>()) {}

MysqlDriver::~MysqlDriver() = default;

bool MysqlDriver::connect(const std::string &uri) {
  return pimpl->connect(uri);
}

bool MysqlDriver::insert(const std::string &table,
                         const std::map<std::string, std::string> &data) {
  return pimpl->insert(table, data);
}

bool MysqlDriver::update(const std::string &table,
                         const std::map<std::string, std::string> &data,
                         const std::string &where) {
  return pimpl->update(table, data, where);
}

bool MysqlDriver::remove(const std::string &table, const std::string &where) {
  return pimpl->remove(table, where);
}

std::vector<std::map<std::string, std::string>> MysqlDriver::query(
    const std::string &sql) {
  return pimpl->query(sql);
}

}  // namespace ya
