#include "android_sensor_viz/android_sensor_panel.hpp"

#include <pluginlib/class_list_macros.hpp>
#include <builtin_interfaces/msg/time.hpp>

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QStringList>
#include <QTimer>
#include <QWidget>
#include <QMetaObject>
#include <algorithm>
#include <cmath>
#include <chrono>

namespace android_sensor_viz
{
namespace
{

QString formatStamp(const builtin_interfaces::msg::Time &stamp)
{
    return QString::number(stamp.sec) + "." + QString::number(stamp.nanosec).rightJustified(9, '0');
}

double norm3(double x, double y, double z)
{
    return std::sqrt((x * x) + (y * y) + (z * z));
}

QString gnssFixText(int status)
{
    if (status < 0) {
        return "NO_FIX";
    }
    if (status == 0) {
        return "FIX";
    }
    if (status == 1) {
        return "SBAS_FIX";
    }
    return "GBAS_FIX";
}

QString gnssServiceText(int service)
{
    if (service <= 0) {
        return "none";
    }
    QStringList parts;
    if (service & 0x01) {
        parts << "GPS";
    }
    if (service & 0x02) {
        parts << "GLONASS";
    }
    if (service & 0x04) {
        parts << "GALILEO";
    }
    if (service & 0x08) {
        parts << "BEIDOU";
    }
    if (parts.isEmpty()) {
        return QString::number(service);
    }
    return parts.join("+");
}

void applySummaryStyle(QLabel *label, const QString &text, const QString &background, const QString &border)
{
    if (!label) {
        return;
    }
    label->setStyleSheet(
        QString(
            "color: %1; background: %2; border: 1px solid %3; border-radius: 6px; "
            "padding: 8px; font-weight: 700;")
            .arg(text)
            .arg(background)
            .arg(border));
}

QWidget *buildInputGroup(
    QWidget *parent,
    const QString &title,
    const QString &topic,
    QLineEdit **topic_input,
    QLabel **summary_label,
    QLabel **value_label)
{
    auto *widget = new QWidget(parent);
    auto *layout = new QVBoxLayout(widget);

    auto *title_label = new QLabel(title);
    title_label->setStyleSheet("font-size: 14px; font-weight: 700;");

    auto *topic_label = new QLabel("Topic:");
    topic_label->setStyleSheet("color: #9e9e9e;");
    auto *topic_row = new QWidget();
    auto *topic_layout = new QHBoxLayout(topic_row);
    topic_layout->setContentsMargins(0, 0, 0, 0);
    *topic_input = new QLineEdit(topic);
    topic_layout->addWidget(topic_label);
    topic_layout->addWidget(*topic_input);

    *summary_label = new QLabel("No samples yet");
    (*summary_label)->setWordWrap(true);
    (*summary_label)->setStyleSheet(
        "color: #D4AF37; background: #2d2d2d; border: 1px solid #4a4a4a; border-radius: 6px;"
        "padding: 8px; font-weight: 600;");

    *value_label = new QLabel("Waiting for data");
    (*value_label)->setWordWrap(true);
    (*value_label)->setStyleSheet(
        "color: #e5e7eb; background: #1e1e1e; border: 1px solid #4a4a4a; border-radius: 6px;"
        "padding: 8px; font-family: monospace;");
    (*value_label)->setTextFormat(Qt::PlainText);

    layout->addWidget(title_label);
    layout->addWidget(topic_row);
    layout->addWidget(*summary_label);
    layout->addWidget(*value_label);
    layout->setSpacing(4);
    return widget;
}
}

AndroidImuPanel::AndroidImuPanel(QWidget *parent)
    : rviz_common::Panel(parent)
{
    spin_timer_ = new QTimer(this);
    setupUi();
    setupConnections();
}

AndroidImuPanel::~AndroidImuPanel()
{
    shutting_down_ = true;
    stopSpinning();
    stopSubscription();
    if (node_) {
        try {
            executor_.remove_node(node_);
        } catch (...) {
        }
        node_.reset();
    }
}

void AndroidImuPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(buildInputGroup(
        this,
        "IMU (/android/<device>/imu)",
        topic_,
        &topic_input_,
        &summary_label_,
        &data_label_));
    status_label_ = new QLabel("Waiting for IMU topic...");
    status_label_->setStyleSheet("color: #6b7280;");
    layout->addWidget(status_label_);
    setLayout(layout);
}

