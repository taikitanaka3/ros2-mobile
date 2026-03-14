#include "android_sensor_viz/magnetometer_widget.hpp"

#include <algorithm>
#include <cmath>

#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QPolygonF>
#include <QPointF>

namespace android_sensor_viz
{

namespace
{
const QColor kBg(0x2D, 0x2D, 0x2D);
const QColor kRoseFill(0x25, 0x25, 0x25);
const QColor kRing(0x4A, 0x4A, 0x4A);
const QColor kGold(0xD4, 0xAF, 0x37);
const QColor kTail(0xAA, 0xAA, 0xBC);
const QColor kBarX(0xCC, 0x66, 0x33);
const QColor kBarY(0x2E, 0x8B, 0x57);
const QColor kBarZ(0xD4, 0xAF, 0x37);
const QColor kTrack(0x3A, 0x3A, 0x3A);
const QColor kText(0xE5, 0xE7, 0xEB);
const QColor kSubtext(0x9C, 0xA3, 0xAF);
const QColor kInactive(0x55, 0x55, 0x70);

constexpr double kMaxFieldUT = 100.0;
}  // namespace

MagnetometerWidget::MagnetometerWidget(QWidget *parent)
: QWidget(parent)
{
    setMinimumSize(minimumSizeHint());
}

void MagnetometerWidget::setField(double x_ut, double y_ut, double z_ut)
{
    field_x_ = x_ut; field_y_ = y_ut; field_z_ = z_ut;
    update();
}

void MagnetometerWidget::setActive(bool active)
{
    active_ = active;
    update();
}

QSize MagnetometerWidget::minimumSizeHint() const { return QSize(180, 240); }
QSize MagnetometerWidget::sizeHint() const { return QSize(220, 280); }

void MagnetometerWidget::drawCompassRose(QPainter &p, double cx, double cy, double radius) const
{
    const QColor needle_color = active_ ? kGold : kInactive;
    const QColor tail_color = active_ ? kTail : kInactive;

    // Rose background
    p.setPen(QPen(kRing, 2.0));
    p.setBrush(kRoseFill);
    p.drawEllipse(QPointF(cx, cy), radius, radius);

    // Tick marks (12, every 30°)
    p.setPen(QPen(kRing, 1.5));
    for (int i = 0; i < 12; ++i) {
        const double a = i * 30.0 * M_PI / 180.0;
        const double inner = (i % 3 == 0) ? radius * 0.72 : radius * 0.82;
        p.drawLine(
            QPointF(cx + std::sin(a) * inner, cy - std::cos(a) * inner),
            QPointF(cx + std::sin(a) * radius * 0.92, cy - std::cos(a) * radius * 0.92));
    }

    // N/E/S/W labels
    QFont lf = p.font();
    lf.setPixelSize(static_cast<int>(radius * 0.22));
    lf.setBold(true);
    p.setFont(lf);

    struct CLabel { const char *text; double angle; QColor color; };
    const CLabel labels[] = {
        {"N",   0.0, active_ ? kGold : kInactive},
        {"E",  90.0, active_ ? kSubtext : kInactive},
        {"S", 180.0, active_ ? kSubtext : kInactive},
        {"W", 270.0, active_ ? kSubtext : kInactive},
    };
    for (const auto &l : labels) {
        const double a = l.angle * M_PI / 180.0;
        const double r = radius * 0.58;
        const double lx = cx + std::sin(a) * r;
        const double ly = cy - std::cos(a) * r;
        const int sz = static_cast<int>(radius * 0.3);
        p.setPen(l.color);
        p.drawText(QRectF(lx - sz / 2.0, ly - sz / 2.0, sz, sz), Qt::AlignCenter, l.text);
    }

    // Magnetic heading: atan2(X, Y)
    const double heading_deg = std::atan2(field_x_, field_y_) * 180.0 / M_PI;

    p.save();
    p.translate(cx, cy);
    p.rotate(heading_deg);

    const double half_w = radius * 0.07;
    const double tip_len = radius * 0.42;
    const double tail_len = tip_len * 0.35;

    // Gold needle tip
    QPolygonF tip;
    tip << QPointF(0, -tip_len) << QPointF(-half_w, 0)
        << QPointF(0, -half_w * 0.5) << QPointF(half_w, 0);
    p.setPen(Qt::NoPen);
    p.setBrush(needle_color);
    p.drawPolygon(tip);

    // Gray tail
    QPolygonF tail;
    tail << QPointF(0, tail_len) << QPointF(-half_w, 0)
         << QPointF(0, half_w * 0.5) << QPointF(half_w, 0);
    p.setBrush(tail_color);
    p.drawPolygon(tail);

    p.restore();

    // Center dot
    p.setPen(Qt::NoPen);
    p.setBrush(kText);
    p.drawEllipse(QPointF(cx, cy), radius * 0.035, radius * 0.035);
}

void MagnetometerWidget::drawFieldBar(QPainter &p, double x, double y,
                                       double w, double h, double value,
                                       double max_val, const QColor &color,
                                       const QString &label) const
{
    const QColor active_color = active_ ? color : kInactive;
    const double center_x = x + w / 2.0;

    // Track
    p.setPen(Qt::NoPen);
    p.setBrush(kTrack);
    p.drawRoundedRect(QRectF(x, y, w, h), h / 2, h / 2);

    // Fill
    const double ratio = std::clamp(value / max_val, -1.0, 1.0);
    const double fill_w = std::abs(ratio) * (w / 2.0);
    if (fill_w > 0.5) {
        const double fill_x = (ratio >= 0) ? center_x : center_x - fill_w;
        p.setBrush(active_color);
        p.drawRoundedRect(QRectF(fill_x, y, fill_w, h), h / 2, h / 2);
    }

    // Center line
    p.setPen(QPen(kText, 1.0));
    p.drawLine(QPointF(center_x, y - 1), QPointF(center_x, y + h + 1));

    // Label
    QFont f = p.font();
    f.setPixelSize(9);
    f.setBold(true);
    p.setFont(f);
    p.setPen(active_color);
    p.drawText(QRectF(x - 18, y, 16, h), Qt::AlignVCenter | Qt::AlignRight, label);

    // Value
    f.setBold(false);
    f.setPixelSize(8);
    p.setFont(f);
    p.setPen(kSubtext);
    p.drawText(QRectF(x + w + 4, y, 50, h), Qt::AlignVCenter | Qt::AlignLeft,
               QString("%1 \u00B5T").arg(value, 0, 'f', 1));
}

void MagnetometerWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();

