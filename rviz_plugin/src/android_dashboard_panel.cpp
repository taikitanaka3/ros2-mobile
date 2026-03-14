#include "android_sensor_viz/android_dashboard_panel.hpp"

#include "android_sensor_viz/gnss_position_widget.hpp"
#include "android_sensor_viz/imu_motion_widget.hpp"
#include "android_sensor_viz/joystick_widget.hpp"
#include "android_sensor_viz/magnetometer_widget.hpp"
#include "android_sensor_viz/sensor_pulse_widget.hpp"
#include <pluginlib/class_list_macros.hpp>

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdio>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace android_sensor_viz
{

namespace
{

// Sensor indices for health strip (matches Android SensorPulseGraphView topic order)
constexpr int kIdxImu = 0;
constexpr int kIdxGps = 1;
constexpr int kIdxCam = 2;
constexpr int kIdxBat = 3;
constexpr int kIdxMag = 4;
constexpr int kIdxBaro = 5;
constexpr int kIdxJoy = 6;
constexpr int kIdxFcam = 7;
constexpr int kIdxThm = 8;
constexpr int kIdxIR = 9;

constexpr int kNumSensors = 10;

const char *kSensorNames[] = {"IMU", "GPS", "RCAM", "BAT", "MAG", "BAR", "JOY", "FCAM", "THM", "IR"};

// Style constants — neutral dark gray (no navy/blue)
const QString kPanelBg = "#1e1e1e";
const QString kSectionBg = "#2d2d2d";
const QString kSectionBorder = "#4a4a4a";
const QString kTextColor = "#e0e0e0";
const QString kSubtitleColor = "#9e9e9e";
const QString kAccentGold = "#D4AF37";
const QString kDotGreen = "#22c55e";
const QString kDotAmber = "#f59e0b";
const QString kDotRed = "#ef4444";
const QString kDotGray = "#4b5563";

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

QString sectionStyle(bool compact = false)
{
    int radius = compact ? 4 : 6;
    int padding = compact ? 4 : 8;
    return QString("background: %1; border: 1px solid %2; border-radius: %3px; padding: %4px;")
        .arg(kSectionBg)
        .arg(kSectionBorder)
        .arg(radius)
        .arg(padding);
}

QString dotStyle(const QString &color)
{
    return QString(
        "background: %1; border-radius: 5px; min-width: 10px; max-width: 10px; "
        "min-height: 10px; max-height: 10px;")
        .arg(color);
}

QString chipStyle(const QString &dotColor)
{
    return QString(
        "background: #252525; border: 1px solid %1; border-radius: 4px; padding: 2px;")
        .arg(dotColor);
}

QLabel *makeSectionTitle(const QString &text)
{
    auto *label = new QLabel(text);
    label->setStyleSheet(
        QString("color: %1; font-size: 12px; font-weight: 700; border: none; padding: 0px;")
            .arg(kAccentGold));
    return label;
}

QLabel *makeDataLabel(const QString &initial_text)
{
    auto *label = new QLabel(initial_text);
    label->setWordWrap(true);
    label->setStyleSheet(
        QString("color: %1; font-family: monospace; font-size: 11px; border: none; padding: 2px;")
            .arg(kTextColor));
    return label;
}

}  // namespace

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

AndroidDashboardPanel::AndroidDashboardPanel(QWidget *parent)
    : rviz_common::Panel(parent)
{
    spin_timer_ = new QTimer(this);
    setupUi();
}

AndroidDashboardPanel::~AndroidDashboardPanel()
{
    shutting_down_ = true;
    stopSpinning();

    imu_sub_.reset();
    gps_sub_.reset();
    battery_sub_.reset();
    camera_sub_.reset();
    joy_sub_.reset();
    mag_sub_.reset();
    baro_sub_.reset();
    front_camera_sub_.reset();
    thermal_sub_.reset();
    ir_sub_.reset();

    if (node_) {
        try {
            executor_.remove_node(node_);
        } catch (...) {
        }
        node_.reset();
    }
}

// ---------------------------------------------------------------------------
// setupUi
// ---------------------------------------------------------------------------

void AndroidDashboardPanel::setupUi()
{
    // Root layout
    auto *root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(0);

    // Scroll area
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(
        QString("QScrollArea { background: %1; border: none; }").arg(kPanelBg));

    auto *content = new QWidget();
    content->setStyleSheet(QString("background: %1;").arg(kPanelBg));
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // ---- Section 1: Topic Prefix ----
    {
        auto *section = new QWidget();
        section->setStyleSheet(sectionStyle());
        auto *row = new QHBoxLayout(section);
        row->setContentsMargins(6, 4, 6, 4);

        auto *label = new QLabel("Topic prefix:");
        label->setStyleSheet(
            QString("color: %1; font-size: 11px; border: none;").arg(kSubtitleColor));

        prefix_input_ = new QLineEdit(prefix_);
        prefix_input_->setStyleSheet(
            QString("color: %1; background: %2; border: 1px solid %3; "
                    "border-radius: 4px; padding: 2px 4px; font-size: 11px; "
                    "selection-background-color: %4;")
                .arg(kTextColor).arg(kPanelBg).arg(kSectionBorder).arg(kAccentGold));

        row->addWidget(label);
        row->addWidget(prefix_input_, 1);
        layout->addWidget(section);

        connect(prefix_input_, &QLineEdit::editingFinished,
                this, &AndroidDashboardPanel::onPrefixChanged);
    }

    // ---- Section 2: Health Strip (2-row grid: 5 top, 5 bottom) ----
    {
        auto *section = new QWidget();
        section->setStyleSheet(sectionStyle(true));
        auto *grid = new QGridLayout(section);
        grid->setContentsMargins(4, 4, 4, 4);
        grid->setHorizontalSpacing(4);
        grid->setVerticalSpacing(4);

        constexpr int kCols = 5;
        for (int i = 0; i < kNumSensors; ++i) {
            const int row = i / kCols;
            const int col_idx = i % kCols;

            // Chip container with colored border
            health_chip_[i] = new QWidget();
            health_chip_[i]->setStyleSheet(chipStyle(kDotGray));

            auto *cell = new QHBoxLayout(health_chip_[i]);
            cell->setAlignment(Qt::AlignVCenter);
            cell->setSpacing(4);
            cell->setContentsMargins(4, 3, 4, 3);

            // Circular status dot (10px)
            health_dot_[i] = new QLabel();
            health_dot_[i]->setStyleSheet(dotStyle(kDotGray));
            health_dot_[i]->setFixedSize(10, 10);
            cell->addWidget(health_dot_[i], 0, Qt::AlignVCenter);

            // Right side: name + hz stacked
            auto *text_col = new QVBoxLayout();
            text_col->setSpacing(0);
            text_col->setContentsMargins(0, 0, 0, 0);

            auto *name_label = new QLabel(kSensorNames[i]);
            name_label->setAlignment(Qt::AlignLeft);
            name_label->setStyleSheet(
                QString("color: %1; font-size: 8px; font-weight: 700; border: none; padding: 0px; margin: 0px;")
                    .arg(kTextColor));
            text_col->addWidget(name_label);

            health_hz_[i] = new QLabel("--");
            health_hz_[i]->setAlignment(Qt::AlignLeft);
            health_hz_[i]->setStyleSheet(
                QString("color: %1; font-size: 8px; border: none; padding: 0px; margin: 0px;")
                    .arg(kSubtitleColor));
            text_col->addWidget(health_hz_[i]);

            cell->addLayout(text_col);

            grid->addWidget(health_chip_[i], row, col_idx);
        }

        layout->addWidget(section);
    }

    // ---- Section 3: IMU Compass + GNSS (two-column) ----
    {
        auto *section = new QWidget();
        section->setStyleSheet(sectionStyle());
        auto *row = new QHBoxLayout(section);
        row->setContentsMargins(6, 6, 6, 6);
        row->setSpacing(8);

        // Left: Compass
        {
            auto *left = new QVBoxLayout();
            left->setSpacing(4);

            left->addWidget(makeSectionTitle("IMU COMPASS"));

            compass_widget_ = new CompassWidget();
            compass_widget_->setFixedSize(150, 150);
            left->addWidget(compass_widget_, 0, Qt::AlignHCenter);
            left->addStretch();

            row->addLayout(left, 1);
        }

        // Right: GNSS
        {
            auto *right = new QVBoxLayout();
            right->setSpacing(4);

            right->addWidget(makeSectionTitle("GNSS"));

            gnss_position_widget_ = new GnssPositionWidget();
            gnss_position_widget_->setFixedSize(200, 200);
            right->addWidget(gnss_position_widget_, 0, Qt::AlignHCenter);

            gnss_fix_label_ = makeDataLabel("Waiting for GPS...");
            right->addWidget(gnss_fix_label_);
            right->addStretch();

            row->addLayout(right, 1);
        }

        layout->addWidget(section);
    }

    // ---- Section 4: IMU Motion ----
    {
        auto *section = new QWidget();
        section->setStyleSheet(sectionStyle());
        auto *sec_layout = new QVBoxLayout(section);
        sec_layout->setContentsMargins(6, 6, 6, 6);
        sec_layout->setSpacing(4);

        sec_layout->addWidget(makeSectionTitle("IMU MOTION"));

        imu_motion_widget_ = new ImuMotionWidget();
        imu_motion_widget_->setFixedHeight(220);
        sec_layout->addWidget(imu_motion_widget_);

        layout->addWidget(section);
    }

    // ---- Section 5: Battery + Barometer (two-column) ----
    {
        auto *section = new QWidget();
        section->setStyleSheet(sectionStyle());
        auto *row = new QHBoxLayout(section);
        row->setContentsMargins(6, 6, 6, 6);
        row->setSpacing(8);

        // Left: Battery
        {
            auto *left = new QVBoxLayout();
            left->setSpacing(4);

            left->addWidget(makeSectionTitle("BATTERY"));

            battery_bar_ = new QProgressBar();
            battery_bar_->setRange(0, 100);
            battery_bar_->setValue(0);
            battery_bar_->setTextVisible(false);
            battery_bar_->setFixedHeight(16);
            battery_bar_->setStyleSheet(
                "QProgressBar { background: #2d2d2d; border: 1px solid #4a4a4a; "
                "border-radius: 4px; } "
                "QProgressBar::chunk { background: #22c55e; border-radius: 3px; }");
            left->addWidget(battery_bar_);

            battery_label_ = makeDataLabel("Waiting for battery...");
            left->addWidget(battery_label_);

            row->addLayout(left, 1);
        }

        // Right: Barometer
        {
            auto *right = new QVBoxLayout();
            right->setSpacing(4);

            right->addWidget(makeSectionTitle("BAROMETER"));

            barometer_label_ = makeDataLabel("Waiting for barometer...");
            right->addWidget(barometer_label_);
            right->addStretch();

            row->addLayout(right, 1);
        }

        layout->addWidget(section);
    }

    // ---- Section 6: Magnetometer ----
    {
        auto *section = new QWidget();
        section->setStyleSheet(sectionStyle());
        auto *sec_layout = new QVBoxLayout(section);
        sec_layout->setContentsMargins(6, 6, 6, 6);
        sec_layout->setSpacing(4);

        sec_layout->addWidget(makeSectionTitle("MAGNETOMETER"));

        magnetometer_widget_ = new MagnetometerWidget();
        magnetometer_widget_->setFixedHeight(280);
        sec_layout->addWidget(magnetometer_widget_);

        layout->addWidget(section);
    }

    // ---- Section 7: Thermal ----
    {
        auto *section = new QWidget();
        section->setStyleSheet(sectionStyle());
        auto *sec_layout = new QVBoxLayout(section);
        sec_layout->setContentsMargins(6, 6, 6, 6);
        sec_layout->setSpacing(4);

        sec_layout->addWidget(makeSectionTitle("THERMAL"));

        thermal_label_ = makeDataLabel("Waiting for thermal...");
        sec_layout->addWidget(thermal_label_);

        layout->addWidget(section);
    }

    // ---- Section 8: Infrared ----
    {
        auto *section = new QWidget();
        section->setStyleSheet(sectionStyle());
        auto *sec_layout = new QVBoxLayout(section);
        sec_layout->setContentsMargins(6, 6, 6, 6);
        sec_layout->setSpacing(4);

        sec_layout->addWidget(makeSectionTitle("INFRARED"));

        ir_label_ = makeDataLabel("Waiting for infrared...");
        sec_layout->addWidget(ir_label_);

        layout->addWidget(section);
    }

    // ---- Section 9: Joystick ----
    {
        auto *section = new QWidget();
        section->setStyleSheet(sectionStyle());
        auto *sec_layout = new QVBoxLayout(section);
        sec_layout->setContentsMargins(6, 6, 6, 6);
        sec_layout->setSpacing(4);

        sec_layout->addWidget(makeSectionTitle("JOYSTICK"));

        joystick_widget_ = new JoystickWidget();
        joystick_widget_->setFixedSize(160, 180);
        sec_layout->addWidget(joystick_widget_, 0, Qt::AlignHCenter);

        layout->addWidget(section);
    }

    // ---- Section 10: Sensor Pulse ----
    {
        auto *section = new QWidget();
        section->setStyleSheet(sectionStyle());
        auto *sec_layout = new QVBoxLayout(section);
        sec_layout->setContentsMargins(6, 6, 6, 6);
        sec_layout->setSpacing(4);

        sec_layout->addWidget(makeSectionTitle("SENSOR PULSE"));

        pulse_widget_ = new SensorPulseWidget();
        pulse_widget_->setFixedHeight(100);
        sec_layout->addWidget(pulse_widget_);

        layout->addWidget(section);
    }

    // ---- Section 11: Telemetry Table ----
    {
        auto *section = new QWidget();
        section->setStyleSheet(sectionStyle());
        auto *sec_layout = new QVBoxLayout(section);
        sec_layout->setContentsMargins(6, 6, 6, 6);
        sec_layout->setSpacing(4);

        sec_layout->addWidget(makeSectionTitle("TELEMETRY"));

        telemetry_label_ = new QLabel("Waiting for data...");
        telemetry_label_->setWordWrap(true);
        telemetry_label_->setTextFormat(Qt::PlainText);
        telemetry_label_->setStyleSheet(
            QString("color: %1; font-family: monospace; font-size: 10px; border: none; padding: 2px;")
                .arg(kTextColor));
        sec_layout->addWidget(telemetry_label_);

        layout->addWidget(section);
    }

    layout->addStretch();

    content->setLayout(layout);
    scroll->setWidget(content);
    root_layout->addWidget(scroll);
    setLayout(root_layout);
}

// ---------------------------------------------------------------------------
// onInitialize
// ---------------------------------------------------------------------------

void AndroidDashboardPanel::onInitialize()
{
    if (shutting_down_ || initialized_) {
        return;
    }

    if (!rclcpp::ok()) {
        return;
    }

    initializeNode();
    if (!node_) {
        return;
    }

    subscribeAll();
    startSpinning();
    initialized_ = true;
}

// ---------------------------------------------------------------------------
// load / save
// ---------------------------------------------------------------------------

void AndroidDashboardPanel::load(const rviz_common::Config &config)
{
    rviz_common::Panel::load(config);
    QString loaded_prefix;
    if (config.mapGetString("TopicPrefix", &loaded_prefix)) {
        prefix_ = loaded_prefix;
        if (prefix_input_) {
            prefix_input_->setText(prefix_);
        }
        if (node_) {
            resubscribeAll();
        }
    }
}

void AndroidDashboardPanel::save(rviz_common::Config config) const
{
    rviz_common::Panel::save(config);
    config.mapSetValue("TopicPrefix", prefix_);
}

// ---------------------------------------------------------------------------
// Node and subscription management
// ---------------------------------------------------------------------------

void AndroidDashboardPanel::initializeNode()
{
    if (node_) {
        return;
    }
    try {
        node_ = std::make_shared<rclcpp::Node>("android_dashboard_panel");
        executor_.add_node(node_);
    } catch (...) {
    }
}

void AndroidDashboardPanel::subscribeAll()
{
    if (!node_) {
        return;
    }

    const auto qos = rclcpp::QoS(10).best_effort();
    const std::string pfx = prefix_.toStdString();

    try {
        imu_sub_ = node_->create_subscription<sensor_msgs::msg::Imu>(
            pfx + "/imu", qos,
            std::bind(&AndroidDashboardPanel::onImu, this, std::placeholders::_1));
    } catch (...) {
        imu_sub_.reset();
    }

    try {
        gps_sub_ = node_->create_subscription<sensor_msgs::msg::NavSatFix>(
            pfx + "/gps", qos,
            std::bind(&AndroidDashboardPanel::onGps, this, std::placeholders::_1));
    } catch (...) {
        gps_sub_.reset();
    }

    try {
        battery_sub_ = node_->create_subscription<sensor_msgs::msg::BatteryState>(
            pfx + "/battery", qos,
            std::bind(&AndroidDashboardPanel::onBattery, this, std::placeholders::_1));
    } catch (...) {
        battery_sub_.reset();
    }

    try {
        camera_sub_ = node_->create_subscription<sensor_msgs::msg::CompressedImage>(
            pfx + "/camera/image_raw/compressed", qos,
            std::bind(&AndroidDashboardPanel::onCamera, this, std::placeholders::_1));
    } catch (...) {
        camera_sub_.reset();
    }

    try {
        joy_sub_ = node_->create_subscription<sensor_msgs::msg::Joy>(
            pfx + "/joy", qos,
            std::bind(&AndroidDashboardPanel::onJoy, this, std::placeholders::_1));
    } catch (...) {
        joy_sub_.reset();
    }

    try {
        mag_sub_ = node_->create_subscription<sensor_msgs::msg::MagneticField>(
            pfx + "/magnetometer", qos,
            std::bind(&AndroidDashboardPanel::onMagnetometer, this, std::placeholders::_1));
    } catch (...) {
        mag_sub_.reset();
    }

    try {
        baro_sub_ = node_->create_subscription<sensor_msgs::msg::FluidPressure>(
            pfx + "/barometer", qos,
            std::bind(&AndroidDashboardPanel::onBarometer, this, std::placeholders::_1));
    } catch (...) {
        baro_sub_.reset();
    }

    try {
        front_camera_sub_ = node_->create_subscription<sensor_msgs::msg::CompressedImage>(
            pfx + "/front_camera/image_raw/compressed", qos,
            std::bind(&AndroidDashboardPanel::onFrontCamera, this, std::placeholders::_1));
    } catch (...) {
        front_camera_sub_.reset();
    }

    try {
        thermal_sub_ = node_->create_subscription<sensor_msgs::msg::Temperature>(
            pfx + "/thermal", qos,
            std::bind(&AndroidDashboardPanel::onThermal, this, std::placeholders::_1));
    } catch (...) {
        thermal_sub_.reset();
    }

    try {
        ir_sub_ = node_->create_subscription<sensor_msgs::msg::Range>(
            pfx + "/infrared", qos,
            std::bind(&AndroidDashboardPanel::onInfrared, this, std::placeholders::_1));
    } catch (...) {
        ir_sub_.reset();
    }
}

void AndroidDashboardPanel::resubscribeAll()
{
    // Tear down existing subscriptions
    imu_sub_.reset();
    gps_sub_.reset();
    battery_sub_.reset();
    camera_sub_.reset();
    joy_sub_.reset();
    mag_sub_.reset();
    baro_sub_.reset();
    front_camera_sub_.reset();
    thermal_sub_.reset();
    ir_sub_.reset();

    // Reset sensor states
    imu_state_ = {};
    gps_state_ = {};
    battery_state_ = {};
    camera_state_ = {};
    joy_state_ = {};
    mag_state_ = {};
    baro_state_ = {};
    front_camera_state_ = {};
    thermal_state_ = {};
    ir_state_ = {};

    // Reset UI
    if (compass_widget_) {
        compass_widget_->setActive(false);
    }
    if (imu_motion_widget_) {
        imu_motion_widget_->setActive(false);
    }
    if (gnss_position_widget_) {
        gnss_position_widget_->clearTrail();
    }
    if (gnss_fix_label_) {
        gnss_fix_label_->setText("Waiting for GPS...");
    }
    if (battery_bar_) {
        battery_bar_->setValue(0);
    }
    if (battery_label_) {
        battery_label_->setText("Waiting for battery...");
    }
    if (barometer_label_) {
        barometer_label_->setText("Waiting for barometer...");
    }
    if (magnetometer_widget_) {
        magnetometer_widget_->setActive(false);
    }
    if (joystick_widget_) {
        joystick_widget_->setActive(false);
    }
    if (thermal_label_) {
        thermal_label_->setText("Waiting for thermal...");
    }
    if (ir_label_) {
        ir_label_->setText("Waiting for infrared...");
    }
    if (pulse_widget_) {
        pulse_widget_->setActive(false);
    }
    if (telemetry_label_) {
        telemetry_label_->setText("Waiting for data...");
    }

    subscribeAll();
}

void AndroidDashboardPanel::onPrefixChanged()
{
    if (shutting_down_ || !prefix_input_) {
        return;
    }

    QString new_prefix = prefix_input_->text().trimmed();
    if (new_prefix == prefix_) {
        return;
    }

    prefix_ = new_prefix;
    if (node_) {
        resubscribeAll();
    }
}

// ---------------------------------------------------------------------------
// Message callbacks
// ---------------------------------------------------------------------------

void AndroidDashboardPanel::onImu(const sensor_msgs::msg::Imu::SharedPtr msg)
{
    if (shutting_down_ || !msg) {
        return;
    }

    imu_state_.recordMessage();

    // Update compass
    if (compass_widget_) {
        compass_widget_->setOrientation(
            msg->orientation.x, msg->orientation.y,
            msg->orientation.z, msg->orientation.w);
        compass_widget_->setActive(true);
    }

    // Update IMU motion widget
    if (imu_motion_widget_) {
        imu_motion_widget_->setData(
            msg->linear_acceleration.x,
            msg->linear_acceleration.y,
            msg->linear_acceleration.z,
            msg->angular_velocity.x,
            msg->angular_velocity.y,
            msg->angular_velocity.z);
        imu_motion_widget_->setActive(true);
    }
}

void AndroidDashboardPanel::onGps(const sensor_msgs::msg::NavSatFix::SharedPtr msg)
{
    if (shutting_down_ || !msg) {
        return;
    }

    gps_state_.recordMessage();

    double horizontal_sigma = -1.0;
    if (msg->position_covariance[0] >= 0.0 && msg->position_covariance[4] >= 0.0) {
        horizontal_sigma = std::sqrt(
            (msg->position_covariance[0] + msg->position_covariance[4]) / 2.0);
    }

    if (gnss_position_widget_) {
        gnss_position_widget_->setPosition(
            msg->latitude, msg->longitude, msg->altitude, horizontal_sigma);
        gnss_position_widget_->setActive(true);
    }

    if (gnss_fix_label_) {
        gnss_fix_label_->setText(
            QString("Fix: %1 (%2)")
                .arg(gnssFixText(msg->status.status))
                .arg(gnssServiceText(msg->status.service)));
    }
}

void AndroidDashboardPanel::onBattery(const sensor_msgs::msg::BatteryState::SharedPtr msg)
{
    if (shutting_down_ || !msg) {
        return;
    }

    battery_state_.recordMessage();

    const int pct = static_cast<int>(std::clamp(msg->percentage * 100.0f, 0.0f, 100.0f));

    if (battery_bar_) {
        battery_bar_->setValue(pct);

        // Color based on level
        QString chunk_color = kDotGreen;
        if (pct <= 15) {
            chunk_color = kDotRed;
        } else if (pct <= 30) {
            chunk_color = kDotAmber;
        }

        battery_bar_->setStyleSheet(
            QString(
                "QProgressBar { background: #2d2d2d; border: 1px solid #4a4a4a; "
                "border-radius: 4px; } "
                "QProgressBar::chunk { background: %1; border-radius: 3px; }")
                .arg(chunk_color));
    }

    if (battery_label_) {
        battery_label_->setText(
            QString("%1%  |  %2 V  |  %3 A")
                .arg(pct)
                .arg(static_cast<double>(msg->voltage), 0, 'f', 2)
                .arg(static_cast<double>(msg->current), 0, 'f', 2));
    }
}

void AndroidDashboardPanel::onCamera(const sensor_msgs::msg::CompressedImage::SharedPtr msg)
{
    if (shutting_down_ || !msg) {
        return;
    }
    camera_state_.recordMessage();
}

void AndroidDashboardPanel::onJoy(const sensor_msgs::msg::Joy::SharedPtr msg)
{
    if (shutting_down_ || !msg) {
        return;
    }

    joy_state_.recordMessage();

    if (joystick_widget_) {
        float ax = (msg->axes.size() > 0) ? msg->axes[0] : 0.0f;
        float ay = (msg->axes.size() > 1) ? msg->axes[1] : 0.0f;
        joystick_widget_->setAxes(ax, ay);
        joystick_widget_->setButtons(
            std::vector<int>(msg->buttons.begin(), msg->buttons.end()));
        joystick_widget_->setActive(true);
    }
}

void AndroidDashboardPanel::onMagnetometer(const sensor_msgs::msg::MagneticField::SharedPtr msg)
{
    if (shutting_down_ || !msg) {
        return;
    }

    mag_state_.recordMessage();

    if (magnetometer_widget_) {
        // Convert from Tesla to microtesla
        const double x_ut = msg->magnetic_field.x * 1e6;
        const double y_ut = msg->magnetic_field.y * 1e6;
        const double z_ut = msg->magnetic_field.z * 1e6;
        magnetometer_widget_->setField(x_ut, y_ut, z_ut);
        magnetometer_widget_->setActive(true);
    }
}

void AndroidDashboardPanel::onBarometer(const sensor_msgs::msg::FluidPressure::SharedPtr msg)
{
    if (shutting_down_ || !msg) {
        return;
    }

    baro_state_.recordMessage();

    if (barometer_label_) {
        // Convert Pa to hPa
        const double hpa = msg->fluid_pressure / 100.0;
        barometer_label_->setText(
            QString("Pressure: %1 hPa")
                .arg(hpa, 0, 'f', 2));
    }
}

void AndroidDashboardPanel::onFrontCamera(const sensor_msgs::msg::CompressedImage::SharedPtr msg)
{
    if (shutting_down_ || !msg) {
        return;
    }
    front_camera_state_.recordMessage();
}

void AndroidDashboardPanel::onThermal(const sensor_msgs::msg::Temperature::SharedPtr msg)
{
    if (shutting_down_ || !msg) {
        return;
    }

    thermal_state_.recordMessage();

    if (thermal_label_) {
        thermal_label_->setText(
            QString("Temperature: %1 °C")
                .arg(static_cast<double>(msg->temperature), 0, 'f', 1));
    }
}

void AndroidDashboardPanel::onInfrared(const sensor_msgs::msg::Range::SharedPtr msg)
{
    if (shutting_down_ || !msg) {
        return;
    }

    ir_state_.recordMessage();

    if (ir_label_) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.1f cm", msg->range * 100.0);
        ir_label_->setText(buf);
    }
}

