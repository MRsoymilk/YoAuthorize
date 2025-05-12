#ifndef YA_JSON_H
#define YA_JSON_H

#include <string>
#include <stdexcept>
#include <memory>

namespace ya {

class JsonParser {
public:
    // Exception class for JSON parsing errors
    class JsonException : public std::runtime_error {
    public:
        JsonException(const std::string& msg) : std::runtime_error(msg) {}
    };

    JsonParser();
    ~JsonParser();

    // Serialize NodeStatus to JSON string
    std::string serialize_node_status(const std::string& id, const std::string& address,
                                     long uptime, const std::string& info) const;

    // Parse NodeStatus from JSON string
    void parse_node_status(const std::string& json,
                          std::string& id, std::string& address,
                          long& uptime, std::string& info) const;

    // Validate JSON string
    bool is_valid(const std::string& json) const;

    // Prevent copying
    JsonParser(const JsonParser&) = delete;
    JsonParser& operator=(const JsonParser&) = delete;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ya

#endif // YA_JSON_H
