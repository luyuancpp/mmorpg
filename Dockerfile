# Development container for the C++ build toolchain.
# Not used for production — see deploy/k8s/ for runtime images.
#
# Usage:
#   docker build -t mmorpg-dev .
#   docker run -it --rm -v .:/workspace mmorpg-dev bash
FROM gcc:13

RUN apt-get update && apt-get -y --no-install-recommends install \
    build-essential \
    cmake \
    gdb \
    wget \
    make \
    libboost-dev \
    libssl-dev \
    vim \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace