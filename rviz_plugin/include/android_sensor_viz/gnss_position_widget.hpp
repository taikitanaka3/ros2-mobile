#ifndef ANDROID_SENSOR_VIZ_GNSS_POSITION_WIDGET_HPP_
#define ANDROID_SENSOR_VIZ_GNSS_POSITION_WIDGET_HPP_

#include <vector>
#include <QWidget>
#include <QPaintEvent>

namespace android_sensor_viz
{
class GnssPositionWidget : public QWidget
{
    Q_OBJECT
public:
    explicit GnssPositionWidget(QWidget *parent = nullptr);
    void setPosition(double lat, double lon, double alt, double accuracy);
    void setActive(bool active);
    void clearTrail();
protected:
    void paintEvent(QPaintEvent *event) override;
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;
private:
    struct TrailPoint {
        double lat;
        double lon;
    };

    std::vector<TrailPoint> trail_;
    double lat_{0}, lon_{0}, alt_{0}, accuracy_{-1};
    bool has_fix_{false};
    bool active_{false};

    static constexpr int kMaxTrail = 10;
    static constexpr double kMinMovementM = 1.0;

    static double haversineM(double lat1, double lon1, double lat2, double lon2);
};
}  // namespace android_sensor_viz
#endif
