#ifndef ANDROID_SENSOR_VIZ_ANDROID_SENSOR_PANEL_HPP_
#define ANDROID_SENSOR_VIZ_ANDROID_SENSOR_PANEL_HPP_

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QTimer>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <array>
#include <memory>
#include <chrono>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/msg/nav_sat_fix.hpp>
#include <rviz_common/panel.hpp>

namespace android_sensor_viz
{
class AndroidImuPanel : public rviz_common::Panel
{
    Q_OBJECT

public:
    explicit AndroidImuPanel(QWidget *parent = nullptr);
    ~AndroidImuPanel() override;

    void onInitialize() override;
    void load(const rviz_common::Config &config) override;
    void save(rviz_common::Config config) const override;

private:
    void setupUi();
    void setupConnections();
    void initializeNode();
    void stopSubscription();
    void stopSpinning();
    void startSpinning();
    void subscribeTopics();
    void onTopicChanged();
    void onMessage(const sensor_msgs::msg::Imu::SharedPtr msg);
    void updateStatusLabel();
    static QString formatImuMessage(const sensor_msgs::msg::Imu &msg);
    static QString formatImuSummary(const sensor_msgs::msg::Imu &msg, double rate_hz);

    QString topic_{"/android/imu"};

    QLineEdit *topic_input_{nullptr};
    QLabel *summary_label_{nullptr};
    QLabel *data_label_{nullptr};
    QLabel *status_label_{nullptr};

    rclcpp::Node::SharedPtr node_;
    rclcpp::executors::SingleThreadedExecutor executor_;
    rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_subscription_;
    QTimer *spin_timer_{nullptr};
    QMetaObject::Connection spin_connection_;
    int message_count_{0};
    double message_rate_hz_{0.0};
    bool has_message_{false};
    std::chrono::steady_clock::time_point last_message_time_{};
    QString subscribed_topic_;
    bool initialized_{false};
    bool shutting_down_{false};

    // Sliding-window Hz
    static constexpr int kHzWindowSize = 20;
    std::array<std::chrono::steady_clock::time_point, kHzWindowSize> hz_window_{};
    int hz_write_{0};
    int hz_count_{0};
};

class AndroidGnssPanel : public rviz_common::Panel
{
    Q_OBJECT

public:
    explicit AndroidGnssPanel(QWidget *parent = nullptr);
    ~AndroidGnssPanel() override;

    void onInitialize() override;
    void load(const rviz_common::Config &config) override;
    void save(rviz_common::Config config) const override;

private:
    void setupUi();
    void setupConnections();
    void initializeNode();
    void stopSubscription();
    void stopSpinning();
    void startSpinning();
    void subscribeTopics();
    void onTopicChanged();
    void onMessage(const sensor_msgs::msg::NavSatFix::SharedPtr msg);
    void updateStatusLabel();
    static QString formatGnssMessage(const sensor_msgs::msg::NavSatFix &msg);
    static QString formatGnssSummary(const sensor_msgs::msg::NavSatFix &msg, double rate_hz);

    QString topic_{"/android/gps"};

    QLineEdit *topic_input_{nullptr};
    QLabel *summary_label_{nullptr};
    QLabel *data_label_{nullptr};
    QLabel *status_label_{nullptr};

    rclcpp::Node::SharedPtr node_;
    rclcpp::executors::SingleThreadedExecutor executor_;
    rclcpp::Subscription<sensor_msgs::msg::NavSatFix>::SharedPtr gps_subscription_;
    QTimer *spin_timer_{nullptr};
    QMetaObject::Connection spin_connection_;
    int message_count_{0};
    double message_rate_hz_{0.0};
    bool has_message_{false};
    std::chrono::steady_clock::time_point last_message_time_{};
    QString subscribed_topic_;
    bool initialized_{false};
    bool shutting_down_{false};

    // Sliding-window Hz
    static constexpr int kHzWindowSize = 20;
    std::array<std::chrono::steady_clock::time_point, kHzWindowSize> hz_window_{};
    int hz_write_{0};
    int hz_count_{0};
};

class AndroidCameraPanel : public rviz_common::Panel
{
    Q_OBJECT

public:
    explicit AndroidCameraPanel(QWidget *parent = nullptr);
    ~AndroidCameraPanel() override;

    void onInitialize() override;
    void load(const rviz_common::Config &config) override;
    void save(rviz_common::Config config) const override;

private:
    void setupUi();
    void setupConnections();
    void initializeNode();
    void stopSubscription();
    void stopSpinning();
    void startSpinning();
    void subscribeTopics();
    void onTopicChanged();
    void onMessage(const sensor_msgs::msg::Image::SharedPtr msg);
    void updateStatusLabel();
    static QString formatCameraMessage(const sensor_msgs::msg::Image &msg);
    static QString formatCameraSummary(const sensor_msgs::msg::Image &msg, double rate_hz);

    QString topic_{"/android/camera/image_raw"};

    QLineEdit *topic_input_{nullptr};
    QLabel *summary_label_{nullptr};
    QLabel *data_label_{nullptr};
    QLabel *status_label_{nullptr};

    rclcpp::Node::SharedPtr node_;
    rclcpp::executors::SingleThreadedExecutor executor_;
    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr camera_subscription_;
    QTimer *spin_timer_{nullptr};
    QMetaObject::Connection spin_connection_;
    int message_count_{0};
    double message_rate_hz_{0.0};
    bool has_message_{false};
    std::chrono::steady_clock::time_point last_message_time_{};
    QString subscribed_topic_;
    bool initialized_{false};
    bool shutting_down_{false};

    // Sliding-window Hz
    static constexpr int kHzWindowSize = 20;
    std::array<std::chrono::steady_clock::time_point, kHzWindowSize> hz_window_{};
    int hz_write_{0};
    int hz_count_{0};
};
}  // namespace android_sensor_viz

#endif  // ANDROID_SENSOR_VIZ_ANDROID_SENSOR_PANEL_HPP_
