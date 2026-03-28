# Privacy Policy

> Last updated: March 21, 2026

ROS2Mobile streams smartphone sensor data to ROS 2 robot systems. This policy explains how data is handled.

## Data We Collect

The App collects only the sensor data that the user explicitly enables.

| Data Type | Details | When Collected |
|-----------|---------|----------------|
| IMU | Acceleration, angular velocity, orientation | Only while enabled and streaming |
| GPS/GNSS | Latitude, longitude, altitude, accuracy | Only while enabled and streaming |
| Camera | Rear/front camera image frames | Only while enabled and streaming |
| Battery | Charge level, voltage, charging state | Only while enabled and streaming |
| Magnetometer | X/Y/Z magnetic flux density | Only while enabled and streaming |
| Barometer | Atmospheric pressure | Only while enabled and streaming |
| Thermometer | Device internal temperature | Only while enabled and streaming |
| Infrared | Proximity sensor distance | Only while enabled and streaming |
| Joystick | Virtual stick axis values, button states | Only while enabled and streaming |

## Where Data Is Sent

Sensor data is sent **only to the network endpoint specified by the user** (e.g., `udp/192.168.1.20:7447`) via the Zenoh protocol.

- **Never sent to external cloud services or third-party servers**
- The user has full control over the data destination

## On-Device Storage

The App stores settings, stream statistics, joystick state, and camera thumbnails in app-private internal storage, inaccessible to other apps. Camera thumbnails are deleted when streaming stops.

## In-App Purchases

An optional supporter subscription is available. Payment is handled entirely by Google Play — no payment information is stored in the App. All features are available regardless of subscription status.

## Permissions

| Permission | Purpose |
|------------|---------|
| Camera | Stream camera images as sensor data |
| Location | Stream GPS/GNSS data as sensor data |
| Internet | Data transmission via Zenoh protocol |
| Foreground Service | Continue streaming with screen off |
| Notifications | Display service status |

## Third-Party Data Sharing

The App does not share, sell, or provide data to any third parties. No analytics or advertising SDKs are included.

## Children's Privacy

This App is designed for robotics developers and researchers, and is not intended for children under 13.

## Changes

This policy may be updated without prior notice. Changes become effective when posted on this page.

Questions? Open an Issue on the [GitHub repository](https://github.com/taikitanaka3/ros2-mobile).
