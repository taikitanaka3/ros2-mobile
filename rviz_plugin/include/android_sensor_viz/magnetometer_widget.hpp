#ifndef ANDROID_SENSOR_VIZ_MAGNETOMETER_WIDGET_HPP_
#define ANDROID_SENSOR_VIZ_MAGNETOMETER_WIDGET_HPP_

#include <QWidget>
#include <QPaintEvent>

namespace android_sensor_viz
{
class MagnetometerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MagnetometerWidget(QWidget *parent = nullptr);
    void setField(double x_ut, double y_ut, double z_ut);
    void setActive(bool active);
protected:
    void paintEvent(QPaintEvent *event) override;
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;
private:
    void drawCompassRose(QPainter &p, double cx, double cy, double radius) const;
    void drawFieldBar(QPainter &p, double x, double y, double w, double h,
                      double value, double max_val, const QColor &color,
                      const QString &label) const;

    double field_x_{0}, field_y_{0}, field_z_{0};
    bool active_{false};
};
}  // namespace android_sensor_viz
#endif
