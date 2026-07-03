#include "sia/http_client.hpp"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <utility>

#include <curl/curl.h>

#include "sia/logger.hpp"

namespace sia {
namespace {

// One-time global libcurl init/teardown.
struct CurlGlobal {
    CurlGlobal() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobal() { curl_global_cleanup(); }
};
void ensure_curl_global() {
    static std::once_flag flag;
    static CurlGlobal* g = nullptr;
    std::call_once(flag, [] { g = new CurlGlobal(); });
    (void)g;
}

size_t write_body(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

size_t write_header(char* buffer, size_t size, size_t nitems, void* userdata) {
    auto* headers = static_cast<std::map<std::string, std::string>*>(userdata);
    const size_t len = size * nitems;
    std::string line(buffer, len);
    auto colon = line.find(':');
    if (colon != std::string::npos) {
        std::string name = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        auto trim = [](std::string& s) {
            const char* ws = " \t\r\n";
            s.erase(0, s.find_first_not_of(ws));
            auto pos = s.find_last_not_of(ws);
            if (pos != std::string::npos) s.erase(pos + 1);
            else s.clear();
        };
        trim(name);
        trim(value);
        if (!name.empty()) (*headers)[name] = value;
    }
    return len;
}

}  // namespace

HttpClient::HttpClient(std::string user_agent, long timeout_ms)
    : user_agent_(std::move(user_agent)), timeout_ms_(timeout_ms) {
    ensure_curl_global();
}

HttpResponse HttpClient::get(const std::string& url) const {
    HttpResponse resp;
    CURL* curl = curl_easy_init();
    if (!curl) {
        log_error("curl_easy_init failed");
        return resp;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms_);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms_);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent_.c_str());
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");  // enable gzip/deflate
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &resp.headers);
    // Cap body size defensively (2 MiB) for enrichment fetches.
    curl_easy_setopt(curl, CURLOPT_MAXFILESIZE_LARGE, static_cast<curl_off_t>(2 * 1024 * 1024));

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        log_warn(std::string("GET failed: ") + url + " -> " + curl_easy_strerror(rc));
        resp.status = 0;
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status);
    }
    curl_easy_cleanup(curl);
    return resp;
}

}  // namespace sia
