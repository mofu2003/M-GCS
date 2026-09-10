# Reproducible Linux development and CI environment for M-GCS.
# This image builds/tests MgCore and runs Flutter analyze/test. Native
# macOS/Windows bundles remain on their respective host or CI runners.
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
ARG FLUTTER_VERSION=3.44.7
ARG FLUTTER_SHA256=a0edd646c159c0e816788c0e46a4f071199c1320495898f5a679599b583a05a4

RUN rm -f /etc/apt/apt.conf.d/docker-clean \
    && echo 'Binary::apt::APT::Keep-Downloaded-Packages "true";' \
      > /etc/apt/apt.conf.d/keep-downloads \
    && apt-get update \
    && apt_ok=0; \
      for attempt in 1 2 3 4 5 6 7 8 9 10; do \
        echo "APT install attempt ${attempt}/10"; \
        if apt-get install -y --no-install-recommends \
          -o Acquire::Retries=10 \
          ca-certificates \
          clang \
          cmake \
          curl \
          file \
          git \
          libglu1-mesa \
          libgtk-3-0 \
          libgtk-3-dev \
          liblzma-dev \
          ninja-build \
          pkg-config \
          python3 \
          python3-pip \
          python3-venv \
          unzip \
          xz-utils \
          zip; then \
          apt_ok=1; \
          break; \
        fi; \
        sleep 5; \
      done; \
      test "${apt_ok}" -eq 1 \
    && rm -rf /var/lib/apt/lists/*

ENV FLUTTER_HOME=/opt/flutter
ENV PATH=/opt/flutter/bin:/opt/flutter/bin/cache/dart-sdk/bin:$PATH
ENV PUB_CACHE=/opt/flutter/.pub-cache
ENV CI=true

RUN curl --retry 10 --retry-delay 5 --retry-all-errors -fL \
      -o /tmp/flutter.tar.xz \
      "https://storage.googleapis.com/flutter_infra_release/releases/stable/linux/flutter_linux_${FLUTTER_VERSION}-stable.tar.xz" \
    && echo "${FLUTTER_SHA256}  /tmp/flutter.tar.xz" | sha256sum -c - \
    && tar -xJf /tmp/flutter.tar.xz -C /opt \
    && rm /tmp/flutter.tar.xz \
    && git config --system --add safe.directory /opt/flutter \
    && flutter config --no-analytics \
    && flutter precache --linux

WORKDIR /workspace
COPY docker/entrypoint.sh /usr/local/bin/mgcs-entrypoint
RUN sed -i 's/\r$//' /usr/local/bin/mgcs-entrypoint \
    && chmod +x /usr/local/bin/mgcs-entrypoint

ENTRYPOINT ["/usr/local/bin/mgcs-entrypoint"]
CMD ["shell"]
