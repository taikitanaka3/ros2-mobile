#ifndef ANDROID_SENSOR_VIZ_ANDROID_DASHBOARD_PANEL_HPP_
#define ANDROID_SENSOR_VIZ_ANDROID_DASHBOARD_PANEL_HPP_

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <sensor_msgs/msg/nav_sat_fix.hpp>
#include <sensor_msgs/msg/battery_state.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/msg/joy.hpp>
#include <sensor_msgs/msg/magnetic_field.hpp>
#include <sensor_msgs/msg/fluid_pressure.hpp>
#include <sensor_msgs/msg/temperature.hpp>
#include <sensor_msgs/msg/range.hpp>
#include <rviz_common/panel.hpp>

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QProgressBar>
#include <QScrollArea>
#include <QTimer>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "android_sensor_viz/compass_widget.hpp"
#include "android_sensor_viz/gnss_position_widget.hpp"
#include "android_sensor_viz/imu_motion_widget.hpp"
#include "android_sensor_viz/joystick_widget.hpp"
#include "android_sensor_viz/magnetometer_widget.hpp"
#include "android_sensor_viz/sensor_pulse_widget.hpp"

namespace android_sensor_viz
{

// Forward declaration
class CompassWidget;

struct SensorState
{
    bool has_message{false};
    int message_count{0};
    double rate_hz{0.0};
    std::chrono::steady_clock::time_point last_time{};

    // Sliding-window Hz: keep timestamps of last kWindowSize arrivals.
    // Hz = (count - 1) / (newest - oldest) over the window.
    static constexpr int kWindowSize = 20;
    std::array<std::chrono::steady_clock::time_point, kWindowSize> window_{};
    int window_write_{0};   // next write position
    int window_count_{0};   // filled slots (0..kWindowSize)

    void recordMessage()
    {
        auto now = std::chrono::steady_clock::now();

        // Store arrival time in ring buffer
        window_[window_write_] = now;
        window_write_ = (window_write_ + 1) % kWindowSize;
        if (window_count_ < kWindowSize) {
            ++window_count_;
        }

        // Compute Hz from window when we have at least 2 samples
        if (window_count_ >= 2) {
            // oldest slot in the ring
            int oldest_idx = (window_write_ - window_count_ + kWindowSize) % kWindowSize;
            // newest slot is the one we just wrote (write_pos - 1)
            int newest_idx = (window_write_ - 1 + kWindowSize) % kWindowSize;
            auto span_us = std::chrono::duration_cast<std::chrono::microseconds>(
                window_[newest_idx] - window_[oldest_idx]).count();
            if (span_us > 0) {
                rate_hz = (static_cast<double>(window_count_ - 1) / static_cast<double>(span_us)) * 1e6;
            }
        }

        last_time = now;
        has_message = true;
        message_count++;
    }

    int64_t ageMs() const
    {
        if (!has_message) {
            return -1;
        }
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - last_time).count();
    }

    void resetRate()
    {
        rate_hz = 0.0;
        window_write_ = 0;
        window_count_ = 0;
        window_ = {};
    }
};

class AndroidDashboardPanel : public rviz_common::Panel
{
    Q_OBJECT

public:
    explicit AndroidDashboardPanel(QWidget *parent = nullptr);
    ~AndroidDashboardPanel() override;

    void onInitialize() override;
    void load(const rviz_common::Config &config) override;
    void save(rviz_common::Config config) const override;

private:
    void setupUi();
    void initializeNode();
    void subscribeAll();
    void resubscribeAll();
    void onPrefixChanged();

    // Message callbacks
    void onImu(const sensor_msgs::msg::Imu::SharedPtr msg);
    void onGps(const sensor_msgs::msg::NavSatFix::SharedPtr msg);
    void onBattery(const sensor_msgs::msg::BatteryState::SharedPtr msg);
    void onCamera(const sensor_msgs::msg::Image::SharedPtr msg);
    void onJoy(const sensor_msgs::msg::Joy::SharedPtr msg);
    void onMagnetometer(const sensor_msgs::msg::MagneticField::SharedPtr msg);
    void onBarometer(const sensor_msgs::msg::FluidPressure::SharedPtr msg);
    void onFrontCamera(const sensor_msgs::msg::Image::SharedPtr msg);
    void onThermal(const sensor_msgs::msg::Temperature::SharedPtr msg);
    void onInfrared(const sensor_msgs::msg::Range::SharedPtr msg);

    void updateHealthStrip();
    void startSpinning();
    void stopSpinning();

    // Topic prefix
    QString prefix_{"/android"};

    // ROS 2 node and executor
    rclcpp::Node::SharedPtr node_;
    rclcpp::executors::SingleThreadedExecutor executor_;

    // Subscriptions
    rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_sub_;
    rclcpp::Subscription<sensor_msgs::msg::NavSatFix>::SharedPtr gps_sub_;
    rclcpp::Subscription<sensor_msgs::msg::BatteryState>::SharedPtr battery_sub_;
    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr camera_sub_;
    rclcpp::Subscription<sensor_msgs::msg::Joy>::SharedPtr joy_sub_;
    rclcpp::Subscription<sensor_msgs::msg::MagneticField>::SharedPtr mag_sub_;
    rclcpp::Subscription<sensor_msgs::msg::FluidPressure>::SharedPtr baro_sub_;
    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr front_camera_sub_;
    rclcpp::Subscription<sensor_msgs::msg::Temperature>::SharedPtr thermal_sub_;
    rclcpp::Subscription<sensor_msgs::msg::Range>::SharedPtr ir_sub_;

    // Per-sensor state
    SensorState imu_state_;
    SensorState gps_state_;
    SensorState battery_state_;
    SensorState camera_state_;
    SensorState joy_state_;
    SensorState mag_state_;
    SensorState baro_state_;
    SensorState front_camera_state_;
    SensorState thermal_state_;
    SensorState ir_state_;

    // UI: topic prefix
    QLineEdit *prefix_input_{nullptr};

    // UI: health strip (10 sensors)
    QWidget *health_chip_[10]{};
    QLabel *health_dot_[10]{};
    QLabel *health_hz_[10]{};

    // UI: IMU compass + motion
    CompassWidget *compass_widget_{nullptr};
    ImuMotionWidget *imu_motion_widget_{nullptr};

    // UI: GNSS
    GnssPositionWidget *gnss_position_widget_{nullptr};
    QLabel *gnss_fix_label_{nullptr};

    // UI: battery
    QProgressBar *battery_bar_{nullptr};
    QLabel *battery_label_{nullptr};

    // UI: barometer
    QLabel *barometer_label_{nullptr};

    // UI: magnetometer
    MagnetometerWidget *magnetometer_widget_{nullptr};

    // UI: joystick
    JoystickWidget *joystick_widget_{nullptr};

    // UI: thermal
    QLabel *thermal_label_{nullptr};

    // UI: infrared
    QLabel *ir_label_{nullptr};

    // UI: sensor pulse graph
    SensorPulseWidget *pulse_widget_{nullptr};

    // UI: telemetry table
    QLabel *telemetry_label_{nullptr};

    // Spin timer
    QTimer *spin_timer_{nullptr};
    QMetaObject::Connection spin_connection_;

    bool initialized_{false};
    bool shutting_down_{false};
};

}  // namespace android_sensor_viz

#endif  // ANDROID_SENSOR_VIZ_ANDROID_DASHBOARD_PANEL_HPP_
