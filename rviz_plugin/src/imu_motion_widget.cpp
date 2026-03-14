#include "android_sensor_viz/imu_motion_widget.hpp"

#include <algorithm>
#include <cmath>

#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QFont>

namespace android_sensor_viz
{

namespace
{
const QColor kColorRoll(0xCC, 0x66, 0x33);
const QColor kColorPitch(0x2E, 0x8B, 0x57);
const QColor kColorYaw(0xD4, 0xAF, 0x37);
const QColor kColorAccelX(0xCC, 0x66, 0x33);
const QColor kColorAccelY(0x2E, 0x8B, 0x57);
const QColor kColorAccelZ(0xD4, 0xAF, 0x37);
const QColor kBg(0x2D, 0x2D, 0x2D);
const QColor kTrack(0x3A, 0x3A, 0x3A);
const QColor kText(0xE5, 0xE7, 0xEB);
const QColor kSubtext(0x9C, 0xA3, 0xAF);
const QColor kInactive(0x55, 0x55, 0x70);

constexpr double kMaxGyro = 2.0;   // rad/s
constexpr double kMaxAccel = 20.0; // m/s²
constexpr double kSweepMax = 270.0; // degrees
}  // namespace

ImuMotionWidget::ImuMotionWidget(QWidget *parent)
: QWidget(parent)
{
    setMinimumSize(minimumSizeHint());
}

void ImuMotionWidget::setData(double ax, double ay, double az,
                               double gx, double gy, double gz)
{
    accel_x_ = ax; accel_y_ = ay; accel_z_ = az;
    gyro_x_ = gx; gyro_y_ = gy; gyro_z_ = gz;
    update();
}

void ImuMotionWidget::setActive(bool active)
{
    active_ = active;
    update();
}

QSize ImuMotionWidget::minimumSizeHint() const { return QSize(180, 200); }
QSize ImuMotionWidget::sizeHint() const { return QSize(220, 220); }

void ImuMotionWidget::drawRingGauge(QPainter &p, double cx, double cy,
                                     double radius, double value, double max_val,
                                     const QColor &color, const QString &label,
                                     const QString &unit_text) const
{
    const QColor active_color = active_ ? color : kInactive;

    // Track circle
    p.setPen(QPen(kTrack, 3.0));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(cx, cy), radius, radius);

