// Dependency-free unit tests for the enrichment stage.
// Exit code 0 = all passed; non-zero = failures (CTest reads this).
#include <iostream>
#include <string>

#include "sia/enrich/founders.hpp"
#include "sia/enrich/funding.hpp"
#include "sia/enrich/industry.hpp"
#include "sia/enrich/tech_stack.hpp"

namespace {
int g_failures = 0;

void check(bool cond, const std::string& what) {
    if (cond) {
        std::cout << "  ok   " << what << "\n";
    } else {
        std::cout << "  FAIL " << what << "\n";
        ++g_failures;
    }
}

bool has(const std::vector<std::string>& v, const std::string& x) {
    for (const auto& e : v) if (e == x) return true;
    return false;
}
}  // namespace

int main() {
    using namespace sia::enrich;

    std::cout << "IndustryClassifier\n";
    {
        IndustryClassifier c;
        check(c.classify("A new payment and banking wallet for crypto") == "fintech",
              "classifies fintech");
        check(c.classify("Open source SDK and CLI for developers") == "devtools",
              "classifies devtools");
        check(c.classify("An LLM agent framework with neural models") == "ai_ml",
              "classifies ai_ml");
        check(c.classify("just some random words here") == "unknown",
              "unknown when no keywords");
    }

    std::cout << "FundingDetector\n";
    {
        FundingDetector f;
        check(f.detect("We raised a $2M seed round led by Acme").detected,
              "detects seed round");
        check(f.detect("Backed by Y Combinator").detected,
              "detects backed-by");
        check(!f.detect("A simple weekend project, no revenue").detected,
              "no false positive");
    }

    std::cout << "FounderExtractor\n";
    {
        FounderExtractor e;
        auto r = e.extract("Hi, I'm Jane Doe and I built this.", "janedoe");
        check(has(r, "janedoe"), "includes poster handle");
        check(has(r, "Jane Doe"), "parses name from text");
    }

    std::cout << "TechStackDetector\n";
    {
        TechStackDetector t;  // no HttpClient -> pure matching
        auto r = t.detect_from("nginx", "Express",
                               "<div id=\"__next_data__\"></div> wp-content js.stripe.com");
        check(has(r, "Nginx"), "detects Nginx from server header");
        check(has(r, "Express"), "detects Express from x-powered-by");
        check(has(r, "Next.js"), "detects Next.js from body");
        check(has(r, "WordPress"), "detects WordPress from body");
        check(has(r, "Stripe"), "detects Stripe from body");
        check(t.detect_from("", "", "").empty(), "empty in -> empty out");
    }

    std::cout << (g_failures == 0 ? "\nALL TESTS PASSED\n"
                                  : "\nTESTS FAILED\n");
    return g_failures == 0 ? 0 : 1;
}
