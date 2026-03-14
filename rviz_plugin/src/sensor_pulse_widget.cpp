#include "android_sensor_viz/sensor_pulse_widget.hpp"

#include <QPainter>
#include <QPen>
#include <QFont>
#include <QColor>

#include <algorithm>
#include <cmath>

namespace android_sensor_viz
{

namespace
{
constexpr QColor kBackgroundColor{0x1E, 0x1E, 0x1E};
constexpr QColor kGridColor{0x2D, 0x2D, 0x2D};
constexpr QColor kLabelColor{0x6b, 0x72, 0x80};
constexpr QColor kInactiveColor{0x4b, 0x55, 0x63};

// Line colors matching Android SensorPulseGraphView
const QColor kLineColors[SensorPulseWidget::kNumLines] = {
    QColor{0xD4, 0xAF, 0x37},  // IMU - Gold
    QColor{0x2E, 0x8B, 0x57},  // GPS - SeaGreen
    QColor{0x33, 0x66, 0x99},  // BAT - Blue
    QColor{0xCC, 0x66, 0x33},  // RCAM - Warm Amber
    QColor{0x8B, 0x69, 0x14},  // FCAM - Dark Gold
    QColor{0x7B, 0x68, 0xEE},  // MAG - MediumSlateBlue
    QColor{0x20, 0xB2, 0xAA},  // BAR - LightSeaGreen
    QColor{0xCD, 0x5C, 0x5C},  // THM - IndianRed
    QColor{0x70, 0x80, 0x90},  // JOY - SlateGray
    QColor{0x8B, 0x45, 0x13},  // IR - SaddleBrown
};

const char * const kLineLabels[SensorPulseWidget::kNumLines] = {
    "IMU", "GPS", "BAT", "RCAM", "FCAM", "MAG", "BAR", "THM", "JOY", "IR"
};

constexpr double kMinMaxHz = 20.0;
constexpr double kGridStepHz = 5.0;
constexpr int kLeftMargin = 28;
constexpr int kRightMargin = 2;
constexpr int kTopMargin = 16;
constexpr int kBottomMargin = 2;
}  // namespace

SensorPulseWidget::SensorPulseWidget(QWidget *parent)
: QWidget(parent)
{
    setMinimumSize(minimumSizeHint());
    // Enable all lines by default
    for (int i = 0; i < kNumLines; ++i) {
        line_enabled_[i] = true;
    }
}

void SensorPulseWidget::addSample(int index, double hz)
{
    if (index < 0 || index >= kNumLines) {
        return;
    }

    // When IMU (index 0) is written, advance write_pos_ first
    if (index == 0) {
        write_pos_ = (write_pos_ + 1) % kHistorySize;
        if (sample_count_ < kHistorySize) {
            ++sample_count_;
        }
        // Clear all lines at the new position so stale data doesn't persist
        for (int i = 0; i < kNumLines; ++i) {
            history_[i][write_pos_] = 0.0;
        }
    }

    history_[index][write_pos_] = hz;
    update();
}

void SensorPulseWidget::setLineEnabled(int index, bool enabled)
{
    if (index >= 0 && index < kNumLines) {
        line_enabled_[index] = enabled;
        update();
    }
}

void SensorPulseWidget::setActive(bool active)
{
    active_ = active;
    update();
}

QSize SensorPulseWidget::minimumSizeHint() const
{
    return QSize(200, 80);
}

QSize SensorPulseWidget::sizeHint() const
{
    return QSize(300, 100);
}

void SensorPulseWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();

    // --- Background ---
    painter.fillRect(rect(), kBackgroundColor);

    // Graph area
    const int graph_left = kLeftMargin;
    const int graph_right = w - kRightMargin;
    const int graph_top = kTopMargin;
    const int graph_bottom = h - kBottomMargin;
    const int graph_w = graph_right - graph_left;
    const int graph_h = graph_bottom - graph_top;

    if (graph_w <= 0 || graph_h <= 0) {
        return;
    }

