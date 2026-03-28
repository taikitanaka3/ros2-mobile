# ros2mobile

Turn your Android phone into a ROS 2 sensor hub.

```
  Android Phone              ROS 2 Host
 ┌──────────────┐      ┌─────────────────────┐
 │  ROS2Mobile  │      │   Zenoh Router      │
 │  (10 sensors)│─────▶│   Bridge Node       │──▶ ROS 2 Topics
 │              │Zenoh │   RViz 2 Plugin     │──▶ Visualization
 └──────────────┘      └─────────────────────┘
       WiFi / USB              docker compose up
```

## Quick Start

```bash
git clone https://github.com/taikitanaka3/ros2-mobile.git
cd ros2mobile
cp .env.example .env
docker compose up
```

Install **ROS2Mobile** from Google Play, connect to your host, then start streaming.

### WiFi

Place the phone and host on the same network. In the app, set the endpoint to `udp/<host-ip>:7447`.

### USB

```bash
adb reverse tcp:7447 tcp:7447
```

In the app, set the endpoint to `tcp/127.0.0.1:7447`. No shared WiFi needed.

### Verify

```bash
ros2 topic list
ros2 topic echo /ros2mobile/imu --once
```