    // Active arc — sweep proportional to |value|
    const double ratio = std::clamp(std::abs(value) / max_val, 0.0, 1.0);
    const double sweep = ratio * kSweepMax;
    if (sweep > 0.5) {
        const int start_angle = 90 * 16; // 12 o'clock in Qt arc coords
        const int span = static_cast<int>(
            (value >= 0 ? -sweep : sweep) * 16.0);
        QRectF arc_rect(cx - radius, cy - radius, radius * 2, radius * 2);
        p.setPen(QPen(active_color, 4.0, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(arc_rect, start_angle, span);
    }

    // Tick marks (every 30°, 12 ticks)
    p.setPen(QPen(kTrack, 1.0));
    for (int i = 0; i < 12; ++i) {
        const double a = i * 30.0 * M_PI / 180.0;
        const double inner = (i % 3 == 0) ? radius * 0.7 : radius * 0.8;
        p.drawLine(
            QPointF(cx + std::sin(a) * inner, cy - std::cos(a) * inner),
            QPointF(cx + std::sin(a) * radius * 0.92, cy - std::cos(a) * radius * 0.92));
    }

    // Center dot
    p.setPen(Qt::NoPen);
    p.setBrush(active_color);
    p.drawEllipse(QPointF(cx, cy), 2.5, 2.5);

    // Label below ring
    QFont f = p.font();
    f.setPixelSize(9);
    f.setBold(true);
    p.setFont(f);
    p.setPen(active_color);
    p.drawText(QRectF(cx - radius, cy + radius + 2, radius * 2, 14),
               Qt::AlignCenter, label);

    // Value text
    f.setPixelSize(8);
    f.setBold(false);
    p.setFont(f);
    p.setPen(kSubtext);
    p.drawText(QRectF(cx - radius, cy + radius + 14, radius * 2, 12),
               Qt::AlignCenter, unit_text);
}

void ImuMotionWidget::drawAccelBar(QPainter &p, double x, double y,
                                    double w, double h, double value,
                                    double max_val, const QColor &color,
                                    const QString &label) const
{
    const QColor active_color = active_ ? color : kInactive;
    const double center_x = x + w / 2.0;

    // Track background
    p.setPen(Qt::NoPen);
    p.setBrush(kTrack);
    p.drawRoundedRect(QRectF(x, y, w, h), h / 2, h / 2);

    // Fill from center
    const double ratio = std::clamp(value / max_val, -1.0, 1.0);
    const double fill_w = std::abs(ratio) * (w / 2.0);
    if (fill_w > 0.5) {
        const double fill_x = (ratio >= 0) ? center_x : center_x - fill_w;
        p.setBrush(active_color);
        p.drawRoundedRect(QRectF(fill_x, y, fill_w, h), h / 2, h / 2);
    }

    // Center zero line
    p.setPen(QPen(kText, 1.0));
    p.drawLine(QPointF(center_x, y - 1), QPointF(center_x, y + h + 1));

    // Label (left)
    QFont f = p.font();
    f.setPixelSize(9);
    f.setBold(true);
    p.setFont(f);
    p.setPen(active_color);
    p.drawText(QRectF(x - 18, y, 16, h), Qt::AlignVCenter | Qt::AlignRight, label);

    // Value (right)
    f.setBold(false);
    f.setPixelSize(8);
    p.setFont(f);
    p.setPen(kSubtext);
    p.drawText(QRectF(x + w + 4, y, 50, h), Qt::AlignVCenter | Qt::AlignLeft,
               QString::number(value, 'f', 1));
}

void ImuMotionWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();

    // Background
    p.setPen(Qt::NoPen);
    p.setBrush(kBg);
    p.drawRoundedRect(rect(), 6, 6);

    // Header: ANGULAR VELOCITY
    QFont hf = p.font();
    hf.setPixelSize(10);
    hf.setBold(true);
    p.setFont(hf);
    p.setPen(kSubtext);
    p.drawText(QRectF(8, 4, w - 16, 14), Qt::AlignLeft | Qt::AlignVCenter,
               "ANGULAR VELOCITY");

    // Three ring gauges
    const double ring_r = std::min(w / 7.0, 27.0);
    const double ring_y = 18 + ring_r + 4;
    const double spacing = w / 3.0;

    const double gx_dps = gyro_x_ * 57.29577951308232;
    const double gy_dps = gyro_y_ * 57.29577951308232;
    const double gz_dps = gyro_z_ * 57.29577951308232;

    drawRingGauge(p, spacing * 0.5, ring_y, ring_r,
                  gyro_x_, kMaxGyro, kColorRoll, "Roll",
                  QString("%1°/s").arg(gx_dps, 0, 'f', 1));
    drawRingGauge(p, spacing * 1.5, ring_y, ring_r,
                  gyro_y_, kMaxGyro, kColorPitch, "Pitch",
                  QString("%1°/s").arg(gy_dps, 0, 'f', 1));
    drawRingGauge(p, spacing * 2.5, ring_y, ring_r,
                  gyro_z_, kMaxGyro, kColorYaw, "Yaw",
                  QString("%1°/s").arg(gz_dps, 0, 'f', 1));

    // Sub-header: ACCELERATION
    const double accel_top = ring_y + ring_r + 32;
    p.setFont(hf);
    p.setPen(kSubtext);
    p.drawText(QRectF(8, accel_top, w - 16, 14), Qt::AlignLeft | Qt::AlignVCenter,
               "ACCELERATION");

    // Three accel bars
    const double bar_left = 24;
    const double bar_w = w - 80;
    const double bar_h = 8;
    const double bar_spacing = 20;
    double by = accel_top + 18;

    drawAccelBar(p, bar_left, by, bar_w, bar_h, accel_x_, kMaxAccel, kColorAccelX, "X");
    by += bar_spacing;
    drawAccelBar(p, bar_left, by, bar_w, bar_h, accel_y_, kMaxAccel, kColorAccelY, "Y");
    by += bar_spacing;
    drawAccelBar(p, bar_left, by, bar_w, bar_h, accel_z_, kMaxAccel, kColorAccelZ, "Z");

    // Footer: magnitude + yaw
    by += bar_spacing + 2;
    const double mag = std::sqrt(accel_x_ * accel_x_ + accel_y_ * accel_y_ + accel_z_ * accel_z_);
    QFont ff = p.font();
    ff.setPixelSize(9);
    ff.setBold(false);
    p.setFont(ff);
    p.setPen(kText);
    p.drawText(QRectF(8, by, w - 16, 14), Qt::AlignCenter,
               QString::fromUtf8("\u2016a\u2016 %1 m/s\u00B2  Yaw %2%3/s")
                   .arg(mag, 0, 'f', 1)
                   .arg(gz_dps >= 0 ? "+" : "")
                   .arg(gz_dps, 0, 'f', 1));
}

}  // namespace android_sensor_viz
