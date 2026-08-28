# ============================================================================
# STAX — Dockerfile (Booster Suite Sandbox)
# @file    Dockerfile
# @author  shadcy
# @brief   Containerized build & emulation sandbox for STAX OS development.
#
# Part of the STAX Operating System.
#
# @license GPL-3.0-or-later
# Copyright (c) 2026 Shreyash Wanjari (Shadcy)
# ============================================================================

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Install ARM cross-compiler toolchain, QEMU with ARM support & GUI, build tools, and utilities
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    gcc-arm-none-eabi \
    binutils-arm-none-eabi \
    libnewlib-arm-none-eabi \
    gdb-multiarch \
    qemu-system-arm \
    qemu-system-gui \
    mtools \
    dosfstools \
    python3 \
    python3-pip \
    libx11-6 \
    libxext6 \
    libxrender1 \
    libxtst6 \
    libpulse0 \
    alsa-utils \
    git \
    sudo \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /stax

CMD ["bash"]
