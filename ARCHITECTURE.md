# Architecture

## Overview

A single-shot C++20 binary (`sia`) runs one **fetch → enrich → store** cycle per
invocation. External schedulers drive the every-35-minute cadence.

```
main.cpp
  └─ Config::load()            env (+ optional SIA_CONFIG json)
  └─ Db::ensure_schema()       ensure unique index on external_id
  └─ Pipeline::run_once()
        ├─ HackerNewsSource.fetch()          HTTP GET Algolia -> Startup[]
        ├─ for each Startup:
        │     ├─ FounderExtractor            poster handle + parsed names
        │     ├─ FundingDetector             round/amount language
        │     ├─ IndustryClassifier          keyword scoring
        │     └─ TechStackDetector           HTTP GET site -> signatures
        └─ Db::upsert()                       update_one(upsert) on external_id
```

## Components

| Unit | Files | Responsibility |
| ---- | ----- | -------------- |
| Config | `config.{hpp,cpp}` | Env + JSON config; builds a MongoDB URI from parts. |
| Logger | `logger.hpp` | Leveled, timestamped, thread-safe stderr logging. |
| HttpClient | `http_client.{hpp,cpp}` | libcurl GET; captures body + headers; errors as values. |
| Db | `db.{hpp,cpp}` | mongocxx (PImpl); index + idempotent upsert + count. |
| Source | `sources/hacker_news.{hpp,cpp}` | Show HN → `Startup[]`. |
| Enrichers | `enrich/*.hpp` (header-only) | Pure, testable founders/funding/industry/tech. |
| Pipeline | `pipeline.{hpp,cpp}` | Orchestrates a single pass; tallies `Stats`. |
| Entry | `main.cpp` | CLI, modes (`--once/--interval/--check-db/--report`). |

The reusable core lives in the `sia_core` library; `sia` and the test binary
link against it.

## Data model (MongoDB)

One collection (default `startup_intel.startups`), one document per startup:

```jsonc
{
  "external_id": "hn:48767885",     // "<source>:<id>", UNIQUE index — dedup key
  "source": "hacker_news",
  "name": "…", "url": "…", "description": "…",
  "founders":  ["handle", "Jane Doe"],
  "tech_stack": ["Next.js", "Vercel"],
  "industry": "ai_ml",
  "has_funding": false, "funding_note": "",
  "discovered_at": "2026-07-03T09:26:08Z",  // source timestamp (string)
  "created_at": ISODate, "updated_at": ISODate
}
```

Upsert: `update_one({external_id}, {$set: …, $setOnInsert: {created_at}}, upsert)`.
A fresh insert reports `upserted_count() > 0`; otherwise it's an update, so
re-runs refresh rather than duplicate. Useful queries:

```js
db.startups.find({ has_funding: true }).sort({ discovered_at: -1 })
db.startups.find({ tech_stack: "Next.js" })
db.startups.aggregate([{ $group: { _id: "$industry", n: { $sum: 1 } } }])
```

## Design choices

- **Enrichers are header-only and side-effect-free.** Network access is injected
  via `HttpClient*`, so `detect_from(...)` is unit-tested with no network/DB.
- **mongocxx confined by PImpl.** Driver headers live only in `db.cpp`; the rest
  of the build doesn't see them. The driver instance is initialised once.
- **Errors from I/O are values, not exceptions.** `HttpResponse::status == 0`
  means "request failed"; the pipeline catches per-item exceptions and counts
  them without aborting the run.
- **Idempotency via `external_id`.** Unique index + upsert; re-runs never dup.
- **Single-shot process.** Simpler and crash-safe; scheduling is external.

## External dependencies

libcurl (HTTP) · mongocxx + bsoncxx (MongoDB) · nlohmann/json (parsing) ·
CMake (build). Data source: Hacker News Algolia API (public, unauthenticated).

## Extending

- New discovery source → follow the source contract (see the enrichment/data
  specialists in the private `.claude/` module).
- New enrichment signal → add a header-only enricher + unit tests.
