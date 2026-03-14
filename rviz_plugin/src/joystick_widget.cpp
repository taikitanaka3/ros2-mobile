#include "android_sensor_viz/joystick_widget.hpp"

#include <algorithm>
#include <cmath>

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QLinearGradient>

namespace android_sensor_viz
{

namespace
{
const QColor kBaseFill(0x2D, 0x2D, 0x2D);
const QColor kBaseBorder(0x4A, 0x4A, 0x4A);
const QColor kGuide(0x3A, 0x3A, 0x3A);
const QColor kKnobTop(0xD4, 0xAF, 0x37);
const QColor kKnobBot(0xB8, 0x86, 0x0B);
const QColor kKnobBorder(0xB8, 0x96, 0x0C);
const QColor kBtnOn(0x22, 0xC5, 0x5E);
const QColor kBtnOff(0x4B, 0x55, 0x63);
const QColor kBg(0x2D, 0x2D, 0x2D);
const QColor kText(0xE5, 0xE7, 0xEB);
const QColor kSubtext(0x9C, 0xA3, 0xAF);
const QColor kInactive(0x55, 0x55, 0x70);

// Number of concentric grid rings inside the base circle
constexpr int kRingCount = 7;
// Maximum number of button dots rendered
constexpr int kMaxButtons = 8;
// Button dot radius in pixels
constexpr double kBtnDotRadius = 4.0;
// Button dot center-to-center stride in pixels
constexpr double kBtnStride = 14.0;
}  // namespace

JoystickWidget::JoystickWidget(QWidget *parent)
: QWidget(parent)
{
    setMinimumSize(minimumSizeHint());
}

void JoystickWidget::setAxes(float x, float y)
{
    axis_x_ = std::clamp(x, -1.0f, 1.0f);
    axis_y_ = std::clamp(y, -1.0f, 1.0f);
    update();
}

void JoystickWidget::setButtons(const std::vector<int> &buttons)
{
    buttons_ = buttons;
    update();
}

void JoystickWidget::setActive(bool active)
{
    if (!active) {
        axis_x_ = 0.0f;
        axis_y_ = 0.0f;
        buttons_.clear();
    }
    active_ = active;
    update();
}

QSize JoystickWidget::minimumSizeHint() const { return QSize(140, 160); }
QSize JoystickWidget::sizeHint() const { return QSize(160, 180); }

void JoystickWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();

    // Background
    p.setPen(Qt::NoPen);
    p.setBrush(kBg);
    p.drawRoundedRect(rect(), 6, 6);

    // Joystick area (leave room for button indicators at bottom)
    const int btn_area = buttons_.empty() ? 0 : 20;
    const int joy_h = h - btn_area;
    const double side = std::min(static_cast<double>(w), static_cast<double>(joy_h));
    const double cx = w / 2.0;
    const double cy = joy_h / 2.0;
    const double base_r = side * 0.42;
    const double knob_r = side * 0.11;

    // Base circle (#1E1E32 fill, #444460 border)
    p.setPen(QPen(active_ ? kBaseBorder : kInactive, 3.0));
    p.setBrush(kBaseFill);
    p.drawEllipse(QPointF(cx, cy), base_r, base_r);

    // Crosshair guide lines spanning the full base circle
    p.setPen(QPen(kGuide, 1.5));
    p.drawLine(QPointF(cx - base_r, cy), QPointF(cx + base_r, cy));
    p.drawLine(QPointF(cx, cy - base_r), QPointF(cx, cy + base_r));

    // 7 concentric rings evenly spaced inside the base circle.
    // Ring i is drawn at radius (base_r * i / 8) so the outermost ring
    // sits at 7/8 of the base radius, leaving the base circle edge as
    // the implicit 8th boundary.
    p.setPen(QPen(kGuide, 0.5));
    p.setBrush(Qt::NoBrush);
    for (int i = 1; i <= kRingCount; ++i) {
        const double r = base_r * i / (kRingCount + 1.0);
        p.drawEllipse(QPointF(cx, cy), r, r);
    }

