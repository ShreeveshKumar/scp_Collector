# ---- Build stage ----------------------------------------------------------
FROM ubuntu:24.04 AS build
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential cmake pkg-config git ca-certificates \
      libcurl4-openssl-dev libmongocxx-dev libbsoncxx-dev nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt ./
COPY include ./include
COPY src ./src
COPY tests ./tests
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && ctest --test-dir build --output-on-failure

# ---- Runtime stage --------------------------------------------------------
FROM ubuntu:24.04 AS runtime
ENV DEBIAN_FRONTEND=noninteractive
# Runtime libs only. ca-certificates is required for TLS to MongoDB Atlas.
RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates libcurl4 libmongocxx-dev \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /src/build/sia /usr/local/bin/sia
# Single-pass by default; scheduling is external (cron / systemd / k8s CronJob).
ENTRYPOINT ["/usr/local/bin/sia"]
CMD ["--once"]
