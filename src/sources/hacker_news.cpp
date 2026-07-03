#include "sia/sources/hacker_news.hpp"

#include <utility>

#include <nlohmann/json.hpp>

#include "sia/logger.hpp"

namespace sia {
namespace {

// "Show HN: Foo – a cool bar" -> "Foo"
std::string clean_name(const std::string& title) {
    std::string t = title;
    for (const char* prefix : {"Show HN:", "Show HN -", "Launch HN:"}) {
        auto pos = t.find(prefix);
        if (pos != std::string::npos) {
            t = t.substr(pos + std::string(prefix).size());
            break;
        }
    }
    // Trim leading whitespace.
    t.erase(0, t.find_first_not_of(" \t"));
    // Cut at the first separator that typically precedes a tagline.
    for (const char* sep : {" \xE2\x80\x93", " \xE2\x80\x94", " - ", ": ", " | "}) {
        auto pos = t.find(sep);
        if (pos != std::string::npos && pos > 0) {
            t = t.substr(0, pos);
        }
    }
    auto end = t.find_last_not_of(" \t");
    if (end != std::string::npos) t.erase(end + 1);
    return t.empty() ? title : t;
}

std::string get_str(const nlohmann::json& j, const char* key) {
    if (j.contains(key) && !j[key].is_null()) {
        if (j[key].is_string()) return j[key].get<std::string>();
    }
    return {};
}

}  // namespace

HackerNewsSource::HackerNewsSource(const HttpClient& http, std::string tags, int hits)
    : http_(http), tags_(std::move(tags)), hits_(hits) {}

std::vector<Startup> HackerNewsSource::fetch() const {
    std::vector<Startup> out;

    std::string url = "https://hn.algolia.com/api/v1/search_by_date?tags=" +
                      tags_ + "&hitsPerPage=" + std::to_string(hits_);
    HttpResponse r = http_.get(url);
    if (!r.ok()) {
        log_error("hacker_news: fetch failed, status=" + std::to_string(r.status));
        return out;
    }

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(r.body);
    } catch (const std::exception& e) {
        log_error(std::string("hacker_news: JSON parse error: ") + e.what());
        return out;
    }

    if (!j.contains("hits") || !j["hits"].is_array()) {
        log_warn("hacker_news: response had no hits array");
        return out;
    }

    for (const auto& hit : j["hits"]) {
        const std::string object_id = get_str(hit, "objectID");
        if (object_id.empty()) continue;

        Startup s;
        s.source = "hacker_news";
        s.external_id = "hn:" + object_id;
        const std::string title = get_str(hit, "title");
        s.name = clean_name(title);
        s.url = get_str(hit, "url");
        // Description feeds the classifiers: title + any self-post text.
        s.description = title;
        const std::string story_text = get_str(hit, "story_text");
        if (!story_text.empty()) s.description += " " + story_text;
        s.discovered_at = get_str(hit, "created_at");  // ISO-8601 from Algolia

        // Poster handle is the primary founder signal (enriched further later).
        const std::string author = get_str(hit, "author");
        if (!author.empty()) s.founders.push_back(author);

        out.push_back(std::move(s));
    }

    log_info("hacker_news: fetched " + std::to_string(out.size()) + " launches");
    return out;
}

}  // namespace sia
