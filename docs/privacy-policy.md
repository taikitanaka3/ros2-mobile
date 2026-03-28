# ROS2Mobile Privacy Policy

> Last updated: March 21, 2026

## 1. Introduction

ROS2Mobile ("the App") is an open-source tool for real-time streaming of smartphone sensor data to ROS 2 robot systems. This Privacy Policy explains what data the App collects and how it is used.

## 2. Data We Collect

The App collects only the sensor data that the user explicitly enables. Each sensor can be individually toggled on or off.

| Data Type | Details | When Collected |
|-----------|---------|----------------|
| IMU (Inertial) | Acceleration, angular velocity, orientation | Only while IMU is enabled and streaming |
| Location (GPS/GNSS) | Latitude, longitude, altitude, accuracy | Only while GPS is enabled and streaming |
| Camera | Rear/front camera image frames | Only while camera is enabled and streaming |
| Battery | Charge level, voltage, charging state | Only while battery is enabled and streaming |
| Magnetometer | X/Y/Z magnetic flux density | Only while magnetometer is enabled and streaming |
| Barometer | Atmospheric pressure | Only while barometer is enabled and streaming |
| Thermometer | Device internal temperature | Only while thermometer is enabled and streaming |
| Infrared/Proximity | Proximity sensor distance | Only while infrared is enabled and streaming |
| Joystick Input | Virtual stick axis values, button states | Only while joystick is enabled and streaming |

## 3. Where Data Is Sent

Sensor data is sent **only to the network endpoint specified by the user** in the Settings screen (e.g., `udp/192.168.1.20:7447`) via the Zenoh protocol over the local network.

- Data is **never sent to external cloud services or third-party servers**
- The user has full control over the data destination
- Data is only transmitted over the internet if the user intentionally specifies a remote endpoint

## 4. On-Device Data Storage

The App stores the following data in the device's internal storage (app-private area), inaccessible to other apps:

- **Settings**: Sensor enable/disable states, transmission intervals, connection endpoint, camera resolution
- **Stream statistics**: Transmission counts, error info (for UI display)
- **Joystick state**: Virtual stick axis values, button states
- **Camera thumbnails**: Temporary image files for UI preview (deleted when streaming stops)
- **Supporter status**: In-app subscription presence (boolean only)

## 5. In-App Purchases

The App offers an optional supporter monthly subscription. Payment processing is handled entirely by Google Play's billing system; no payment information is stored or processed within the App. There are no feature restrictions based on subscription status.

## 6. Permissions Required

| Permission | Purpose |
|------------|---------|
| Camera | To stream camera images as sensor data |
| Location (Fine & Coarse) | To stream GPS/GNSS data as sensor data |
| Internet | For data transmission via Zenoh protocol |
| Foreground Service | To continue streaming with the screen off |
| Notifications | To display foreground service status |

## 7. Third-Party Data Sharing

The App does not share, sell, or provide collected data to any third parties (ad networks, analytics services, etc.). The App contains no analytics or advertising SDKs.

## 8. Children's Privacy

The App is a technical tool designed for robotics developers and researchers, and is not intended for children under 13.

## 9. Open Source

The App's source code is publicly available on GitHub. You can directly review how data is handled by examining the source code.

## 10. Contact

