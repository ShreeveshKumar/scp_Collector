#include <chrono>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>
#include <thread>

#include "sia/config.hpp"
#include "sia/db.hpp"
#include "sia/http_client.hpp"
#include "sia/logger.hpp"
#include "sia/pipeline.hpp"

namespace {

void print_usage(const char* argv0) {
    std::cout <<
        "Startup Intelligence Agent\n"
        "Usage: " << argv0 << " [options]\n\n"
        "Options:\n"
        "  --once                 Run a single discovery pass (default).\n"
        "  --interval <seconds>   Run continuously every <seconds> (internal scheduler).\n"
        "  --check-db             Connect, ensure schema, print row count, exit.\n"
        "  --report               Print stored-startup summary and exit.\n"
        "  --log-level <level>    debug|info|warn|error (overrides config).\n"
        "  --config <path>        Path to a JSON config file (sets SIA_CONFIG).\n"
        "  -h, --help             Show this help.\n\n"
        "Configuration is read from the environment; set MONGODB_URI (or\n"
        "MONGODB_HOST/MONGODB_USERNAME/MONGODB_PASSWORD). See .env.example.\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::string mode = "once";
    long interval_seconds = 0;
    std::string log_level_override;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "missing value for " << name << "\n";
                std::exit(2);
            }
            return argv[++i];
        };
        if (arg == "--once") {
            mode = "once";
        } else if (arg == "--interval") {
            mode = "interval";
            interval_seconds = std::stol(next("--interval"));
        } else if (arg == "--check-db") {
            mode = "check-db";
        } else if (arg == "--report") {
            mode = "report";
        } else if (arg == "--log-level") {
            log_level_override = next("--log-level");
        } else if (arg == "--config") {
            setenv("SIA_CONFIG", next("--config").c_str(), 1);
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "unknown argument: " << arg << "\n";
            print_usage(argv[0]);
            return 2;
        }
    }

    try {
        sia::Config cfg = sia::Config::load();
        if (!log_level_override.empty()) cfg.log_level = log_level_override;
        sia::set_log_level(cfg.log_level);

        sia::Db db(cfg.mongodb_uri, cfg.mongodb_database, cfg.mongodb_collection);
        db.ensure_schema();

        if (mode == "check-db") {
            std::cout << "mongodb ok (" << cfg.mongodb_database << "."
                      << cfg.mongodb_collection << "); startups stored: "
                      << db.count() << "\n";
            return 0;
        }
        if (mode == "report") {
            std::cout << "Startup Intelligence — stored startups: " << db.count() << "\n";
            return 0;
        }

        sia::HttpClient http(cfg.http_user_agent, cfg.http_timeout_ms);
        sia::Pipeline pipeline(cfg, http, db);

        if (mode == "interval") {
            sia::log_info("starting internal scheduler, interval=" +
                          std::to_string(interval_seconds) + "s");
            for (;;) {
                pipeline.run_once();
                std::this_thread::sleep_for(std::chrono::seconds(interval_seconds));
            }
        }

        sia::Pipeline::Stats st = pipeline.run_once();
        return st.errors > 0 && st.inserted == 0 && st.updated == 0 ? 1 : 0;
    } catch (const std::exception& e) {
        sia::log_error(std::string("fatal: ") + e.what());
        return 1;
    }
}
