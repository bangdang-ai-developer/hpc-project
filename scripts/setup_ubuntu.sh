#!/usr/bin/env bash
set -euo pipefail

sudo apt update
sudo apt install -y build-essential openmpi-bin libopenmpi-dev python3 python3-docx rsync openssh-server

sudo systemctl enable --now ssh || true

echo "Required packages installed. Run: bash scripts/check_local_env.sh"
