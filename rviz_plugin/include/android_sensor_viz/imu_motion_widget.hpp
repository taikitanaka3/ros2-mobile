#ifndef ANDROID_SENSOR_VIZ_IMU_MOTION_WIDGET_HPP_
#define ANDROID_SENSOR_VIZ_IMU_MOTION_WIDGET_HPP_

#include <QWidget>
#include <QPaintEvent>

namespace android_sensor_viz
{
class ImuMotionWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ImuMotionWidget(QWidget *parent = nullptr);
    void setData(double ax, double ay, double az,
                 double gx, double gy, double gz);
    void setActive(bool active);
protected:
    void paintEvent(QPaintEvent *event) override;
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;
private:
    void drawRingGauge(QPainter &p, double cx, double cy, double radius,
                       double value, double max_val, const QColor &color,
                       const QString &label, const QString &unit_text) const;
    void drawAccelBar(QPainter &p, double x, double y, double w, double h,
                      double value, double max_val, const QColor &color,
                      const QString &label) const;

    double accel_x_{0}, accel_y_{0}, accel_z_{0};
    double gyro_x_{0}, gyro_y_{0}, gyro_z_{0};
    bool active_{false};
};
}  // namespace android_sensor_viz
#endif
