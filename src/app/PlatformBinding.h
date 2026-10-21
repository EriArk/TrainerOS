#pragma once
#include "platform/sdk/PlatformHost.h"
#include "core/navigation/ShellController.h"
#include "core/navigation/AdventureLaunchController.h"
#include <QUuid>
#include <QDebug>

namespace trainer {
// Composition only: real controllers remain the source of lifecycle/context.
class PlatformBinding final : public QObject {
public:
    PlatformBinding(ShellController& shell, AdventureLaunchController& launch,
                    const QString& endpoint, QObject* parent = nullptr)
        : QObject(parent), shell_(shell), launch_(launch), host_(*shell.controls()) {
        connect(&shell, &ShellController::changed, this, [this] { sync(); });
        connect(shell.trainer(), &TrainerController::changed, this, [this] { sync(); });
        connect(shell.apps(), &AppsController::changed, this, [this] { sync(); });
        connect(&launch, &AdventureLaunchController::changed, this, [this] { sync(); });
        connect(shell.powerControl(), &PowerControl::woke, this, [this] { host_.notifyLifecycle("wake"); });
        sync();
        QString error;
        if (!host_.start(endpoint, &error)) qWarning().noquote() << "Platform SDK unavailable:" << error;
    }
private:
    ShellController& shell_;
    AdventureLaunchController& launch_;
    PlatformHost host_;
    QString session_;
    void sync() {
        const auto owner = shell_.trainer()->profile()["id"].toString();
        // setOwner is idempotent. A profile change clears old UI before rebinding.
        shell_.controls()->setOwner(owner);
        if (launch_.state() == "running" && session_.isEmpty())
            session_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (!launch_.active()) session_.clear();
        host_.setContext({{"profile", owner},
            {"selected", QJsonObject{{"id", shell_.currentAdventureId()}}},
            {"game", QJsonObject{{"id", launch_.adventureId()}, {"session", session_},
                {"state", launch_.state()}, {"minimized", launch_.minimized()}}},
            {"app", QJsonObject{{"id", shell_.apps()->current()}}}});
    }
};
}
