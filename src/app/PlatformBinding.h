#pragma once
#include "platform/sdk/PlatformHost.h"
#include "platform/sdk/SdkNetwork.h"
#include "core/navigation/ShellController.h"
#include "core/navigation/AdventureLaunchController.h"
#include <QUuid>
#include <QDebug>
#include <QStandardPaths>
#include <QFileInfo>

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
        const auto data=QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/TrainerOS";
        auto root=data+"/platform/records";
        if(!QFileInfo::exists(root+"/secure-channel.py"))root=data+"/emulators/melonds-netplay/transport";
#ifdef Q_OS_WIN
        const auto interpreter=root+"/venv/Scripts/python.exe";
#else
        const auto interpreter=root+"/venv/bin/python";
#endif
        network_=std::make_unique<SdkNetwork>(*shell.social()->lobbyCoordinator(),*shell.social()->lobbyRuntime(),
            qEnvironmentVariable("TRAINEROS_RECORD_PYTHON",interpreter),
            qEnvironmentVariable("TRAINEROS_RECORD_HELPER",root+"/secure-channel.py"));
        host_.setNetwork(network_.get());
        network_->launchInstalled=[this](const QString& local){
            if(local.isEmpty()||launch_.active()||shell_.launchPreparation().busy())return false;
            shell_.launchPreparation().launch(local);return true;
        };
        connect(network_.get(),&SdkNetwork::activitiesChanged,this,[this]{shell_.social()->setLobbyActivities(network_->activities());});
        connect(shell_.crates()->installer(),&CratesInstaller::inventoryChanged,this,[this]{network_->setInstalledActivities(shell_.crates()->installer()->activities());});
        network_->setInstalledActivities(shell_.crates()->installer()->activities());
        // Use the same route as shell invitations: it also requests a safe
        // game-to-shell handoff, rather than changing a page behind the game.
        connect(network_.get(),&SdkNetwork::openLobbyRequested,this,[this]{shell_.social()->openLobby();});
        sync();
        QString error;
        if (!host_.start(endpoint, &error)) qWarning().noquote() << "Platform SDK unavailable:" << error;
    }
private:
    ShellController& shell_;
    AdventureLaunchController& launch_;
    PlatformHost host_;
    std::unique_ptr<SdkNetwork> network_;
    QString session_;
    void sync() {
        const auto owner = shell_.trainer()->profile()["id"].toString();
        // setOwner is idempotent. A profile change clears old UI before rebinding.
        shell_.controls()->setOwner(owner);
        if (launch_.state() == "running" && session_.isEmpty())
            session_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (!launch_.active()) session_.clear();
        host_.setContext({{"profile", owner},
            {"account",shell_.backendAccount()->snapshot()},
            {"selected", QJsonObject{{"id", shell_.currentAdventureId()}}},
            {"game", QJsonObject{{"id", launch_.adventureId()}, {"session", session_},
                {"state", launch_.state()}, {"minimized", launch_.minimized()}}},
            {"app", QJsonObject{{"id", shell_.apps()->current()}}}});
        auto avatar=shell_.trainer()->avatarDescriptor();
        if(!avatar.isEmpty())avatar["profile"]=owner;
        host_.setAvatar(avatar,shell_.trainer()->avatarAvailable());
    }
};
}
