#pragma once
#include <QObject>
#include <QTimer>
#include <QMap>
#include <QSocketNotifier>

namespace trainer {
// Physical media keys belong to the dedicated session, independent of game focus.
// Never grabs input or handles power/lid/gamepad events.
class VolumeKeys final : public QObject {
    Q_OBJECT
public:
    explicit VolumeKeys(bool enabled, QObject* parent = nullptr);
    ~VolumeKeys() override;
    void keyEvent(int code, int value);
signals:
    void adjustmentRequested(int delta);
private:
    void scan();
    void release();
    QMap<int, QSocketNotifier*> nodes_;
    QTimer scanTimer_, repeat_;
    int held_ = 0;
};
}
