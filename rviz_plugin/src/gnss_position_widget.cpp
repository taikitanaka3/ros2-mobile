#include "android_sensor_viz/gnss_position_widget.hpp"

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
const QColor kBg(0x2D, 0x2D, 0x2D);
const QColor kPlotBg(0x25, 0x25, 0x25);
const QColor kGrid(0x3A, 0x3A, 0x3A);
const QColor kGold(0xD4, 0xAF, 0x37);
const QColor kText(0xE5, 0xE7, 0xEB);
const QColor kSubtext(0x9C, 0xA3, 0xAF);
const QColor kInactive(0x55, 0x55, 0x70);

constexpr double kDegToM = 111320.0; // approx meters per degree latitude
}  // namespace

GnssPositionWidget::GnssPositionWidget(QWidget *parent)
: QWidget(parent)
{
    setMinimumSize(minimumSizeHint());
}

double GnssPositionWidget::haversineM(double lat1, double lon1, double lat2, double lon2)
{
    const double dlat = (lat2 - lat1) * M_PI / 180.0;
    const double dlon = (lon2 - lon1) * M_PI / 180.0;
    const double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
        std::cos(lat1 * M_PI / 180.0) * std::cos(lat2 * M_PI / 180.0) *
        std::sin(dlon / 2) * std::sin(dlon / 2);
    return 6371000.0 * 2.0 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
}

void GnssPositionWidget::setPosition(double lat, double lon, double alt, double accuracy)
{
    lat_ = lat; lon_ = lon; alt_ = alt; accuracy_ = accuracy;
    has_fix_ = true;

    // Add trail point if moved > kMinMovementM
    if (trail_.empty() || haversineM(trail_.back().lat, trail_.back().lon, lat, lon) >= kMinMovementM) {
        trail_.push_back({lat, lon});
        if (static_cast<int>(trail_.size()) > kMaxTrail) {
            trail_.erase(trail_.begin());
        }
    }

    update();
}

void GnssPositionWidget::setActive(bool active)
{
    active_ = active;
    update();
}

void GnssPositionWidget::clearTrail()
{
    trail_.clear();
    has_fix_ = false;
    update();
}

QSize GnssPositionWidget::minimumSizeHint() const { return QSize(160, 180); }
QSize GnssPositionWidget::sizeHint() const { return QSize(200, 200); }

void GnssPositionWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();

    // Background
    p.setPen(Qt::NoPen);
    p.setBrush(kBg);
    p.drawRoundedRect(rect(), 6, 6);

    if (!has_fix_ || !active_) {
        QFont f = p.font();
        f.setPixelSize(11);
        p.setFont(f);
        p.setPen(kInactive);
        p.drawText(rect(), Qt::AlignCenter, has_fix_ ? "GNSS inactive" : "Waiting for GPS...");
        return;
    }

    // Plot area
    const double plot_size = std::min(w - 16.0, h - 70.0);
    const double plot_cx = w / 2.0;
    const double plot_cy = 8 + plot_size / 2.0;
    const double plot_r = plot_size / 2.0 - 4;

    // Plot background circle
    p.setPen(QPen(kGrid, 1.0));
    p.setBrush(kPlotBg);
    p.drawEllipse(QPointF(plot_cx, plot_cy), plot_r, plot_r);

    // Crosshair
    p.setPen(QPen(kGrid, 0.5, Qt::DashLine));
    p.drawLine(QPointF(plot_cx - plot_r, plot_cy), QPointF(plot_cx + plot_r, plot_cy));
    p.drawLine(QPointF(plot_cx, plot_cy - plot_r), QPointF(plot_cx, plot_cy + plot_r));

    // Scale ring (50%)
    p.drawEllipse(QPointF(plot_cx, plot_cy), plot_r * 0.5, plot_r * 0.5);

    // N/S/E/W labels
    QFont lf = p.font();
    lf.setPixelSize(8);
    lf.setBold(true);
    p.setFont(lf);
    p.setPen(kSubtext);
    const double lr = plot_r + 1;
    p.drawText(QRectF(plot_cx - 8, plot_cy - lr - 12, 16, 12), Qt::AlignCenter, "N");
    p.drawText(QRectF(plot_cx - 8, plot_cy + lr, 16, 12), Qt::AlignCenter, "S");
    p.drawText(QRectF(plot_cx + lr, plot_cy - 6, 12, 12), Qt::AlignCenter, "E");
    p.drawText(QRectF(plot_cx - lr - 12, plot_cy - 6, 12, 12), Qt::AlignCenter, "W");

    // Compute scale: find max extent from current position in meters
    double max_extent = 5.0; // minimum 5m
    for (const auto &pt : trail_) {
        const double dlat_m = (pt.lat - lat_) * kDegToM;
        const double dlon_m = (pt.lon - lon_) * kDegToM * std::cos(lat_ * M_PI / 180.0);
        max_extent = std::max(max_extent, std::max(std::abs(dlat_m), std::abs(dlon_m)));
    }
    max_extent *= 1.3; // padding
    const double px_per_m = (plot_r * 0.8) / max_extent;

    // Draw trail dots (oldest = small & dim, newest = large & bright)
    for (size_t i = 0; i < trail_.size(); ++i) {
        const double dlat_m = (trail_[i].lat - lat_) * kDegToM;
        const double dlon_m = (trail_[i].lon - lon_) * kDegToM * std::cos(lat_ * M_PI / 180.0);

        const double dx = dlon_m * px_per_m;
        const double dy = -dlat_m * px_per_m; // north = up

        const double age_ratio = static_cast<double>(i) / std::max(1.0, static_cast<double>(trail_.size() - 1));
        const double dot_r = 1.5 + age_ratio * 2.5;
        const int alpha = 80 + static_cast<int>(age_ratio * 175);

        QColor c = kGold;
        c.setAlpha(alpha);
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(QPointF(plot_cx + dx, plot_cy + dy), dot_r, dot_r);
    }

    // Current position (large gold dot)
    p.setBrush(kGold);
    p.drawEllipse(QPointF(plot_cx, plot_cy), 4.0, 4.0);

    // Scale text
    lf.setPixelSize(7);
    lf.setBold(false);
    p.setFont(lf);
    p.setPen(kSubtext);
    QString scale_text = (max_extent < 1000) ?
        QString("%1 m").arg(max_extent, 0, 'f', 0) :
        QString("%1 km").arg(max_extent / 1000.0, 0, 'f', 1);
    p.drawText(QRectF(plot_cx + plot_r * 0.5 - 20, plot_cy + 2, 40, 10),
               Qt::AlignCenter, scale_text);

    // Text area below plot
    const double text_top = plot_cy + plot_r + 14;
    QFont tf = p.font();
    tf.setPixelSize(10);
    tf.setBold(false);
    p.setFont(tf);
    p.setPen(kText);

    const char ns = lat_ >= 0 ? 'N' : 'S';
    const char ew = lon_ >= 0 ? 'E' : 'W';
    p.drawText(QRectF(4, text_top, w - 8, 14), Qt::AlignCenter,
               QString("%1%2  %3%4")
                   .arg(std::abs(lat_), 0, 'f', 6).arg(ns)
                   .arg(std::abs(lon_), 0, 'f', 6).arg(ew));

    p.drawText(QRectF(4, text_top + 14, w - 8, 14), Qt::AlignCenter,
               QString("Alt %1 m  Acc %2 m")
                   .arg(alt_, 0, 'f', 1)
                   .arg(accuracy_ >= 0 ? QString::number(accuracy_, 'f', 1) : "--"));
}

}  // namespace android_sensor_viz
