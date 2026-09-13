#pragma once
#include <QObject>
#include <QWindow>

namespace trainer {
// Window-local presentation only: never consumes touch or mouse input.
class PointerVisibility final : public QObject {
public:
    explicit PointerVisibility(QWindow& window, bool dedicatedSession);
    void hide();
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    QWindow& window_;
    bool dedicatedSession_;
    bool hidden_ = true;
};
}
