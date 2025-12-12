ARG BASE=debian:bookworm-slim
FROM ${BASE}

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      build-essential \
      ccache \
      git \
      qtbase5-dev qttools5-dev qttools5-dev-tools qtchooser qt5-qmake \
      libqt5serialport5-dev libqt5svg5-dev libqt5opengl5-dev \
      libudev-dev zlib1g-dev libgl1-mesa-dev \
      ruby ruby-dev rubygems \
      rpm ca-certificates && \
    gem install --no-document fpm && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

ENV CCACHE_DIR=/ccache
VOLUME /ccache
WORKDIR /workspace

CMD ["/bin/bash"]
