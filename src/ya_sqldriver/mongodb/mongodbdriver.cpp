#include "mongodbdriver.h"

MongodbDriver::MongodbDriver() {}

MongodbDriver::~MongodbDriver() {}

bool MongodbDriver::connect(const std::string &uri) {
  return true;
}

bool MongodbDriver::insert(const std::string &table,
                           const std::map<std::string, std::string> &data) {
  return true;
}

bool MongodbDriver::update(const std::string &table,
                           const std::map<std::string, std::string> &data,
                           const std::string &where) {
  
  return true;
}

bool MongodbDriver::remove(const std::string &table, const std::string &where) {
  return true;
}

std::vector<std::map<std::string, std::string> > MongodbDriver::query(
    const std::string &sql) {
  return {};
}