void AndroidImuPanel::setupConnections()
{
    connect(topic_input_, &QLineEdit::editingFinished, this, &AndroidImuPanel::onTopicChanged);
}

void AndroidImuPanel::onInitialize()
{
    if (shutting_down_) {
        return;
    }
    if (initialized_) {
        return;
    }

    if (!status_label_ || !topic_input_ || !data_label_) {
        return;
    }

    if (!rclcpp::ok()) {
        status_label_->setText("ROS 2 is not initialized.");
        return;
    }

    initializeNode();
    if (!node_) {
        return;
    }
    subscribeTopics();
    startSpinning();
    initialized_ = true;
}

void AndroidImuPanel::load(const rviz_common::Config &config)
{
    rviz_common::Panel::load(config);
    config.mapGetString("ImuTopic", &topic_);
    if (!topic_input_) {
        return;
    }
    topic_input_->setText(topic_);
    if (node_) {
        subscribeTopics();
    }
}

void AndroidImuPanel::save(rviz_common::Config config) const
{
    rviz_common::Panel::save(config);
    config.mapSetValue("ImuTopic", topic_);
}

void AndroidImuPanel::initializeNode()
{
    if (node_) {
        return;
    }
    try {
        node_ = std::make_shared<rclcpp::Node>("android_sensor_viz_imu_panel");
        executor_.add_node(node_);
    } catch (...) {
    if (status_label_) {
        status_label_->setText("Failed to create ROS node.");
    }
    }
}

void AndroidImuPanel::stopSubscription()
{
    imu_subscription_.reset();
    subscribed_topic_.clear();
}

void AndroidImuPanel::subscribeTopics()
{
    if (!node_) {
        return;
    }
    if (topic_ == subscribed_topic_) {
        return;
    }
    subscribed_topic_ = topic_;

    stopSubscription();

    if (!topic_.isEmpty()) {
        try {
            imu_subscription_ = node_->create_subscription<sensor_msgs::msg::Imu>(
                topic_.toStdString(),
                rclcpp::QoS(10).best_effort(),
                std::bind(&AndroidImuPanel::onMessage, this, std::placeholders::_1)
            );
            if (status_label_) {
                status_label_->setText(QString("Subscribing to IMU topic: %1").arg(topic_));
            }
        } catch (...) {
            imu_subscription_.reset();
            if (status_label_) {
                status_label_->setText(QString("Failed to subscribe IMU: %1").arg(topic_));
            }
        }
    }

    updateStatusLabel();
}

void AndroidImuPanel::onTopicChanged()
{
    if (shutting_down_) {
        return;
    }
    if (!topic_input_) {
        return;
    }
    topic_ = topic_input_->text().trimmed();
    if (topic_ == subscribed_topic_) {
        return;
    }
    subscribeTopics();
}

void AndroidImuPanel::onMessage(const sensor_msgs::msg::Imu::SharedPtr msg)
{
    if (shutting_down_ || !data_label_ || !msg) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();

    // Sliding-window Hz: store arrival time, then compute over window
    hz_window_[hz_write_] = now;
    hz_write_ = (hz_write_ + 1) % kHzWindowSize;
    if (hz_count_ < kHzWindowSize) {
        ++hz_count_;
    }
    if (hz_count_ >= 2) {
        int oldest_idx = (hz_write_ - hz_count_ + kHzWindowSize) % kHzWindowSize;
        int newest_idx = (hz_write_ - 1 + kHzWindowSize) % kHzWindowSize;
        auto span_us = std::chrono::duration_cast<std::chrono::microseconds>(
            hz_window_[newest_idx] - hz_window_[oldest_idx]).count();
        if (span_us > 0) {
            message_rate_hz_ = (static_cast<double>(hz_count_ - 1) / static_cast<double>(span_us)) * 1e6;
        }
    }

    last_message_time_ = now;
    has_message_ = true;

    if (summary_label_) {
        summary_label_->setText(formatImuSummary(*msg, message_rate_hz_));
        const double accel_norm = norm3(
            msg->linear_acceleration.x,
            msg->linear_acceleration.y,
            msg->linear_acceleration.z);
        if (std::isfinite(accel_norm) && accel_norm >= 8.0 && accel_norm <= 12.0 && message_rate_hz_ >= 2.0) {
            applySummaryStyle(summary_label_, "#d1fae5", "#0f2e27", "#34d399");
        } else if (message_rate_hz_ < 1.0) {
            applySummaryStyle(summary_label_, "#fecaca", "#341c1c", "#ef4444");
        } else {
            applySummaryStyle(summary_label_, "#fde68a", "#31250f", "#f59e0b");
        }
    }
    data_label_->setText(formatImuMessage(*msg));
    message_count_++;
}