For questions about this Privacy Policy, please open an Issue on the [GitHub repository](https://github.com/taikitanaka3/ros2-mobile).

## 11. Changes to This Policy

This Privacy Policy may be updated without prior notice. Changes become effective when posted on this page.

---

# ROS2Mobile プライバシーポリシー

> 最終更新日: 2026年3月21日

## 1. はじめに

ROS2Mobile（以下「本アプリ」）は、スマートフォンのセンサーデータを ROS 2 ロボットシステムにリアルタイム配信するためのオープンソースツールです。本プライバシーポリシーは、本アプリがどのようなデータを収集・使用するかを説明します。

## 2. 収集するデータ

本アプリは、ユーザーが明示的に有効にしたセンサーデータのみを収集します。各センサーは個別にオン/オフを切り替えることができます。

| データの種類 | 内容 | 収集タイミング |
|-------------|------|---------------|
| IMU（慣性計測） | 加速度、角速度、姿勢 | ユーザーが IMU を有効にし、ストリーミング中のみ |
| 位置情報（GPS/GNSS） | 緯度、経度、高度、精度 | ユーザーが GPS を有効にし、ストリーミング中のみ |
| カメラ映像 | 背面/前面カメラの画像フレーム | ユーザーがカメラを有効にし、ストリーミング中のみ |
| バッテリー情報 | 充電率、電圧、充電状態 | ユーザーがバッテリーを有効にし、ストリーミング中のみ |
| 磁気センサー | X/Y/Z 磁束密度 | ユーザーが磁気センサーを有効にし、ストリーミング中のみ |
| 気圧計 | 大気圧 | ユーザーが気圧計を有効にし、ストリーミング中のみ |
| 温度計 | デバイス内部温度 | ユーザーが温度計を有効にし、ストリーミング中のみ |
| 赤外線/近接 | 近接センサー距離 | ユーザーが赤外線を有効にし、ストリーミング中のみ |
| ジョイスティック入力 | 仮想スティックの軸値、ボタン状態 | ユーザーがジョイスティックを有効にし、ストリーミング中のみ |

## 3. データの送信先

センサーデータは、**ユーザーが設定画面で指定したネットワークエンドポイント**（例: `udp/192.168.1.20:7447`）にのみ、Zenoh プロトコルを使用してローカルネットワーク経由で送信されます。

- データは**外部クラウドやサードパーティのサーバーには一切送信されません**
- データの送信先はユーザーが完全に制御します
- インターネット経由での送信は、ユーザーが意図的にインターネット上のエンドポイントを指定した場合にのみ発生します

## 4. デバイス上のデータ保存

本アプリは以下のデータをデバイス内部ストレージ（アプリ専用領域）に保存します。これらは他のアプリからアクセスできません。

- **設定情報**: センサーの有効/無効、送信間隔、接続先エンドポイント、カメラ解像度
- **ストリーム統計**: 送信カウント、エラー情報（UI 表示用）
- **ジョイスティック状態**: 仮想スティックの軸値、ボタン状態
- **カメラサムネイル**: UI プレビュー用の一時画像ファイル（ストリーミング停止時に削除）
- **サポーター状態**: アプリ内課金のサブスクリプション有無（有無のみ）

## 5. アプリ内課金

本アプリは任意のサポーター月額サブスクリプションを提供しています。課金処理は Google Play の課金システムを通じて行われ、支払い情報はアプリ内に保存・処理されません。サブスクリプションの有無によるアプリ機能の制限はありません。

## 6. 必要なパーミッション

| パーミッション | 用途 |
|--------------|------|
| カメラ | カメラ映像をセンサーデータとして配信するため |
| 位置情報（精密・概算） | GPS/GNSS データをセンサーデータとして配信するため |
| インターネット | Zenoh プロトコルによるデータ送信のため |
| フォアグラウンドサービス | 画面消灯中もセンサーデータの配信を継続するため |
| 通知 | フォアグラウンドサービスの実行状態を表示するため |

## 7. 第三者へのデータ提供

本アプリは、収集したデータを第三者（広告ネットワーク、アナリティクスサービス等）に提供・販売・共有しません。アプリにはアナリティクス SDK や広告 SDK は含まれていません。

## 8. 子供のプライバシー

本アプリはロボット開発者・研究者向けの技術ツールであり、13歳未満の子供を対象としていません。

## 9. オープンソース

本アプリのソースコードは GitHub で公開されています。データの取り扱いについてはソースコードを直接確認いただけます。

## 10. お問い合わせ

本プライバシーポリシーに関するご質問は、[GitHub リポジトリ](https://github.com/taikitanaka3/ros2-mobile)の Issue にてお問い合わせください。

## 11. ポリシーの変更

本プライバシーポリシーは予告なく変更される場合があります。変更後のポリシーは本ページに掲載された時点で効力を生じます。
