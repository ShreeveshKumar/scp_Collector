#include "sia/config.hpp"

#include <cstdlib>
#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace sia {
namespace {

std::string env_str(const char* key, const std::string& fallback) {
    const char* v = std::getenv(key);
    return (v && *v) ? std::string(v) : fallback;
}

int env_int(const char* key, int fallback) {
    const char* v = std::getenv(key);
    if (!v || !*v) return fallback;
    try {
        return std::stoi(v);
    } catch (...) {
        return fallback;
    }
}

bool env_bool(const char* key, bool fallback) {
    const char* v = std::getenv(key);
    if (!v || !*v) return fallback;
    std::string s(v);
    return s == "1" || s == "true" || s == "TRUE" || s == "yes";
}

// Assemble a MongoDB URI from parts when MONGODB_URI is not supplied.
// For values with special characters, prefer setting MONGODB_URI directly.
std::string assemble_uri() {
    const std::string scheme  = env_str("MONGODB_SCHEME", "mongodb");  // or mongodb+srv
    const std::string user    = env_str("MONGODB_USERNAME", "");
    const std::string pass    = env_str("MONGODB_PASSWORD", "");
    const std::string host    = env_str("MONGODB_HOST", "localhost:27017");
    const std::string options = env_str("MONGODB_OPTIONS", "");

    std::string uri = scheme + "://";
    if (!user.empty()) {
        uri += user;
        if (!pass.empty()) uri += ":" + pass;
        uri += "@";
    }
    uri += host;
    if (!options.empty()) uri += "/?" + options;
    return uri;
}

}  // namespace

Config Config::load() {
    Config c;

    // 1) Environment (12-factor).
    c.mongodb_uri         = env_str("MONGODB_URI", c.mongodb_uri);
    c.mongodb_database    = env_str("MONGODB_DATABASE", c.mongodb_database);
    c.mongodb_collection  = env_str("MONGODB_COLLECTION", c.mongodb_collection);
    c.hn_tags             = env_str("SIA_HN_TAGS", c.hn_tags);
    c.hn_hits             = env_int("SIA_HN_HITS", c.hn_hits);
    c.http_user_agent     = env_str("SIA_HTTP_USER_AGENT", c.http_user_agent);
    c.http_timeout_ms     = env_int("SIA_HTTP_TIMEOUT_MS",
                                    static_cast<int>(c.http_timeout_ms));
    c.enable_tech_detection = env_bool("SIA_ENABLE_TECH_DETECTION",
                                       c.enable_tech_detection);
    c.log_level           = env_str("SIA_LOG_LEVEL", c.log_level);

    // 2) Optional JSON file overrides (path in SIA_CONFIG).
    const char* cfg_path = std::getenv("SIA_CONFIG");
    if (cfg_path && *cfg_path) {
        std::ifstream in(cfg_path);
        if (!in) {
            throw std::runtime_error(std::string("cannot open SIA_CONFIG: ") + cfg_path);
        }
        nlohmann::json j;
        in >> j;
        if (j.contains("mongodb_uri"))          c.mongodb_uri = j["mongodb_uri"].get<std::string>();
        if (j.contains("mongodb_database"))     c.mongodb_database = j["mongodb_database"].get<std::string>();
        if (j.contains("mongodb_collection"))   c.mongodb_collection = j["mongodb_collection"].get<std::string>();
        if (j.contains("hn_tags"))              c.hn_tags = j["hn_tags"].get<std::string>();
        if (j.contains("hn_hits"))              c.hn_hits = j["hn_hits"].get<int>();
        if (j.contains("http_user_agent"))      c.http_user_agent = j["http_user_agent"].get<std::string>();
        if (j.contains("http_timeout_ms"))      c.http_timeout_ms = j["http_timeout_ms"].get<long>();
        if (j.contains("enable_tech_detection")) c.enable_tech_detection = j["enable_tech_detection"].get<bool>();
        if (j.contains("log_level"))            c.log_level = j["log_level"].get<std::string>();
    }

    // 3) Fall back to assembling the URI from parts.
    if (c.mongodb_uri.empty()) {
        c.mongodb_uri = assemble_uri();
    }
    if (c.mongodb_uri.rfind("mongodb", 0) != 0) {
        throw std::runtime_error(
            "MongoDB URI required: set MONGODB_URI, or MONGODB_HOST/"
            "MONGODB_USERNAME/MONGODB_PASSWORD (see .env.example)");
    }
    if (c.mongodb_database.empty()) {
        throw std::runtime_error("MONGODB_DATABASE must not be empty");
    }
    return c;
}

}  // namespace sia