    // Background
    p.setPen(Qt::NoPen);
    p.setBrush(kBg);
    p.drawRoundedRect(rect(), 6, 6);

    // Compass rose
    const double rose_r = std::min(w * 0.35, 55.0);
    const double rose_cx = w / 2.0;
    const double rose_cy = 8 + rose_r + 4;
    drawCompassRose(p, rose_cx, rose_cy, rose_r);

    // Heading text below compass
    const double heading_deg = std::atan2(field_x_, field_y_) * 180.0 / M_PI;
    const double display_heading = (heading_deg < 0) ? heading_deg + 360.0 : heading_deg;

    QFont hf = p.font();
    hf.setPixelSize(10);
    hf.setBold(true);
    p.setFont(hf);
    p.setPen(active_ ? kText : kInactive);
    const double heading_y = rose_cy + rose_r + 6;
    p.drawText(QRectF(0, heading_y, w, 14), Qt::AlignCenter,
               QString("Heading: %1\u00B0").arg(display_heading, 0, 'f', 1));

    // Field bars
    const double bar_left = 24;
    const double bar_w = w - 84;
    const double bar_h = 8;
    const double bar_spacing = 20;
    double by = heading_y + 22;

    drawFieldBar(p, bar_left, by, bar_w, bar_h, field_x_, kMaxFieldUT, kBarX, "X");
    by += bar_spacing;
    drawFieldBar(p, bar_left, by, bar_w, bar_h, field_y_, kMaxFieldUT, kBarY, "Y");
    by += bar_spacing;
    drawFieldBar(p, bar_left, by, bar_w, bar_h, field_z_, kMaxFieldUT, kBarZ, "Z");

    // Footer: magnitude + dip angle
    by += bar_spacing + 4;
    const double mag = std::sqrt(field_x_ * field_x_ + field_y_ * field_y_ + field_z_ * field_z_);
    const double horiz = std::sqrt(field_x_ * field_x_ + field_y_ * field_y_);
    const double dip_deg = std::atan2(-field_z_, horiz) * 180.0 / M_PI;

    hf.setPixelSize(9);
    hf.setBold(false);
    p.setFont(hf);
    p.setPen(active_ ? kText : kInactive);
    p.drawText(QRectF(4, by, w - 8, 14), Qt::AlignCenter,
               QString("Mag: %1 \u00B5T  Dip: %2\u00B0")
                   .arg(mag, 0, 'f', 1).arg(dip_deg, 0, 'f', 1));
}

}  // namespace android_sensor_viz
