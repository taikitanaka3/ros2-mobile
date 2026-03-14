#ifndef ANDROID_SENSOR_VIZ_COMPASS_WIDGET_HPP_
#define ANDROID_SENSOR_VIZ_COMPASS_WIDGET_HPP_

#include <QWidget>
#include <QPaintEvent>

namespace android_sensor_viz
{
class CompassWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CompassWidget(QWidget *parent = nullptr);
    void setOrientation(double qx, double qy, double qz, double qw);
    void setActive(bool active);
    double yawDeg() const { return yaw_deg_; }
    double rollDeg() const { return roll_deg_; }
    double pitchDeg() const { return pitch_deg_; }
protected:
    void paintEvent(QPaintEvent *event) override;
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;
private:
    double roll_deg_{0.0};
    double pitch_deg_{0.0};
    double yaw_deg_{0.0};
    bool active_{false};
};
}  // namespace android_sensor_viz
#endif