QString AndroidImuPanel::formatImuMessage(const sensor_msgs::msg::Imu &msg)
{
    return QString("frame_id: %1\n"
                   "stamp: %2\n"
                   "orientation: [x=%3 y=%4 z=%5 w=%6]\n"
                   "gyro rad/s: [x=%7 y=%8 z=%9]\n"
                   "accel m/s2: [x=%10 y=%11 z=%12]")
        .arg(QString::fromStdString(msg.header.frame_id))
        .arg(formatStamp(msg.header.stamp))
        .arg(msg.orientation.x, 0, 'f', 4).arg(msg.orientation.y, 0, 'f', 4).arg(msg.orientation.z, 0, 'f', 4)
        .arg(msg.orientation.w, 0, 'f', 4)
        .arg(msg.angular_velocity.x, 0, 'f', 4).arg(msg.angular_velocity.y, 0, 'f', 4).arg(msg.angular_velocity.z, 0, 'f', 4)
        .arg(msg.linear_acceleration.x, 0, 'f', 4).arg(msg.linear_acceleration.y, 0, 'f', 4).arg(msg.linear_acceleration.z, 0, 'f', 4);
}

QString AndroidImuPanel::formatImuSummary(const sensor_msgs::msg::Imu &msg, double rate_hz)
{
    constexpr double kRadToDeg = 57.29577951308232;
    const double x = msg.orientation.x;
    const double y = msg.orientation.y;
    const double z = msg.orientation.z;
    const double w = msg.orientation.w;
    const double sinr_cosp = 2.0 * ((w * x) + (y * z));
    const double cosr_cosp = 1.0 - (2.0 * ((x * x) + (y * y)));
    const double roll_deg = std::atan2(sinr_cosp, cosr_cosp) * kRadToDeg;
    const double sinp = 2.0 * ((w * y) - (z * x));
    const double pitch_deg = std::asin(std::clamp(sinp, -1.0, 1.0)) * kRadToDeg;
    const double siny_cosp = 2.0 * ((w * z) + (x * y));
    const double cosy_cosp = 1.0 - (2.0 * ((y * y) + (z * z)));
    const double yaw_deg = std::atan2(siny_cosp, cosy_cosp) * kRadToDeg;
    const double accel_norm = norm3(
        msg.linear_acceleration.x,
        msg.linear_acceleration.y,
        msg.linear_acceleration.z);
    const double gyro_norm = norm3(
        msg.angular_velocity.x,
        msg.angular_velocity.y,
        msg.angular_velocity.z);

    return QString(
               "Rate: %1 Hz  |  Tilt R/P/Y: %2 / %3 / %4 deg\n"
               "Accel norm: %5 m/s2  |  Gyro norm: %6 rad/s")
        .arg(rate_hz, 0, 'f', 1)
        .arg(roll_deg, 0, 'f', 1)
        .arg(pitch_deg, 0, 'f', 1)
        .arg(yaw_deg, 0, 'f', 1)
        .arg(accel_norm, 0, 'f', 2)
        .arg(gyro_norm, 0, 'f', 3);
}

