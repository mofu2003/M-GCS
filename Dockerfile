# Reproducible Linux development and CI environment for M-GCS.
# This image builds/tests MgCore and runs Flutter analyze/test. Native
# macOS/Windows bundles remain on their respective host or CI runners.
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
ARG FLUTTER_VERSION=3.44.7

RUN apt-get update && apt-get install -y --no-install-recommends \
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
    zip \
    && rm -rf /var/lib/apt/lists/*

ENV FLUTTER_HOME=/opt/flutter
ENV PATH=/opt/flutter/bin:/opt/flutter/bin/cache/dart-sdk/bin:$PATH
ENV PUB_CACHE=/opt/flutter/.pub-cache
ENV CI=true

RUN curl -fsSL "https://storage.googleapis.com/flutter_infra_release/releases/stable/linux/flutter_linux_${FLUTTER_VERSION}-stable.tar.xz" \
    | tar -xJ -C /opt \
    && git config --system --add safe.directory /opt/flutter \
    && flutter config --no-analytics \
    && flutter precache --linux

WORKDIR /workspace
COPY docker/entrypoint.sh /usr/local/bin/mgcs-entrypoint
RUN chmod +x /usr/local/bin/mgcs-entrypoint

ENTRYPOINT ["/usr/local/bin/mgcs-entrypoint"]
CMD ["shell"]
