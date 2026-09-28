#pragma once
#include "EmulatorDiscovery.h"
#include <QObject>
#include <QThread>
#include <QTimer>
#include <functional>
#include <optional>

namespace trainer {
// Inventory runs off the GUI thread. Preparation/publication waits for the
// shell's idle gate, checked again after inventory completes.
class EmulatorRefresh final : public QObject {
    Q_OBJECT
public:
    using Reader = std::function<EmulatorEnvironment(const QString&)>;
    EmulatorRefresh(Reader, QObject* parent = nullptr);
    ~EmulatorRefresh() override;
    void request(const QString& root, bool scanLibrary = false);
    std::function<bool()> idle;
    std::function<void(const EmulatorEnvironment&, bool)> apply;
private:
    void advance();
    Reader read_;
    QThread thread_;
    QObject* worker_;
    QTimer retry_;
    QString root_;
    quint64 revision_ = 0;
    bool requested_ = false, reading_ = false, scan_ = false;
    std::optional<EmulatorEnvironment> ready_;
};
}
