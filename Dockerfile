# ---- Build stage ----------------------------------------------------------
# Ubuntu does not package the MongoDB C++ driver (only the C driver), so we
# build mongo-cxx-driver from source. r3.11+ auto-downloads the matching C
# driver, so this is self-contained.
FROM ubuntu:22.04 AS build
ENV DEBIAN_FRONTEND=noninteractive
ARG MONGOCXX_VERSION=r3.11.0
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential cmake git ca-certificates pkg-config \
      libssl-dev libsasl2-dev zlib1g-dev \
      libcurl4-openssl-dev nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

# Build & install the MongoDB C++ driver into /usr/local.
RUN git clone --depth 1 --branch "$MONGOCXX_VERSION" \
      https://github.com/mongodb/mongo-cxx-driver.git /tmp/mcd \
    && cmake -S /tmp/mcd -B /tmp/mcd/build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DMONGOCXX_OVERRIDE_DEFAULT_INSTALL_PREFIX=OFF \
        -DENABLE_TESTS=OFF \
    && cmake --build /tmp/mcd/build --parallel \
    && cmake --install /tmp/mcd/build \
    && rm -rf /tmp/mcd

WORKDIR /src
COPY CMakeLists.txt ./
COPY include ./include
COPY src ./src
COPY tests ./tests
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr/local \
    && cmake --build build --parallel \
    && ctest --test-dir build --output-on-failure \
    && cmake --install build

# ---- Runtime stage --------------------------------------------------------
FROM ubuntu:22.04 AS runtime
ENV DEBIAN_FRONTEND=noninteractive
# Runtime libs only. ca-certificates is required for TLS to MongoDB Atlas.
RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates libssl3 libsasl2-2 zlib1g libcurl4 \
    && rm -rf /var/lib/apt/lists/*

# Bring over the driver shared libs (mongocxx/bsoncxx + the fetched mongoc/bson)
# and the built binary; refresh the linker cache.
COPY --from=build /usr/local/lib/ /usr/local/lib/
COPY --from=build /usr/local/bin/sia /usr/local/bin/sia
RUN ldconfig

# Single-pass by default; scheduling is external (cron / systemd / k8s CronJob).
ENTRYPOINT ["/usr/local/bin/sia"]
CMD ["--once"]
