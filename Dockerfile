FROM debian:trixie AS builder

ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /src

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       build-essential \
       cmake \
       file \
       pkg-config \
       libglib2.0-dev \
       libgtk-3-dev \
       libnotify-dev \
       libayatana-appindicator3-dev \
       libcurl4-openssl-dev \
       libsecret-1-dev \
       libxml2-dev \
    && rm -rf /var/lib/apt/lists/*

COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
       -DFRITZMONITOR_SERVICE_EXECUTABLE=%h/.local/bin/fritzmonitor \
    && cmake --build build --parallel \
    && ctest --test-dir build --output-on-failure \
    && test "$(./build/fritzmonitor --version)" = "FritzMonitor $(cat VERSION)"

FROM scratch AS artifact

COPY --from=builder /src/build/fritzmonitor /fritzmonitor
COPY --from=builder /src/build/fritzmonitor.service /fritzmonitor.service
COPY --from=builder /src/assets/fritzmonitor-phone-green.svg /fritzmonitor-phone-green.svg
COPY --from=builder /src/assets/fritzmonitor-phone-red.svg /fritzmonitor-phone-red.svg

CMD ["/fritzmonitor"]
