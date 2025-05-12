#include "ya_json.h"
#include <sstream>
#include "simdjson.h"

namespace ya {

class JsonParser::Impl {
public:
    Impl() : parser_(std::make_unique<simdjson::ondemand::parser>()) {}
    ~Impl() = default;

    std::string serialize_node_status(const std::string& id, const std::string& address,
                                     long uptime, const std::string& info) const {
        std::stringstream ss;
        ss << "{\"id\":\"" << id << "\",\"address\":\"" << address
           << "\",\"uptime\":" << uptime << ",\"info\":\"" << info << "\"}";
        return ss.str();
    }

    void parse_node_status(const std::string& json,
                          std::string& id, std::string& address,
                          long& uptime, std::string& info) const {
        try {
            simdjson::padded_string padded(json);
            simdjson::ondemand::document doc = parser_->iterate(padded);
            id = std::string(doc["id"].get_string().value());
            address = std::string(doc["address"].get_string().value());
            uptime = doc["uptime"].get_int64().value();
            info = std::string(doc["info"].get_string().value());
        } catch (const simdjson::simdjson_error& e) {
            throw JsonException("Failed to parse NodeStatus: " + std::string(e.what()));
        }
    }

    bool is_valid(const std::string& json) const {
        try {
            simdjson::padded_string padded(json);
            simdjson::ondemand::document doc = parser_->iterate(padded);
            return true;
        } catch (const simdjson::simdjson_error&) {
            return false;
        }
    }

private:
    std::unique_ptr<simdjson::ondemand::parser> parser_;
};

JsonParser::JsonParser() : impl_(std::make_unique<Impl>()) {}
JsonParser::~JsonParser() = default;

std::string JsonParser::serialize_node_status(const std::string& id, const std::string& address,
                                             long uptime, const std::string& info) const {
    return impl_->serialize_node_status(id, address, uptime, info);
}

void JsonParser::parse_node_status(const std::string& json,
                                  std::string& id, std::string& address,
                                  long& uptime, std::string& info) const {
    impl_->parse_node_status(json, id, address, uptime, info);
}

bool JsonParser::is_valid(const std::string& json) const {
    return impl_->is_valid(json);
}

} // namespace ya
