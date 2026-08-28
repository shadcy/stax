#!/usr/bin/env bash
# ============================================================================
# STAX — docker-run.sh (Booster Suite Launcher)
# @file    docker-run.sh
# @author  shadcy
# @brief   Launches the containerized STAX sandbox with GUI forwarding.
#
# Part of the STAX Operating System.
#
# @license GPL-3.0-or-later
# Copyright (c) 2026 Shreyash Wanjari (Shadcy)
# ============================================================================
set -e

IMAGE_NAME="stax-sandbox"

# Build image if not present or requested
if [[ "$(docker images -q ${IMAGE_NAME} 2> /dev/null)" == "" ]]; then
    echo "[*] Building Docker image ${IMAGE_NAME}..."
    docker build -t ${IMAGE_NAME} .
fi

# Allow local X11 connections for GUI (QEMU window)
xhost +local:docker > /dev/null 2>&1 || true

# Run container with X11 forwarding and workspace mount
# Starting (Environment for STAX os dev) sandbox container...
echo "[*] Booster Suite"
docker run -it --rm \
    --net=host \
    -e DISPLAY="${DISPLAY}" \
    -v /tmp/.X11-unix:/tmp/.X11-unix:rw \
    -v "$(pwd)":/stax \
    -w /stax \
    ${IMAGE_NAME} "$@"