    // --- Determine Y-axis max (only from enabled lines) ---
    double max_val = 0.0;
    const int n = std::min(sample_count_, kHistorySize);
    for (int line = 0; line < kNumLines; ++line) {
        if (!line_enabled_[line]) {
            continue;
        }
        for (int i = 0; i < n; ++i) {
            int idx = (write_pos_ - n + 1 + i + kHistorySize) % kHistorySize;
            if (history_[line][idx] > max_val) {
                max_val = history_[line][idx];
            }
        }
    }
    // Snap to next multiple of kGridStepHz, minimum kMinMaxHz
    double y_max = kMinMaxHz;
    if (max_val > kMinMaxHz) {
        y_max = std::ceil(max_val / kGridStepHz) * kGridStepHz;
    }

    // --- Grid lines and Y-axis labels ---
    {
        QPen grid_pen(kGridColor, 1.0);
        painter.setPen(grid_pen);

        QFont label_font;
        label_font.setPixelSize(9);
        painter.setFont(label_font);

        const int num_grid_lines = static_cast<int>(y_max / kGridStepHz);
        for (int g = 0; g <= num_grid_lines; ++g) {
            double hz_val = g * kGridStepHz;
            int y = graph_bottom - static_cast<int>((hz_val / y_max) * graph_h);

            // Grid line
            painter.setPen(grid_pen);
            painter.drawLine(graph_left, y, graph_right, y);

            // Label
            painter.setPen(kLabelColor);
            QString label = QString::number(static_cast<int>(hz_val));
            QRect label_rect(0, y - 6, kLeftMargin - 3, 12);
            painter.drawText(label_rect, Qt::AlignRight | Qt::AlignVCenter, label);
        }
    }

    // --- Sensor lines (only enabled) ---
    if (n >= 2) {
        QPen line_pen;
        line_pen.setWidthF(1.5);
        line_pen.setCapStyle(Qt::RoundCap);
        line_pen.setJoinStyle(Qt::RoundJoin);

        for (int line = 0; line < kNumLines; ++line) {
            if (!line_enabled_[line]) {
                continue;
            }

            QColor color = active_ ? kLineColors[line] : kInactiveColor;
            line_pen.setColor(color);
            painter.setPen(line_pen);

            // Build points, skipping zero-valued samples
            bool prev_valid = false;
            QPointF prev_pt;

            for (int i = 0; i < n; ++i) {
                int idx = (write_pos_ - n + 1 + i + kHistorySize) % kHistorySize;
                double val = history_[line][idx];

                if (val <= 0.0) {
                    prev_valid = false;
                    continue;
                }

                double x = graph_left + (static_cast<double>(i) / (n - 1)) * graph_w;
                double y = graph_bottom - (val / y_max) * graph_h;
                QPointF pt(x, y);

                if (prev_valid) {
                    painter.drawLine(prev_pt, pt);
                }

                prev_pt = pt;
                prev_valid = true;
            }
        }
    }

    // --- Legend (top area, wrapping to 2 rows if needed) ---
    {
        QFont legend_font;
        legend_font.setPixelSize(8);
        painter.setFont(legend_font);
        QFontMetrics fm(legend_font);

        const int dot_size = 5;
        const int spacing = 4;
        const int entry_gap = 6;
        const int max_legend_w = graph_w;
        const int row_height = 10;

        int lx = graph_left;
        int ly = 2;
        int row = 0;

        for (int i = 0; i < kNumLines; ++i) {
            if (!line_enabled_[i]) {
                continue;
            }

            int entry_w = dot_size + spacing + fm.horizontalAdvance(kLineLabels[i]) + entry_gap;
            if (lx + entry_w > graph_left + max_legend_w && lx > graph_left) {
                // Wrap to next row
                lx = graph_left;
                row++;
                if (row >= 2) {
                    break;
                }
            }

            int row_y = ly + row * row_height;
            QColor color = active_ ? kLineColors[i] : kInactiveColor;

            // Colored dot
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawEllipse(lx, row_y, dot_size, dot_size);
            lx += dot_size + spacing;

            // Label text
            painter.setPen(color);
            painter.drawText(lx, row_y + dot_size - 1, kLineLabels[i]);
            lx += fm.horizontalAdvance(kLineLabels[i]) + entry_gap;
        }
    }
}

}  // namespace android_sensor_viz