    // Compute knob position. Clamp the displacement vector to the base
    // circle so diagonal extremes (e.g. axes = 1,1) do not escape.
    double dx = static_cast<double>(axis_x_);
    double dy = static_cast<double>(-axis_y_);  // Y inverted: positive = up
    const double mag = std::sqrt(dx * dx + dy * dy);
    if (mag > 1.0) {
        dx /= mag;
        dy /= mag;
    }
    const double knob_x = cx + dx * base_r;
    const double knob_y = cy + dy * base_r;

    // Clip rendering to the base circle so the knob never bleeds outside
    {
        QPainterPath clip;
        clip.addEllipse(QPointF(cx, cy), base_r + knob_r, base_r + knob_r);
        p.setClipPath(clip);
    }

    // Crosshair at knob position (scaled to widget size)
    if (active_) {
        const double ch_len = side * 0.06;
        p.setPen(QPen(kKnobBorder, 0.8));
        p.drawLine(QPointF(knob_x - ch_len, knob_y), QPointF(knob_x + ch_len, knob_y));
        p.drawLine(QPointF(knob_x, knob_y - ch_len), QPointF(knob_x, knob_y + ch_len));
    }

    // Knob circle with gradient (#5CC5FF top -> #2A74B4 bottom)
    QLinearGradient grad(QPointF(knob_x, knob_y - knob_r),
                         QPointF(knob_x, knob_y + knob_r));
    if (active_) {
        grad.setColorAt(0, kKnobTop);
        grad.setColorAt(1, kKnobBot);
    } else {
        grad.setColorAt(0, kInactive);
        grad.setColorAt(1, kInactive.darker(130));
    }
    p.setPen(QPen(active_ ? kKnobBorder : kInactive, 2.0));
    p.setBrush(grad);
    p.drawEllipse(QPointF(knob_x, knob_y), knob_r, knob_r);

    // Remove clip for text and button drawing
    p.setClipping(false);

    // Axis values text (positioned inside the widget margins)
    QFont vf = p.font();
    vf.setPixelSize(static_cast<int>(std::max(8.0, side * 0.055)));
    p.setFont(vf);
    p.setPen(active_ ? kSubtext : kInactive);

    // X label: right of base circle, clamped to stay within widget
    const double x_label_left = std::min(cx + base_r + 2.0, static_cast<double>(w) - 40.0);
    p.drawText(QRectF(x_label_left, cy - 6, 40, 12), Qt::AlignLeft,
               QString("X:%1").arg(axis_x_, 0, 'f', 2));

    // Y label: above base circle, clamped to stay within widget
    const double y_label_top = std::max(cy - base_r - 13.0, 1.0);
    p.drawText(QRectF(cx - 20, y_label_top, 40, 12), Qt::AlignCenter,
               QString("Y:%1").arg(axis_y_, 0, 'f', 2));

    // Button indicator dots (ON = green, OFF = gray)
    if (!buttons_.empty()) {
        const double btn_y = h - 10.0;
        const int n = std::min(static_cast<int>(buttons_.size()), kMaxButtons);
        // Total span: n dots of diameter 2*kBtnDotRadius with (n-1) gaps.
        // Stride between centers = kBtnStride.
        const double total_span = (n - 1) * kBtnStride + kBtnDotRadius * 2.0;
        double bx = (w - total_span) / 2.0 + kBtnDotRadius;
        for (int i = 0; i < n; ++i) {
            const bool on = buttons_[i] != 0;
            p.setPen(Qt::NoPen);
            p.setBrush(active_ ? (on ? kBtnOn : kBtnOff) : kInactive);
            p.drawEllipse(QPointF(bx, btn_y), kBtnDotRadius, kBtnDotRadius);
            bx += kBtnStride;
        }
    }
}

}  // namespace android_sensor_viz
