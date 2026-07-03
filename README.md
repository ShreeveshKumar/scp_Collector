# Startup Intelligence Agent

An agent that runs **every 35 minutes** to build a proprietary **startup
intelligence database**.
Each run it discovers newly launched startups, extracts founders, fingerprints
their tech stack, detects funding signals, classifies industry, and stores
everything in **MongoDB**. Over months you accumulate a queryable dataset of who
launched what, built with which stack, in which industry, and who's funded.

Built in **C++20** (libcurl · mongocxx · nlohmann/json), with full CI/CD and
auto-deploy on push.

## Pipeline

```
 ┌────────────┐   ┌──────────────────────────────┐   ┌──────────────┐
 │  Sources   │──▶│         Enrichment           │──▶│   MongoDB    │
 │ (Show HN)  │   │ founders · tech · funding ·  │   │  (upsert on  │
 └────────────┘   │ industry                     │   │ external_id) │
                  └──────────────────────────────┘   └──────────────┘
```

- **Source:** Hacker News "Show HN" via the public Algolia API (no key needed).
- **Founders:** poster handle + names parsed from launch text.
- **Tech stack:** Wappalyzer-style fingerprinting of the site's headers + HTML.
- **Funding:** round/amount language detection.
- **Industry:** keyword classifier (fintech, ai_ml, devtools, …).
- **Storage:** idempotent upsert into MongoDB keyed on a stable `external_id`.

## Configure — just fill in `.env`

Copy `.env.example` to `.env` and set **either** a full connection string **or**
the parts:

```bash
# Option A (recommended, e.g. MongoDB Atlas):
MONGODB_URI=mongodb+srv://USER:PASS@cluster0.xxxxx.mongodb.net/?retryWrites=true&w=majority

# Option B (parts — a URI is assembled for you):
MONGODB_USERNAME=myuser
MONGODB_PASSWORD=mypass
MONGODB_HOST=localhost:27017
```

That's the only required configuration.

## Quick start (Docker Compose)

Starts MongoDB + the agent (every-35-minute loop), no local toolchain needed:

```bash
docker compose up --build
```

## Build & run locally

Requires a C++20 compiler, CMake ≥ 3.16, and dev packages for libcurl, the
MongoDB C++ driver (mongocxx/bsoncxx), and nlohmann/json.

```bash
# Debian/Ubuntu deps
sudo apt-get install -y build-essential cmake pkg-config \
     libcurl4-openssl-dev libmongocxx-dev libbsoncxx-dev nlohmann-json3-dev
# macOS (Homebrew)
brew install cmake curl mongo-cxx-driver nlohmann-json

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure          # unit tests

export MONGODB_URI='mongodb://localhost:27017'
./build/sia --check-db      # connect + ensure indexes
./build/sia --once          # one discovery pass
./build/sia --report        # stored-startup count
```

## CLI

| Flag | Meaning |
| ---- | ------- |
| `--once` | Single discovery pass (default). |
| `--interval <seconds>` | Run continuously (internal scheduler). |
| `--check-db` | Connect, ensure indexes, print count, exit. |
| `--report` | Print stored-startup summary. |
| `--log-level <lvl>` | `debug`\|`info`\|`warn`\|`error`. |
| `--config <path>` | JSON config file (see `config/config.example.json`). |

## CI/CD — push to deploy

- **CI** (`ci.yml`) — build + unit tests + MongoDB connectivity smoke on every push/PR.
- **Deploy** (`deploy.yml`) — on push to `main`: builds & pushes the Docker image
  to GHCR, then runs one live discovery pass against your MongoDB.
- **Scheduled** (`scheduled.yml`) — discovery run on a schedule. Cron can't do a
  true "every 35 min", so it fires at **:00 and :35** each hour; for an exact
  35-minute interval use the internal scheduler (`--interval 2100`) or systemd.

**One-time setup:** add a repository secret `MONGODB_URI` (your Atlas/self-hosted
connection string). After that, every `git push` to `main` deploys and runs.
Optional repo variables `MONGODB_DATABASE` / `MONGODB_COLLECTION` override the
defaults.

## Deploy targets (self-hosted alternatives)

The binary runs **one pass and exits** — scheduling is external:
- **Kubernetes:** [`deploy/k8s-cronjob.yaml`](deploy/k8s-cronjob.yaml)
- **systemd:** [`deploy/systemd/`](deploy/systemd/) (`sia.service` + `sia.timer`)
- **cron:** [`deploy/crontab.example`](deploy/crontab.example)

## Docs

- [ARCHITECTURE.md](ARCHITECTURE.md)
