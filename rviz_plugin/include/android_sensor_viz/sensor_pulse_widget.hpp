#ifndef ANDROID_SENSOR_VIZ_SENSOR_PULSE_WIDGET_HPP_
#define ANDROID_SENSOR_VIZ_SENSOR_PULSE_WIDGET_HPP_

#include <QWidget>
#include <QPaintEvent>

namespace android_sensor_viz
{
class SensorPulseWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SensorPulseWidget(QWidget *parent = nullptr);

    /// Push a new Hz sample for the given sensor line.
    /// index: 0=IMU, 1=GPS, 2=BAT, 3=RCAM, 4=FCAM, 5=MAG, 6=BAR, 7=THM, 8=JOY, 9=IR
    void addSample(int index, double hz);

    /// Enable/disable a specific line by index.
    void setLineEnabled(int index, bool enabled);

    void setActive(bool active);

    static constexpr int kNumLines = 10;

protected:
    void paintEvent(QPaintEvent *event) override;
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

private:
    static constexpr int kHistorySize = 200;
    double history_[kNumLines][kHistorySize]{};
    bool line_enabled_[kNumLines]{};
    int write_pos_{0};
    int sample_count_{0};
    bool active_{false};
};
}  // namespace android_sensor_viz
#endif
