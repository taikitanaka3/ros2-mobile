#!/usr/bin/env bash
# start_bridge.sh — Start the ROS2Mobile bridge node with standard sensor bindings.
#
# Usage (run from ros2mobile/ root):
#   ./scripts/start_bridge.sh [device_name]
#
# Arguments:
#   device_name   Optional. Android device identifier used in Zenoh keys and
#                 ROS topic paths (default: device1).
#
# Examples:
#   ./scripts/start_bridge.sh
#   ./scripts/start_bridge.sh phone1

set -euo pipefail

DEVICE="${1:-device1}"

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

echo "Starting bridge for device: $DEVICE"
echo "  Zenoh keys : android/$DEVICE/<sensor>"
echo "  ROS topics : /android/$DEVICE/<sensor>"
echo ""

cd "$BRIDGE_ROOT"

exec python3 -m bridge \
    --binding "imu:android/${DEVICE}/imu:/android/${DEVICE}/imu" \
    --binding "gps:android/${DEVICE}/gps:/android/${DEVICE}/gps" \
    --binding "battery:android/${DEVICE}/battery:/android/${DEVICE}/battery" \
    --binding "camera:android/${DEVICE}/camera:/android/${DEVICE}/camera" \
    --binding "magnetometer:android/${DEVICE}/magnetometer:/android/${DEVICE}/magnetometer" \
    --binding "barometer:android/${DEVICE}/barometer:/android/${DEVICE}/barometer" \
    --binding "joy:android/${DEVICE}/joy:/android/${DEVICE}/joy"
