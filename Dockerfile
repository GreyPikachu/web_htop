# syntax=docker/dockerfile:1.7

# Buildx selects the target architecture for this stage. Non-native targets
# are compiled under the QEMU worker configured by the container workflow.
FROM debian:bookworm-slim AS build

RUN apt-get update \
    && apt-get install --no-install-recommends -y \
        ca-certificates \
        cmake \
        g++ \
        ninja-build \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DWEB_HTOP_BUILD_APPS=ON \
        -DWEB_HTOP_BUILD_TESTS=OFF \
        -DWEB_HTOP_BUILD_LEGACY_TESTS=OFF \
    && cmake --build build --target web_htop_server --parallel \
    && strip build/server/web_htop_server

FROM debian:bookworm-slim AS runtime

RUN apt-get update \
    && apt-get install --no-install-recommends -y \
        libgcc-s1 \
        libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /src/build/server/web_htop_server /usr/local/bin/web_htop_server

USER 10001:10001
EXPOSE 8080 9999

ENTRYPOINT ["/usr/local/bin/web_htop_server"]
CMD ["--bind", "0.0.0.0"]
