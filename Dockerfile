FROM ubuntu:22.04 AS base

SHELL ["/bin/bash", "-c"]

ENV project=meeting-sdk-linux-sample
ENV cwd=/tmp/$project
WORKDIR $cwd

ARG DEBIAN_FRONTEND=noninteractive

# Install GCC-12 and make it the default
RUN apt-get update  \
    && apt-get install -y software-properties-common \
    && add-apt-repository -y ppa:ubuntu-toolchain-r/test \
    && apt-get update \
    && apt-get install -y \
        build-essential \
        ca-certificates \
        cmake \
        curl \
        gdb \
        git \
        gfortran \
        gcc-12 \
        g++-12 \
        libopencv-dev \
        libdbus-1-3 \
        libgbm1 \
        libgl1-mesa-glx \
        libglib2.0-0 \
        libglib2.0-dev \
        libssl-dev \
        libx11-dev \
        libx11-xcb1 \
        libxcb-image0 \
        libxcb-keysyms1 \
        libxcb-randr0 \
        libxcb-shape0 \
        libxcb-shm0 \
        libxcb-xfixes0 \
        libxcb-xtest0 \
        libgl1-mesa-dri \
        libxfixes3 \
        linux-libc-dev \
        make \
        pkgconf \
        tar \
        unzip \
        zip

# Set GCC-12 as the default compiler
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 100 \
    && gcc --version \
    && g++ --version

# Install ALSA
RUN apt-get install -y libasound2 libasound2-plugins alsa alsa-utils alsa-oss

# Install Pulseaudio
RUN apt-get install -y pulseaudio pulseaudio-utils

FROM base AS deps

ENV TINI_VERSION v0.19.0
ADD https://github.com/krallin/tini/releases/download/${TINI_VERSION}/tini /tini
RUN chmod +x /tini

WORKDIR /opt
RUN git clone --depth 1 https://github.com/Microsoft/vcpkg.git \
    && ./vcpkg/bootstrap-vcpkg.sh -disableMetrics \
    && ln -s /opt/vcpkg/vcpkg /usr/local/bin/vcpkg \
    && vcpkg install vcpkg-cmake

FROM deps AS build

WORKDIR $cwd
COPY ./bin/entry.sh ./bin/entry.sh  # Copy the entry.sh file
RUN chmod +x ./bin/entry.sh

ENTRYPOINT ["/tini", "--", "./bin/entry.sh"]