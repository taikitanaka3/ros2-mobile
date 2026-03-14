#ifndef ANDROID_SENSOR_VIZ_JOYSTICK_WIDGET_HPP_
#define ANDROID_SENSOR_VIZ_JOYSTICK_WIDGET_HPP_

#include <vector>
#include <QWidget>
#include <QPaintEvent>

namespace android_sensor_viz
{
class JoystickWidget : public QWidget
{
    Q_OBJECT
public:
    explicit JoystickWidget(QWidget *parent = nullptr);
    void setAxes(float x, float y);
    void setButtons(const std::vector<int> &buttons);
    void setActive(bool active);
protected:
    void paintEvent(QPaintEvent *event) override;
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;
private:
    float axis_x_{0}, axis_y_{0};
    std::vector<int> buttons_;
    bool active_{false};
};
}  // namespace android_sensor_viz
#endif