void AndroidImuPanel::updateStatusLabel()
{
    if (shutting_down_) {
        return;
    }
    if (!status_label_) {
        return;
    }
    if (topic_.isEmpty()) {
        status_label_->setText("IMU topic is not set.");
        return;
    }

    if (!has_message_) {
        status_label_->setText(QString("Listening for IMU data (topic=%1)").arg(topic_));
        status_label_->setStyleSheet("color: #9ca3af;");
        return;
    }

    const auto age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - last_message_time_).count();
    QString freshness = "fresh";
    QString color = "#22c55e";
    if (age_ms > 5000) {
        freshness = "stale";
        color = "#ef4444";
        message_rate_hz_ = 0.0;
    } else if (age_ms > 2000) {
        freshness = "aging";
        color = "#f59e0b";
    }

    status_label_->setStyleSheet(QString("color: %1; font-weight: 600;").arg(color));
    status_label_->setText(
        QString("IMU %1 | %2 msgs | %3 Hz | age=%4 ms | topic=%5")
            .arg(freshness)
            .arg(message_count_)
            .arg(message_rate_hz_, 0, 'f', 1)
            .arg(age_ms)
            .arg(topic_));
}

void AndroidImuPanel::startSpinning()
{
    if (shutting_down_) {
        return;
    }
    if (!spin_timer_ || !node_) {
        return;
    }
    stopSpinning();

    spin_timer_->setInterval(30);
    spin_connection_ = connect(spin_timer_, &QTimer::timeout, this, [this]() {
        if (!node_ || !rclcpp::ok()) {
            return;
        }
        try {
            executor_.spin_some();
            updateStatusLabel();
        } catch (...) {
        }
    });
    spin_timer_->start();
}

void AndroidImuPanel::stopSpinning()
{
    if (spin_timer_ && spin_timer_->isActive()) {
        spin_timer_->stop();
    }
    if (spin_connection_) {
        QObject::disconnect(spin_connection_);
        spin_connection_ = QMetaObject::Connection();
    }
}

AndroidGnssPanel::AndroidGnssPanel(QWidget *parent)
    : rviz_common::Panel(parent)
{
    spin_timer_ = new QTimer(this);
    setupUi();
    setupConnections();
}

AndroidGnssPanel::~AndroidGnssPanel()
{
    shutting_down_ = true;
    stopSpinning();
    stopSubscription();
    if (node_) {
        try {
            executor_.remove_node(node_);
        } catch (...) {
        }
        node_.reset();
    }
}

void AndroidGnssPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(buildInputGroup(
        this,
        "GNSS / GPS (/android/<device>/gps)",
        topic_,
        &topic_input_,
        &summary_label_,
        &data_label_
    ));
    status_label_ = new QLabel("Waiting for GNSS topic...");
    status_label_->setStyleSheet("color: #6b7280;");
    layout->addWidget(status_label_);
    setLayout(layout);
}

void AndroidGnssPanel::setupConnections()
{
    connect(topic_input_, &QLineEdit::editingFinished, this, &AndroidGnssPanel::onTopicChanged);
}

void AndroidGnssPanel::onInitialize()
{
    if (shutting_down_) {
        return;
    }
    if (initialized_) {
        return;
    }

    if (!status_label_ || !topic_input_ || !data_label_) {
        return;
    }

    if (!rclcpp::ok()) {
        status_label_->setText("ROS 2 is not initialized.");
        return;
    }

    initializeNode();
    if (!node_) {
        return;
    }
    subscribeTopics();
    startSpinning();
    initialized_ = true;
}

void AndroidGnssPanel::load(const rviz_common::Config &config)
{
    rviz_common::Panel::load(config);
    config.mapGetString("GnssTopic", &topic_);
    if (!topic_input_) {
        return;
    }
    topic_input_->setText(topic_);
    if (node_) {
        subscribeTopics();
    }
}

void AndroidGnssPanel::save(rviz_common::Config config) const
{
    rviz_common::Panel::save(config);
    config.mapSetValue("GnssTopic", topic_);
}

