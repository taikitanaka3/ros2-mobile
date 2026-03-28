# ros2mobile

Stream your Android phone's sensors to ROS 2 in one command.

**[Documentation](https://taikitanaka3.github.io/ros2-mobile/)** | **[Privacy Policy](https://taikitanaka3.github.io/ros2-mobile/#/privacy-policy)**

```
Android Phone ──Zenoh──▶ Bridge Node ──▶ ROS 2 Topics ──▶ RViz 2
```

## Quick Start

```bash
git clone https://github.com/taikitanaka3/ros2-mobile.git
cd ros2mobile
cp .env.example .env
docker compose up
```

Install the **ROS2Mobile** app from Google Play, enter your host IP and port `7447`, then start streaming.

## Connection

### WiFi

Place the phone and the ROS 2 host on the same network. In the app, set the endpoint to `udp/<host-ip>:7447`.

### USB

```bash
adb reverse tcp:7447 tcp:7447
```

In the app, set the endpoint to `tcp/127.0.0.1:7447`.

## Supported Sensors

| Sensor | ROS 2 Message Type | Default Topic |
|--------|-------------------|---------------|
| IMU | `sensor_msgs/Imu` | `/ros2mobile/imu` |
| GPS | `sensor_msgs/NavSatFix` | `/ros2mobile/gps` |
| Camera | `sensor_msgs/Image` | `/ros2mobile/camera/image_raw` |
| Front Camera | `sensor_msgs/Image` | `/ros2mobile/front_camera/image_raw` |
| Battery | `sensor_msgs/BatteryState` | `/ros2mobile/battery` |
| Magnetometer | `sensor_msgs/MagneticField` | `/ros2mobile/magnetometer` |
| Barometer | `sensor_msgs/FluidPressure` | `/ros2mobile/barometer` |
| Joystick | `sensor_msgs/Joy` | `/ros2mobile/joy` |
| Infrared | `sensor_msgs/Range` | `/ros2mobile/infrared` |
| Thermal | `std_msgs/Float32` | `/ros2mobile/thermal` |

## License

MIT — see [LICENSE](LICENSE) for details.
