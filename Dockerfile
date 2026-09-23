# Reproducible Linux build/test environment for the M-GCS backend.
# The image only builds and tests MgCore; it does not contain any frontend.
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

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
          git \
          liblzma-dev \
          ninja-build \
          pkg-config \
          python3 \
          python3-pip \
          python3-venv; then \
          apt_ok=1; \
          break; \
        fi; \
        sleep 5; \
      done; \
      test "${apt_ok}" -eq 1 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY docker/entrypoint.sh /usr/local/bin/mgcs-entrypoint
RUN sed -i 's/\r$//' /usr/local/bin/mgcs-entrypoint \
    && chmod +x /usr/local/bin/mgcs-entrypoint

ENTRYPOINT ["/usr/local/bin/mgcs-entrypoint"]
CMD ["shell"]