void AndroidGnssPanel::initializeNode()
{
    if (node_) {
        return;
    }
    try {
        node_ = std::make_shared<rclcpp::Node>("android_sensor_viz_gnss_panel");
        executor_.add_node(node_);
    } catch (...) {
        if (status_label_) {
            status_label_->setText("Failed to create ROS node.");
        }
    }
}

void AndroidGnssPanel::stopSubscription()
{
    gps_subscription_.reset();
    subscribed_topic_.clear();
}

void AndroidGnssPanel::subscribeTopics()
{
    if (!node_) {
        return;
    }
    if (topic_ == subscribed_topic_) {
        return;
    }
    subscribed_topic_ = topic_;

    stopSubscription();

    if (!topic_.isEmpty()) {
        try {
            gps_subscription_ = node_->create_subscription<sensor_msgs::msg::NavSatFix>(
                topic_.toStdString(),
                rclcpp::QoS(10).best_effort(),
                std::bind(&AndroidGnssPanel::onMessage, this, std::placeholders::_1)
            );
            if (status_label_) {
                status_label_->setText(QString("Subscribing to GNSS topic: %1").arg(topic_));
            }
        } catch (...) {
            gps_subscription_.reset();
            if (status_label_) {
                status_label_->setText(QString("Failed to subscribe GNSS: %1").arg(topic_));
            }
        }
    }

    updateStatusLabel();
}

void AndroidGnssPanel::onTopicChanged()
{
    if (shutting_down_) {
        return;
    }
    if (!topic_input_) {
        return;
    }
    topic_ = topic_input_->text().trimmed();
    if (topic_ == subscribed_topic_) {
        return;
    }
    subscribeTopics();
}

void AndroidGnssPanel::onMessage(const sensor_msgs::msg::NavSatFix::SharedPtr msg)
{
    if (shutting_down_ || !summary_label_ || !data_label_ || !msg) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();

    // Sliding-window Hz: store arrival time, then compute over window
    hz_window_[hz_write_] = now;
    hz_write_ = (hz_write_ + 1) % kHzWindowSize;
    if (hz_count_ < kHzWindowSize) {
        ++hz_count_;
    }
    if (hz_count_ >= 2) {
        int oldest_idx = (hz_write_ - hz_count_ + kHzWindowSize) % kHzWindowSize;
        int newest_idx = (hz_write_ - 1 + kHzWindowSize) % kHzWindowSize;
        auto span_us = std::chrono::duration_cast<std::chrono::microseconds>(
            hz_window_[newest_idx] - hz_window_[oldest_idx]).count();
        if (span_us > 0) {
            message_rate_hz_ = (static_cast<double>(hz_count_ - 1) / static_cast<double>(span_us)) * 1e6;
        }
    }

    last_message_time_ = now;
    has_message_ = true;
    summary_label_->setText(formatGnssSummary(*msg, message_rate_hz_));
    double horizontal_sigma = -1.0;
    if (msg->position_covariance[0] >= 0.0 && msg->position_covariance[4] >= 0.0) {
        horizontal_sigma = std::sqrt((msg->position_covariance[0] + msg->position_covariance[4]) / 2.0);
    }
    const bool has_fix = msg->status.status >= 0;
    if (!has_fix) {
        applySummaryStyle(summary_label_, "#fecaca", "#341c1c", "#ef4444");
    } else if (horizontal_sigma >= 0.0 && horizontal_sigma <= 10.0) {
        applySummaryStyle(summary_label_, "#d1fae5", "#0f2e27", "#34d399");
    } else {
        applySummaryStyle(summary_label_, "#fde68a", "#31250f", "#f59e0b");
    }
    data_label_->setText(formatGnssMessage(*msg));
    message_count_++;
}

QString AndroidGnssPanel::formatGnssMessage(const sensor_msgs::msg::NavSatFix &msg)
{
    return QString("frame_id: %1\n"
                   "stamp: %2\n"
                   "lat: %3 deg\n"
                   "lon: %4 deg\n"
                   "alt: %5 m\n"
                   "status: %6\n"
                   "service: %7")
        .arg(QString::fromStdString(msg.header.frame_id))
        .arg(formatStamp(msg.header.stamp))
        .arg(msg.latitude, 0, 'f', 6).arg(msg.longitude, 0, 'f', 6)
        .arg(msg.altitude, 0, 'f', 2)
        .arg(msg.status.status).arg(msg.status.service);
}

