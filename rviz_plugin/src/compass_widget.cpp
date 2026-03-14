#include "android_sensor_viz/compass_widget.hpp"

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

CompassWidget::CompassWidget(QWidget *parent)
: QWidget(parent)
{
  setMinimumSize(minimumSizeHint());
}

void CompassWidget::setOrientation(double qx, double qy, double qz, double qw)
{
  const double x = qx;
  const double y = qy;
  const double z = qz;
  const double w = qw;

  const double sinr_cosp = 2.0 * ((w * x) + (y * z));
  const double cosr_cosp = 1.0 - (2.0 * ((x * x) + (y * y)));
  roll_deg_ = std::atan2(sinr_cosp, cosr_cosp) * 57.29577951308232;

  const double sinp = 2.0 * ((w * y) - (z * x));
  pitch_deg_ = std::asin(std::clamp(sinp, -1.0, 1.0)) * 57.29577951308232;

  const double siny_cosp = 2.0 * ((w * z) + (x * y));
  const double cosy_cosp = 1.0 - (2.0 * ((y * y) + (z * z)));
  yaw_deg_ = std::atan2(siny_cosp, cosy_cosp) * 57.29577951308232;

  update();
}

void CompassWidget::setActive(bool active)
{
  active_ = active;
  update();
}

QSize CompassWidget::minimumSizeHint() const
{
  return QSize(100, 100);
}

QSize CompassWidget::sizeHint() const
{
  return QSize(150, 150);
}

void CompassWidget::paintEvent(QPaintEvent * /*event*/)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  // Determine the square region to paint into
  const int side = std::min(width(), height());
  const double radius = side * 0.45;
  const double cx = width() / 2.0;
  const double cy = height() / 2.0;

  // Colors
  const QColor bg_color(0x2D, 0x2D, 0x2D);
  const QColor ring_color(0x4A, 0x4A, 0x4A);
  const QColor mark_color(0x5A, 0x5A, 0x5A);
  const QColor inactive_color(0x5A, 0x5A, 0x5A);
  const QColor label_color(0x9C, 0xA3, 0xAF);
  const QColor north_label_color(0xD4, 0xAF, 0x37);
  const QColor pointer_color = active_ ? QColor(0xD4, 0xAF, 0x37) : inactive_color;
  const QColor dot_color = active_ ? QColor(0xD4, 0xAF, 0x37) : inactive_color;
  const QColor center_color(0xE5, 0xE7, 0xEB);

  // --- Background circle ---
  painter.setPen(QPen(ring_color, 2.0));
  painter.setBrush(QBrush(bg_color));
  painter.drawEllipse(QPointF(cx, cy), radius, radius);

  // --- Compass dial marks ---
  painter.save();
  painter.translate(cx, cy);
  painter.setPen(QPen(mark_color, 1.5));
  for (int i = 0; i < 12; ++i) {
    const double angle_deg = i * 30.0;
    const double angle_rad = angle_deg * M_PI / 180.0;
    const double inner = (i % 3 == 0) ? radius * 0.72 : radius * 0.82;
    const double outer = radius * 0.92;
    const double x1 = std::sin(angle_rad) * inner;
    const double y1 = -std::cos(angle_rad) * inner;
    const double x2 = std::sin(angle_rad) * outer;
    const double y2 = -std::cos(angle_rad) * outer;
    painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
  }
  painter.restore();

  // --- Cardinal labels (N/S/E/W) ---
  QFont label_font = painter.font();
  label_font.setPixelSize(static_cast<int>(radius * 0.22));
  label_font.setBold(true);
  painter.setFont(label_font);

  struct CardinalLabel {
    const char *text;
    double angle_deg;  // 0=N, 90=E, 180=S, 270=W
    QColor color;
  };
  const CardinalLabel labels[] = {
    {"N",   0.0, active_ ? north_label_color : inactive_color},
    {"E",  90.0, active_ ? label_color : inactive_color},
    {"S", 180.0, active_ ? label_color : inactive_color},
    {"W", 270.0, active_ ? label_color : inactive_color},
  };

  for (const auto &lbl : labels) {
    const double angle_rad = lbl.angle_deg * M_PI / 180.0;
    const double label_r = radius * 0.58;
    const double lx = cx + std::sin(angle_rad) * label_r;
    const double ly = cy - std::cos(angle_rad) * label_r;
    painter.setPen(lbl.color);
    const int text_size = static_cast<int>(radius * 0.3);
    QRectF text_rect(lx - text_size / 2.0, ly - text_size / 2.0, text_size, text_size);
    painter.drawText(text_rect, Qt::AlignCenter, lbl.text);
  }

  // --- North pointer triangle (rotated by -yaw_deg_ since yaw is heading) ---
  painter.save();
  painter.translate(cx, cy);
  painter.rotate(-yaw_deg_);

  const double half_w = radius * 0.07;
  const double tip_len = radius * 0.42;
  const double tail_len = tip_len * 0.35;

  // Gold diamond tip (north)
  QPolygonF north_diamond;
  north_diamond << QPointF(0.0, -tip_len)
                << QPointF(-half_w, 0.0)
                << QPointF(0.0, -half_w * 0.5)
                << QPointF(half_w, 0.0);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QBrush(pointer_color));
  painter.drawPolygon(north_diamond);

  // Gray diamond tail (south)
  QPolygonF south_diamond;
  south_diamond << QPointF(0.0, tail_len)
                << QPointF(-half_w, 0.0)
                << QPointF(0.0, half_w * 0.5)
                << QPointF(half_w, 0.0);
  const QColor tail_color(0xAA, 0xAA, 0xBC);
  painter.setBrush(QBrush(active_ ? tail_color : inactive_color));
  painter.drawPolygon(south_diamond);

  painter.restore();

  // --- Roll/pitch crosshair dot ---
  {
    const double max_angle = 30.0;  // degrees
    const double max_offset = radius * 0.35;

    const double roll_clamped = std::clamp(roll_deg_, -max_angle, max_angle);
    const double pitch_clamped = std::clamp(pitch_deg_, -max_angle, max_angle);

    const double dx = (roll_clamped / max_angle) * max_offset;
    const double dy = -(pitch_clamped / max_angle) * max_offset;  // pitch up = dot up

    const double dot_x = cx + dx;
    const double dot_y = cy + dy;
    const double dot_r = radius * 0.06;

    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(dot_color));
    painter.drawEllipse(QPointF(dot_x, dot_y), dot_r, dot_r);
  }

  // --- Center dot ---
  {
    const double center_r = radius * 0.035;
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(center_color));
    painter.drawEllipse(QPointF(cx, cy), center_r, center_r);
  }
}

}  // namespace android_sensor_viz
