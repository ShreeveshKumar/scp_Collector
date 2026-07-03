#include "sia/pipeline.hpp"

#include <exception>
#include <string>

#include "sia/enrich/founders.hpp"
#include "sia/enrich/funding.hpp"
#include "sia/enrich/industry.hpp"
#include "sia/enrich/tech_stack.hpp"
#include "sia/logger.hpp"
#include "sia/sources/hacker_news.hpp"

namespace sia {

Pipeline::Stats Pipeline::run_once() {
    Stats stats;

    HackerNewsSource source(http_, cfg_.hn_tags, cfg_.hn_hits);
    std::vector<Startup> startups = source.fetch();
    stats.fetched = static_cast<int>(startups.size());

    enrich::FounderExtractor founders;
    enrich::FundingDetector funding;
    enrich::IndustryClassifier industry;
    enrich::TechStackDetector tech(cfg_.enable_tech_detection ? &http_ : nullptr);

    for (auto& s : startups) {
        try {
            // Founders: poster handle + names parsed from launch text.
            const std::string poster = s.founders.empty() ? std::string{} : s.founders.front();
            s.founders = founders.extract(s.description, poster);

            // Funding signal.
            auto f = funding.detect(s.description);
            s.has_funding = f.detected;
            s.funding_note = f.note;

            // Industry classification.
            s.industry = industry.classify(s.description);

            // Tech stack (network fetch of the startup's site, if enabled).
            s.tech_stack = tech.detect(s.url);

            if (s.has_funding || !s.tech_stack.empty()) ++stats.enriched;

            if (db_.upsert(s)) {
                ++stats.inserted;
                log_info("inserted " + s.external_id + " \"" + s.name +
                         "\" [" + s.industry + "]");
            } else {
                ++stats.updated;
                log_debug("updated " + s.external_id);
            }
        } catch (const std::exception& e) {
            ++stats.errors;
            log_error("failed processing " + s.external_id + ": " + e.what());
        }
    }

    log_info("run complete: fetched=" + std::to_string(stats.fetched) +
             " inserted=" + std::to_string(stats.inserted) +
             " updated=" + std::to_string(stats.updated) +
             " enriched=" + std::to_string(stats.enriched) +
             " errors=" + std::to_string(stats.errors));
    return stats;
}

}  // namespace sia