QString AndroidGnssPanel::formatGnssSummary(const sensor_msgs::msg::NavSatFix &msg, double rate_hz)
{
    double horizontal_sigma = -1.0;
    if (msg.position_covariance[0] >= 0.0 && msg.position_covariance[4] >= 0.0) {
        horizontal_sigma = std::sqrt((msg.position_covariance[0] + msg.position_covariance[4]) / 2.0);
    }

    QString quality = "unknown";
    if (horizontal_sigma >= 0.0) {
        if (horizontal_sigma <= 3.0) {
            quality = "excellent";
        } else if (horizontal_sigma <= 10.0) {
            quality = "good";
        } else if (horizontal_sigma <= 30.0) {
            quality = "coarse";
        } else {
            quality = "poor";
        }
    }

    return QString(
               "Rate: %1 Hz  |  Fix: %2 (%3)  |  Quality: %4\n"
               "Horizontal sigma: %5 m  |  Altitude: %6 m")
        .arg(rate_hz, 0, 'f', 1)
        .arg(gnssFixText(msg.status.status))
        .arg(gnssServiceText(msg.status.service))
        .arg(quality)
        .arg(horizontal_sigma >= 0.0 ? QString::number(horizontal_sigma, 'f', 2) : "--")
        .arg(msg.altitude, 0, 'f', 2);
}

void AndroidGnssPanel::updateStatusLabel()
{
    if (shutting_down_) {
        return;
    }
    if (!status_label_) {
        return;
    }
    if (topic_.isEmpty()) {
        status_label_->setText("GNSS topic is not set.");
        return;
    }

    if (!has_message_) {
        status_label_->setText(QString("Listening for GNSS data (topic=%1)").arg(topic_));
        status_label_->setStyleSheet("color: #9ca3af;");
        return;
    }

    const auto age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - last_message_time_).count();
    QString freshness = "fresh";
    QString color = "#22c55e";
    if (age_ms > 5000) {
        freshness = "stale";
        color = "#ef4444";
        message_rate_hz_ = 0.0;
    } else if (age_ms > 2000) {
        freshness = "aging";
        color = "#f59e0b";
    }
    status_label_->setStyleSheet(QString("color: %1; font-weight: 600;").arg(color));
    status_label_->setText(
        QString("GNSS %1 | %2 msgs | %3 Hz | age=%4 ms | topic=%5")
            .arg(freshness)
            .arg(message_count_)
            .arg(message_rate_hz_, 0, 'f', 1)
            .arg(age_ms)
            .arg(topic_)
    );
}

void AndroidGnssPanel::startSpinning()
{
    if (shutting_down_) {
        return;
    }
    if (!spin_timer_ || !node_) {
        return;
    }
    stopSpinning();

    spin_timer_->setInterval(30);
    spin_connection_ = connect(spin_timer_, &QTimer::timeout, this, [this]() {
        if (!node_ || !rclcpp::ok()) {
            return;
        }
        try {
            executor_.spin_some();
            updateStatusLabel();
        } catch (...) {
        }
    });
    spin_timer_->start();
}

void AndroidGnssPanel::stopSpinning()
{
    if (spin_timer_ && spin_timer_->isActive()) {
        spin_timer_->stop();
    }
    if (spin_connection_) {
        QObject::disconnect(spin_connection_);
        spin_connection_ = QMetaObject::Connection();
    }
}

AndroidCameraPanel::AndroidCameraPanel(QWidget *parent)
    : rviz_common::Panel(parent)
{
    spin_timer_ = new QTimer(this);
    setupUi();
    setupConnections();
}

AndroidCameraPanel::~AndroidCameraPanel()
{
    shutting_down_ = true;
    stopSpinning();
    stopSubscription();
    if (node_) {
        try {
            executor_.remove_node(node_);
        } catch (...) {
        }
        node_.reset();
    }
}

void AndroidCameraPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(buildInputGroup(
        this,
        "Camera (/android/<device>/camera)",
        topic_,
        &topic_input_,
        &summary_label_,
        &data_label_));
    status_label_ = new QLabel("Waiting for Camera topic...");
    status_label_->setStyleSheet("color: #6b7280;");
    layout->addWidget(status_label_);
    setLayout(layout);
}

void AndroidCameraPanel::setupConnections()
{
    connect(topic_input_, &QLineEdit::editingFinished, this, &AndroidCameraPanel::onTopicChanged);
}

void AndroidCameraPanel::onInitialize()
{
    if (shutting_down_) {
        return;
    }
    if (initialized_) {
        return;
    }

    if (!status_label_ || !topic_input_ || !data_label_) {
        return;
    }

    if (!rclcpp::ok()) {
        status_label_->setText("ROS 2 is not initialized.");
        return;
    }

    initializeNode();
    if (!node_) {
        return;
    }
    subscribeTopics();
    startSpinning();
    initialized_ = true;
}

void AndroidCameraPanel::load(const rviz_common::Config &config)
{
    rviz_common::Panel::load(config);
    config.mapGetString("CameraTopic", &topic_);
    if (!topic_input_) {
        return;
    }
    topic_input_->setText(topic_);
    if (node_) {
        subscribeTopics();
    }
}

void AndroidCameraPanel::save(rviz_common::Config config) const
{
    rviz_common::Panel::save(config);
    config.mapSetValue("CameraTopic", topic_);
}

void AndroidCameraPanel::initializeNode()
{
    if (node_) {
        return;
    }
    try {
        node_ = std::make_shared<rclcpp::Node>("android_sensor_viz_camera_panel");
        executor_.add_node(node_);
    } catch (...) {
        if (status_label_) {
            status_label_->setText("Failed to create ROS node.");
        }
    }
}

void AndroidCameraPanel::stopSubscription()
{
    camera_subscription_.reset();
    subscribed_topic_.clear();
}

void AndroidCameraPanel::subscribeTopics()
{
    if (!node_) {
        return;
    }
    if (topic_ == subscribed_topic_) {
        return;
    }
    subscribed_topic_ = topic_;

    stopSubscription();

    if (!topic_.isEmpty()) {
        try {
            camera_subscription_ = node_->create_subscription<sensor_msgs::msg::CompressedImage>(
                topic_.toStdString(),
                rclcpp::QoS(10).best_effort(),
                std::bind(&AndroidCameraPanel::onMessage, this, std::placeholders::_1)
            );
            if (status_label_) {
                status_label_->setText(QString("Subscribing to Camera topic: %1").arg(topic_));
            }
        } catch (...) {
            camera_subscription_.reset();
            if (status_label_) {
                status_label_->setText(QString("Failed to subscribe Camera: %1").arg(topic_));
            }
        }
    }

    updateStatusLabel();
}

void AndroidCameraPanel::onTopicChanged()
{
    if (shutting_down_) {
        return;
    }
    if (!topic_input_) {
        return;
    }
    topic_ = topic_input_->text().trimmed();
    if (topic_ == subscribed_topic_) {
        return;
    }
    subscribeTopics();
}

void AndroidCameraPanel::onMessage(const sensor_msgs::msg::CompressedImage::SharedPtr msg)
{
    if (shutting_down_ || !summary_label_ || !data_label_ || !msg) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();

    // Sliding-window Hz: store arrival time, then compute over window
    hz_window_[hz_write_] = now;
    hz_write_ = (hz_write_ + 1) % kHzWindowSize;
    if (hz_count_ < kHzWindowSize) {
        ++hz_count_;
    }
    if (hz_count_ >= 2) {
        int oldest_idx = (hz_write_ - hz_count_ + kHzWindowSize) % kHzWindowSize;
        int newest_idx = (hz_write_ - 1 + kHzWindowSize) % kHzWindowSize;
        auto span_us = std::chrono::duration_cast<std::chrono::microseconds>(
            hz_window_[newest_idx] - hz_window_[oldest_idx]).count();
        if (span_us > 0) {
            message_rate_hz_ = (static_cast<double>(hz_count_ - 1) / static_cast<double>(span_us)) * 1e6;
        }
    }

    last_message_time_ = now;
    has_message_ = true;

    summary_label_->setText(formatCameraSummary(*msg, message_rate_hz_));
    if (message_rate_hz_ >= 2.0) {
        applySummaryStyle(summary_label_, "#d1fae5", "#0f2e27", "#34d399");
    } else if (message_rate_hz_ >= 1.0) {
        applySummaryStyle(summary_label_, "#fde68a", "#31250f", "#f59e0b");
    } else {
        applySummaryStyle(summary_label_, "#fecaca", "#341c1c", "#ef4444");
    }
    data_label_->setText(formatCameraMessage(*msg));
    message_count_++;
}

