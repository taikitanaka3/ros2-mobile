# ros2mobile

Android-to-ROS 2 sensor bridge — stream phone sensors to your robot

---

## Overview

ros2mobile turns an Android phone into a ROS 2 sensor source. The Android app
(distributed separately as a pre-built APK) collects data from the phone's
built-in sensors, transmits it over [Zenoh](https://zenoh.io) pub/sub
middleware, and a Python bridge node converts each payload into standard ROS 2
messages. An optional RViz 2 plugin provides live visualization panels.

**Supported sensors**: IMU, GPS (GNSS), Camera, Front Camera, Battery,
Magnetometer, Barometer, Joystick, Infrared, Thermal

---

## Architecture

```
Android Phone (ros2mobile app)
  [IMU / GPS / Camera / ...]
        |
        | JSON payloads (Zenoh pub/sub, port 7447)
        v
  Zenoh Router
        |
        v
  Bridge Node  (Python)
        |
        | ROS 2 messages
        v
  ROS 2 Topics
        |
        v
  RViz 2  (optional android_sensor_viz plugin)
```

Data flows one-way from the phone to ROS 2. The bridge node is stateless; it
validates each incoming JSON payload against a JSON schema, transforms it into
the appropriate ROS 2 message type, and publishes it on a configured topic.

---

## Quick Start

1. **Clone the repository**

   ```bash
   git clone https://github.com/your-org/ros2mobile.git
   cd ros2mobile
   ```

2. **Run setup**

   ```bash
   ./scripts/setup.sh
   ```

3. **Install the Android app**

   Download the latest APK from the [Releases](../../releases) page, then:

   ```bash
   adb install path/to/ros2mobile.apk
   ```

4. **Start Zenoh and the bridge node**

   ```bash
   # Docker Compose (recommended):
   docker compose up

   # Or run the bridge directly (requires a sourced ROS 2 workspace):
   ./scripts/start_bridge.sh
   ```

5. **Open RViz 2 with the plugin** (optional)

   Build the `android_sensor_viz` package (see
   [Building from Source](#building-from-source)), then add the
   `AndroidDashboardPanel` or individual sensor panels in RViz 2.

---

## Supported Sensors

| Sensor       | ROS 2 Message Type               | Default Topic                        |
|--------------|----------------------------------|--------------------------------------|
| IMU          | `sensor_msgs/Imu`                | `/ros2mobile/imu`                       |
| GPS          | `sensor_msgs/NavSatFix`          | `/ros2mobile/gps`                       |
| Camera       | `sensor_msgs/Image`              | `/ros2mobile/camera/image_raw`          |
| Front Camera | `sensor_msgs/Image`              | `/ros2mobile/front_camera/image_raw`    |
| Battery      | `sensor_msgs/BatteryState`       | `/ros2mobile/battery`                   |
| Magnetometer | `sensor_msgs/MagneticField`      | `/ros2mobile/magnetometer`              |
| Barometer    | `sensor_msgs/FluidPressure`      | `/ros2mobile/barometer`                 |
| Joystick     | `sensor_msgs/Joy`                | `/ros2mobile/joy`                       |
| Infrared     | `sensor_msgs/Range`              | `/ros2mobile/infrared`                  |
| Thermal      | `std_msgs/Float32`               | `/ros2mobile/thermal`                   |

Use `--all` to bind every stream with default topics, or `--binding` for
individual streams:

```bash
# All streams at once:
python3 -m bridge --all

# Shorthand — just the stream name:
python3 -m bridge --binding imu --binding gps

# Full form — custom Zenoh key and ROS topic:
python3 -m bridge --binding imu:ros2mobile/imu:/ros2mobile/imu
```

---

## Connection Setup

### WiFi

Place the phone and the ROS 2 host on the same network. Open the ros2mobile
app, enter the host's IP address and port `7447` in the connection settings,
then start streaming.

### USB (ADB port forwarding)

Connect the phone via USB and forward the Zenoh port:

```bash
adb reverse tcp:7447 tcp:7447
```

In the app, set the endpoint to `tcp/127.0.0.1:7447`. Traffic travels over the
USB link without needing a shared WiFi network.

---

## Building from Source

### Bridge node

Requirements: Python 3.10 or later.

```bash
pip install -r bridge/requirements.txt
```

### RViz 2 plugin

Requirements: a sourced ROS 2 workspace (Humble or later), Qt5.

```bash
# Symlink or copy rviz_plugin/ into your colcon workspace src/ directory, then:
colcon build --packages-select android_sensor_viz
source install/setup.bash
```

Load the default dashboard layout:

```bash
rviz2 -d rviz_plugin/config/android_dashboard.rviz
```

---

## Configuration

### Zenoh endpoint configs

Pre-configured connection profiles are provided in `config/`:

| File | Use case |
|------|----------|
| `zenoh-client-wifi.conf.json5`    | WiFi: phone and PC on the same network |
| `zenoh-client-usb.conf.json5`     | USB: ADB port forwarding               |
| `zenoh-server-android.conf.json5` | Android as Zenoh server                 |

### Environment variables

Copy `.env.example` to `.env` and customize:

```bash
cp .env.example .env
```

Key variables: `ZENOH_PORT`, `BRIDGE_BINDINGS` (see `.env.example` for details).

---

## Directory Structure

```
ros2mobile/
├── README.md
├── LICENSE
├── docker-compose.yml        # Zenoh + bridge (one command startup)
├── .env.example              # Environment variable template
├── bridge/                   # Python bridge node
│   ├── cli.py                #   CLI entry point
│   ├── node.py               #   Zenoh subscribe -> ROS 2 publish
│   ├── transform.py          #   Payload-to-ROS message conversion
│   ├── schema_validation.py  #   JSON schema validation
│   ├── schemas/              #   JSON schemas (internal)
│   └── requirements.txt      #   Runtime dependencies
├── rviz_plugin/              # RViz 2 visualization panels (C++/Qt)
│   ├── src/                  #   Panel and widget source files
│   ├── include/              #   Header files
│   ├── config/               #   RViz layout config
│   └── CMakeLists.txt / package.xml
├── config/                   # Zenoh connection profiles
└── scripts/                  # Setup and helper scripts
```

---

## License

MIT — see [LICENSE](LICENSE) for details.
