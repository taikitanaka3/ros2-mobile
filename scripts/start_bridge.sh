#!/usr/bin/env bash
# start_bridge.sh — Start the ROS2Mobile bridge node with standard sensor bindings.
#
# Usage (run from ros2mobile/ root):
#   ./scripts/start_bridge.sh
#
# Topics:
#   Zenoh keys : ros2mobile/<sensor>
#   ROS topics : /ros2mobile/<sensor>

set -euo pipefail

# Source ROS 2 environment if setup.bash exists and rclpy is not already available.
if ! python3 -c "import rclpy" 2>/dev/null; then
    for setup in \
        "/opt/ros/jazzy/setup.bash" \
        "/opt/ros/humble/setup.bash" \
        "/opt/ros/iron/setup.bash" \
        "/opt/ros/rolling/setup.bash"
    do
        if [ -f "$setup" ]; then
            # shellcheck source=/dev/null
            source "$setup"
            echo "Sourced ROS 2 environment: $setup"
            break
        fi
    done
fi

# Resolve script directory so the command works regardless of cwd.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BRIDGE_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "Starting bridge"
echo "  Zenoh keys : ros2mobile/<sensor>"
echo "  ROS topics : /ros2mobile/<sensor>"
echo ""

cd "$BRIDGE_ROOT"

exec python3 -m bridge --all