QString AndroidCameraPanel::formatCameraMessage(const sensor_msgs::msg::CompressedImage &msg)
{
    return QString("frame_id: %1\n"
                   "stamp: %2\n"
                   "format: %3\n"
                   "data size: %4 bytes")
        .arg(QString::fromStdString(msg.header.frame_id))
        .arg(formatStamp(msg.header.stamp))
        .arg(QString::fromStdString(msg.format))
        .arg(msg.data.size());
}

QString AndroidCameraPanel::formatCameraSummary(const sensor_msgs::msg::CompressedImage &msg, double rate_hz)
{
    return QString(
               "Rate: %1 Hz  |  Format: %2\n"
               "Data size: %3 bytes")
        .arg(rate_hz, 0, 'f', 1)
        .arg(QString::fromStdString(msg.format))
        .arg(msg.data.size());
}

void AndroidCameraPanel::updateStatusLabel()
{
    if (shutting_down_) {
        return;
    }
    if (!status_label_) {
        return;
    }
    if (topic_.isEmpty()) {
        status_label_->setText("Camera topic is not set.");
        return;
    }

    if (!has_message_) {
        status_label_->setText(QString("Listening for Camera data (topic=%1)").arg(topic_));
        status_label_->setStyleSheet("color: #9ca3af;");
        return;
    }

    const auto age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - last_message_time_).count();
    QString freshness = "fresh";
    QString color = "#22c55e";
    if (age_ms > 5000) {
        freshness = "stale";
        color = "#ef4444";
        message_rate_hz_ = 0.0;
    } else if (age_ms > 2000) {
        freshness = "aging";
        color = "#f59e0b";
    }
    status_label_->setStyleSheet(QString("color: %1; font-weight: 600;").arg(color));
    status_label_->setText(
        QString("Camera %1 | %2 msgs | %3 Hz | age=%4 ms | topic=%5")
            .arg(freshness)
            .arg(message_count_)
            .arg(message_rate_hz_, 0, 'f', 1)
            .arg(age_ms)
            .arg(topic_)
    );
}

void AndroidCameraPanel::startSpinning()
{
    if (shutting_down_) {
        return;
    }
    if (!spin_timer_ || !node_) {
        return;
    }
    stopSpinning();

    spin_timer_->setInterval(30);
    spin_connection_ = connect(spin_timer_, &QTimer::timeout, this, [this]() {
        if (!node_ || !rclcpp::ok()) {
            return;
        }
        try {
            executor_.spin_some();
            updateStatusLabel();
        } catch (...) {
        }
    });
    spin_timer_->start();
}

void AndroidCameraPanel::stopSpinning()
{
    if (spin_timer_ && spin_timer_->isActive()) {
        spin_timer_->stop();
    }
    if (spin_connection_) {
        QObject::disconnect(spin_connection_);
        spin_connection_ = QMetaObject::Connection();
    }
}

}  // namespace android_sensor_viz

PLUGINLIB_EXPORT_CLASS(android_sensor_viz::AndroidImuPanel, rviz_common::Panel)
PLUGINLIB_EXPORT_CLASS(android_sensor_viz::AndroidGnssPanel, rviz_common::Panel)
PLUGINLIB_EXPORT_CLASS(android_sensor_viz::AndroidCameraPanel, rviz_common::Panel)
