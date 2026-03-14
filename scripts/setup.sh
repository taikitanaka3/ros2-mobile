#!/usr/bin/env bash
# setup.sh — ros2mobile environment setup
#
# Run this script from the ros2mobile/ root directory:
#   ./scripts/setup.sh
#
# What it does:
#   1. Checks that required tools are installed
#   2. Installs Python dependencies for the bridge
#   3. Prints next steps

set -euo pipefail

# ── Helpers ──────────────────────────────────────────────────────────────────

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

info()    { echo -e "${GREEN}[INFO]${NC}  $*"; }
warn()    { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error()   { echo -e "${RED}[ERROR]${NC} $*" >&2; }
die()     { error "$*"; exit 1; }

# ── 1. Check required tools ───────────────────────────────────────────────────

info "Checking required tools..."

check_tool() {
    local cmd="$1"
    local install_hint="$2"
    if command -v "$cmd" &>/dev/null; then
        info "  $cmd ... OK ($(command -v "$cmd"))"
    else
        die "$cmd not found. $install_hint"
    fi
}

check_tool python3    "Install Python 3: https://www.python.org/downloads/"
check_tool pip3       "Install pip: https://pip.pypa.io/en/stable/installation/"
check_tool docker     "Install Docker: https://docs.docker.com/get-docker/"

# docker compose can be either the standalone binary or the Docker plugin
if docker compose version &>/dev/null 2>&1; then
    info "  docker compose ... OK (plugin)"
elif command -v docker-compose &>/dev/null; then
    info "  docker-compose ... OK (standalone)"
else
    die "docker compose not found. Install the Compose plugin: https://docs.docker.com/compose/install/"
fi

# ── 2. Install Python dependencies ───────────────────────────────────────────

REQUIREMENTS="bridge/requirements.txt"

if [ ! -f "$REQUIREMENTS" ]; then
    die "$REQUIREMENTS not found. Make sure you are running this script from the ros2mobile/ root directory."
fi

info "Installing Python dependencies from $REQUIREMENTS..."
pip3 install -r "$REQUIREMENTS"

# ── 3. Success message ────────────────────────────────────────────────────────

echo ""
info "Setup complete!"
echo ""
echo "Next steps:"
echo "  1. Start the Zenoh router and bridge:"
echo "       docker compose up"
echo ""
echo "  2. Or run the bridge directly (requires sourced ROS 2 workspace):"
echo "       ./scripts/start_bridge.sh"
echo ""
echo "  3. Install the Android app (download APK from Releases):"
echo "       adb install path/to/ros2mobile.apk"
echo ""
