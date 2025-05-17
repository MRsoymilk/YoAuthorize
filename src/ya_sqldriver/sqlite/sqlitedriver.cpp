#include "sqlitedriver.h"

#include <map>
#include <sstream>
#include <string>

#include "sqlite3.h"

SQLiteDriver::SQLiteDriver() : db(nullptr) {}

SQLiteDriver::~SQLiteDriver() {
  if (db) {
    sqlite3_close(static_cast<sqlite3 *>(db));  // Cast void* to sqlite3*
    db = nullptr;
  }
}

bool SQLiteDriver::connect(const std::string &uri) {
  if (db) {
    sqlite3_close(static_cast<sqlite3 *>(db));  // Cast void* to sqlite3*
    db = nullptr;
  }

  sqlite3 *temp_db = nullptr;
  int rc = sqlite3_open(uri.c_str(), &temp_db);
  if (rc != SQLITE_OK) {
    sqlite3_close(temp_db);
    return false;
  }
  db = temp_db;  // Store sqlite3* as void*
  return true;
}

bool SQLiteDriver::insert(const std::string &table,
                          const std::map<std::string, std::string> &data) {
  if (!db || data.empty()) return false;

  std::stringstream columns, values;
  for (const auto &pair : data) {
    columns << pair.first << ",";
    values << "'" << pair.second << "',";
  }

  std::string col_str = columns.str();
  std::string val_str = values.str();
  col_str.pop_back();  // Remove trailing comma
  val_str.pop_back();

  std::string sql =
      "INSERT INTO " + table + " (" + col_str + ") VALUES (" + val_str + ")";

  char *err_msg = nullptr;
  int rc = sqlite3_exec(static_cast<sqlite3 *>(db), sql.c_str(), nullptr,
                        nullptr, &err_msg);  // Cast void* to sqlite3*

  if (rc != SQLITE_OK) {
    if (err_msg) {
      sqlite3_free(err_msg);
    }
    return false;
  }

  return true;
}

bool SQLiteDriver::update(const std::string &table,
                          const std::map<std::string, std::string> &data,
                          const std::string &where) {
  if (!db || data.empty()) return false;

  std::stringstream set_clause;
  for (const auto &pair : data) {
    set_clause << pair.first << "='" << pair.second << "',";
  }

  std::string set_str = set_clause.str();
  set_str.pop_back();  // Remove trailing comma

  std::string sql = "UPDATE " + table + " SET " + set_str;
  if (!where.empty()) {
    sql += " WHERE " + where;
  }

  char *err_msg = nullptr;
  int rc = sqlite3_exec(static_cast<sqlite3 *>(db), sql.c_str(), nullptr,
                        nullptr, &err_msg);  // Cast void* to sqlite3*

  if (rc != SQLITE_OK) {
    if (err_msg) {
      sqlite3_free(err_msg);
    }
    return false;
  }

  return true;
}

bool SQLiteDriver::remove(const std::string &table, const std::string &where) {
  if (!db) return false;

  std::string sql = "DELETE FROM " + table;
  if (!where.empty()) {
    sql += " WHERE " + where;
  }

  char *err_msg = nullptr;
  int rc = sqlite3_exec(static_cast<sqlite3 *>(db), sql.c_str(), nullptr,
                        nullptr, &err_msg);  // Cast void* to sqlite3*

  if (rc != SQLITE_OK) {
    if (err_msg) {
      sqlite3_free(err_msg);
    }
    return false;
  }

  return true;
}

std::vector<std::map<std::string, std::string>> SQLiteDriver::query(
    const std::string &sql) {
  std::vector<std::map<std::string, std::string>> results;
  if (!db) return results;

  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(static_cast<sqlite3 *>(db), sql.c_str(), -1, &stmt,
                         nullptr) != SQLITE_OK) {  // Cast void* to sqlite3*
    return results;
  }

  int col_count = sqlite3_column_count(stmt);
  std::vector<std::string> col_names(col_count);
  for (int i = 0; i < col_count; i++) {
    col_names[i] = sqlite3_column_name(stmt, i);
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    std::map<std::string, std::string> row;
    for (int i = 0; i < col_count; i++) {
      const char *value =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, i));
      row[col_names[i]] = value ? value : "";
    }
    results.push_back(row);
  }

  sqlite3_finalize(stmt);
  return results;
}