// ---------------------------------------------------------------------------
// Health strip update
// ---------------------------------------------------------------------------

void AndroidDashboardPanel::updateHealthStrip()
{
    if (shutting_down_) {
        return;
    }

    SensorState *states[] = {
        &imu_state_, &gps_state_, &camera_state_, &battery_state_,
        &mag_state_, &baro_state_, &joy_state_, &front_camera_state_, &thermal_state_,
        &ir_state_
    };

    for (int i = 0; i < kNumSensors; ++i) {
        const int64_t age = states[i]->ageMs();
        QString color;
        QString hz_text;

        if (age < 0) {
            // No data received yet
            color = kDotGray;
            hz_text = "--";
        } else if (age < 2000) {
            color = kDotGreen;
            hz_text = QString("%1 Hz").arg(states[i]->rate_hz, 0, 'f', 1);
        } else if (age < 5000) {
            color = kDotAmber;
            hz_text = QString("%1 Hz  %2s ago").arg(states[i]->rate_hz, 0, 'f', 1)
                          .arg(age / 1000.0, 0, 'f', 1);
        } else {
            color = kDotRed;
            states[i]->resetRate();
            hz_text = QString("stale %1s").arg(age / 1000.0, 0, 'f', 0);
        }

        if (health_dot_[i]) {
            health_dot_[i]->setStyleSheet(dotStyle(color));
        }
        if (health_hz_[i]) {
            health_hz_[i]->setText(hz_text);
        }
        if (health_chip_[i]) {
            health_chip_[i]->setStyleSheet(chipStyle(color));
        }
    }

    // Deactivate widgets when stale (age > 5000ms)
    if (compass_widget_ && imu_state_.ageMs() > 5000) {
        compass_widget_->setActive(false);
    }
    if (imu_motion_widget_ && imu_state_.ageMs() > 5000) {
        imu_motion_widget_->setActive(false);
    }
    if (gnss_position_widget_ && gps_state_.ageMs() > 5000) {
        gnss_position_widget_->setActive(false);
    }
    if (magnetometer_widget_ && mag_state_.ageMs() > 5000) {
        magnetometer_widget_->setActive(false);
    }
    if (joystick_widget_ && joy_state_.ageMs() > 5000) {
        joystick_widget_->setActive(false);
    }

    // Feed sensor pulse widget (index mapping: 0=IMU,1=GPS,2=BAT,3=RCAM,4=FCAM,5=MAG,6=BAR,7=THM,8=JOY,9=IR)
    if (pulse_widget_) {
        bool any_active = false;
        // Map health strip order to pulse widget order
        // Health: IMU(0), GPS(1), CAM(2), BAT(3), MAG(4), BARO(5), JOY(6), FCAM(7), THM(8), IR(9)
        // Pulse:  IMU(0), GPS(1), BAT(2), RCAM(3), FCAM(4), MAG(5), BAR(6), THM(7), JOY(8), IR(9)
        struct Mapping { int health_idx; int pulse_idx; };
        const Mapping map[] = {
            {0, 0}, {1, 1}, {3, 2}, {2, 3}, {7, 4}, {4, 5}, {5, 6}, {8, 7}, {6, 8}, {9, 9}
        };
        for (const auto &m : map) {
            double hz = states[m.health_idx]->rate_hz;
            bool alive = states[m.health_idx]->ageMs() >= 0 && states[m.health_idx]->ageMs() < 5000;
            pulse_widget_->setLineEnabled(m.pulse_idx, alive);
            if (alive && hz > 0) {
                any_active = true;
            }
        }
        // Push IMU sample first (triggers write_pos_ advance)
        pulse_widget_->addSample(0, imu_state_.ageMs() < 5000 ? imu_state_.rate_hz : 0.0);
        pulse_widget_->addSample(1, gps_state_.ageMs() < 5000 ? gps_state_.rate_hz : 0.0);
        pulse_widget_->addSample(2, battery_state_.ageMs() < 5000 ? battery_state_.rate_hz : 0.0);
        pulse_widget_->addSample(3, camera_state_.ageMs() < 5000 ? camera_state_.rate_hz : 0.0);
        pulse_widget_->addSample(4, front_camera_state_.ageMs() < 5000 ? front_camera_state_.rate_hz : 0.0);
        pulse_widget_->addSample(5, mag_state_.ageMs() < 5000 ? mag_state_.rate_hz : 0.0);
        pulse_widget_->addSample(6, baro_state_.ageMs() < 5000 ? baro_state_.rate_hz : 0.0);
        pulse_widget_->addSample(7, thermal_state_.ageMs() < 5000 ? thermal_state_.rate_hz : 0.0);
        pulse_widget_->addSample(8, joy_state_.ageMs() < 5000 ? joy_state_.rate_hz : 0.0);
        pulse_widget_->addSample(9, ir_state_.ageMs() < 5000 ? ir_state_.rate_hz : 0.0);
        pulse_widget_->setActive(any_active);
    }

    // Update telemetry table
    if (telemetry_label_) {
        char buf[256];
        QString table;

        std::snprintf(buf, sizeof(buf), "%-6s %8s %8s  %s\n",
                      "Sensor", "Count", "Hz", "Status");
        table += buf;
        table += "------  --------  --------  ------\n";

        for (int i = 0; i < kNumSensors; ++i) {
            const int64_t age = states[i]->ageMs();
            const char *status;
            if (age < 0) {
                status = "idle";
            } else if (age < 2000) {
                status = "fresh";
            } else if (age < 5000) {
                status = "aging";
            } else {
                status = "stale";
            }

            char hz_buf[16];
            if (age < 0) {
                std::snprintf(hz_buf, sizeof(hz_buf), "--");
            } else {
                std::snprintf(hz_buf, sizeof(hz_buf), "%.1f", states[i]->rate_hz);
            }

            std::snprintf(buf, sizeof(buf), "%-6s %8d %8s  %s\n",
                          kSensorNames[i],
                          static_cast<int>(states[i]->message_count),
                          hz_buf, status);
            table += buf;
        }
        telemetry_label_->setText(table);
    }
}

// ---------------------------------------------------------------------------
// Spin timer
// ---------------------------------------------------------------------------

void AndroidDashboardPanel::startSpinning()
{
    if (shutting_down_ || !spin_timer_ || !node_) {
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
            updateHealthStrip();
        } catch (...) {
        }
    });
    spin_timer_->start();
}

void AndroidDashboardPanel::stopSpinning()
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

PLUGINLIB_EXPORT_CLASS(android_sensor_viz::AndroidDashboardPanel, rviz_common::Panel)
