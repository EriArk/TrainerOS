#include "AdventureOverlayService.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonDocument>
#include <QLoggingCategory>

namespace trainer {
Q_LOGGING_CATEGORY(overlayLog, "trainer.overlay")
AdventureOverlayService::AdventureOverlayService(ProcessService& game, AdventureLaunchController& launch,
        AdventureExitPresentation& view, const QString& helper, QObject* parent)
    : QObject(parent), game_(game), exit_(launch.exitController()), view_(view), launch_(launch), helper_(helper) {
    heartbeat_.setInterval(250);
    retry_.setInterval(1000); retry_.setSingleShot(true);
    stopDeadline_.setInterval(1500); stopDeadline_.setSingleShot(true);
    handoffDeadline_.setInterval(5000); handoffDeadline_.setSingleShot(true);
    connect(&handoffDeadline_, &QTimer::timeout, this, [this] {
        // A silent helper cannot retain the exclusive input lease indefinitely.
        // Its independent watchdog releases input back to the owned game.
        helperProcess_.kill();
        emit handoffFailed("The control handoff timed out. Returning control to the game.");
    });
    connect(&stopDeadline_, &QTimer::timeout, this, [this] { helperProcess_.kill(); }); // Helper only; watchdog survives.
    connect(&retry_, &QTimer::timeout, this, &AdventureOverlayService::start);
    connect(&heartbeat_, &QTimer::timeout, this, [this] { send({{"command", "ping"}}); });
    connect(&launch, &AdventureLaunchController::suspendRequested, this, [this] { active_ = true; start(); });
    connect(&launch, &AdventureLaunchController::restoreRequested, this, [this] { stop(); });
    connect(&helperProcess_, &QProcess::started, this, [this] { heartbeat_.start(); send({{"command", "ping"}}); });
    connect(&helperProcess_, &QProcess::readyReadStandardOutput, this, &AdventureOverlayService::receive);
    connect(&helperProcess_, &QProcess::readyReadStandardError, this, [this] { helperProcess_.readAllStandardError(); });
    connect(&helperProcess_, &QProcess::finished, this, [this] { stopDeadline_.stop(); lost(); });
    connect(&helperProcess_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) lost();
    });
    connect(&exit_, &AdventureExitController::captureRequested, this, [this](quint64 token) {
        attempt_ = token;
        view_.setInputIsolated(true);
        send({{"command", "capture"}, {"token", QString::number(token)}});
    });
    connect(&view_, &AdventureExitPresentation::menuCaptureRequested, this, [this](quint64 token) {
        view_.setInputIsolated(true);
        send({{"command", "preview"}, {"token", QString::number(token)}});
    });
    connect(&view_, &AdventureExitPresentation::menuDismissed, this, [this] { send({{"command", "cancel"}}); });
    connect(&exit_, &AdventureExitController::returnToGameRequested, this, [this] { send({{"command", "cancel"}}); });
    connect(&exit_, &AdventureExitController::gracefulExitRequested, this, [this](quint64 token) {
        send({{"command", "close"}, {"token", QString::number(token)}});
    });
    connect(&view_, &AdventureExitPresentation::changed, this, [this] {
        if (sentEpoch_ == view_.inputGeneration()) return;
        sentEpoch_ = view_.inputGeneration();
        send({{"command", "context"}, {"epoch", QString::number(sentEpoch_)}});
    });
}
AdventureOverlayService::~AdventureOverlayService() {
    active_ = false; heartbeat_.stop(); retry_.stop();
    helperProcess_.closeWriteChannel();
    if (helperProcess_.state() != QProcess::NotRunning && !helperProcess_.waitForFinished(200)) {
        helperProcess_.kill(); helperProcess_.waitForFinished(500);
    }
}
void AdventureOverlayService::start() {
    if (!active_ || !game_.processId() || helperProcess_.state() != QProcess::NotRunning) return;
    const QFileInfo file(helper_);
    if (!file.isAbsolute() || !file.isFile() || !file.isReadable()) return;
    temporary_ = std::make_unique<QTemporaryDir>();
    if (!temporary_->isValid()) return;
    buffer_.clear(); sentEpoch_ = 0;
    helperProcess_.start("/usr/bin/python3", {helper_, "--game", QString::number(game_.processId()),
        "--shell", QString::number(QCoreApplication::applicationPid()), "--directory", temporary_->path()});
}
void AdventureOverlayService::stop() {
    active_ = false; retry_.stop(); heartbeat_.stop(); handoffDeadline_.stop();
    exit_.setAvailable(false); view_.setInputIsolated(false);
    if (helperProcess_.state() != QProcess::NotRunning) {
        send({{"command", "stop"}}); helperProcess_.closeWriteChannel(); stopDeadline_.start();
    }
}
void AdventureOverlayService::lost() {
    handoffDeadline_.stop();
    protocol_=0;shellReady_=false;returning_=minimizing_=false;
    if(launch_.minimized()){emit gameRequested();launch_.setMinimized(false);}
    heartbeat_.stop(); exit_.setAvailable(false); view_.setInputIsolated(false);
    if (exit_.phase() == AdventureExitController::Phase::Closing) {
        // The helper watches the OS process, while ProcessService remains
        // active during adapter settlement. Its EOF must not erase a confirmed
        // capture in that gap. Allow queued child-exit notifications to settle;
        // only a still-running child represents a lost close connection.
        const auto token = attempt_;
        QTimer::singleShot(200, this, [this, token] {
            if (token == attempt_ && exit_.phase() == AdventureExitController::Phase::Closing && game_.processId())
                exit_.gracefulExitFailed(token, "The exit connection was lost. Your game has not been forced to stop.");
        });
    }
    if (active_ && game_.active() && game_.processId()) retry_.start();
}
void AdventureOverlayService::send(const QJsonObject& object) {
    if (helperProcess_.state() != QProcess::Running) return;
    if (helperProcess_.bytesToWrite() > 16384) { helperProcess_.kill(); return; }
    helperProcess_.write(QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n');
}
void AdventureOverlayService::receive() {
    buffer_ += helperProcess_.readAllStandardOutput();
    if (buffer_.size() > 65536) { helperProcess_.kill(); lost(); buffer_.clear(); return; }
    while (buffer_.contains('\n')) {
        const auto index = buffer_.indexOf('\n');
        const auto message = QJsonDocument::fromJson(buffer_.left(index)).object(); buffer_.remove(0, index + 1);
        if (!active_ || !game_.active()) continue;
        const auto event = message["event"].toString();
        if (event == "ready") {protocol_=message["protocol"].toInt();exit_.setAvailable(protocol_==2 || protocol_==3);}
        else if(event=="minimized") {
            if(minimizing_ && launch_.setMinimized(true)){handoffDeadline_.stop();minimizing_=false;shellReady_=false;heldAction_.clear();view_.handOffToShell();emit shellRequested();}
        } else if(event=="shell-input" && launch_.minimized() && !returning_ && message["epoch"].toString().toULongLong()==view_.inputGeneration())shellInput(message);
        else if(event=="returned") {
            if(returning_){handoffDeadline_.stop();returning_=false;launch_.setMinimized(false);}
        } else if(event=="handoff-failed") {
            handoffDeadline_.stop();
            returning_=minimizing_=false;shellReady_=false;
            if(launch_.minimized())emit shellRequested();
            emit handoffFailed("Couldn't transfer controls. Release the buttons and try again.");
        }
        else if (event == "request") { if (!view_.requestMenu()) send({{"command", "cancel"}}); }
        else if (event == "released") view_.setInputIsolated(false);
        else if (event == "failed") lost();
        else if (event == "input") {
            view_.updateInput(message["epoch"].toString().toULongLong(), {message["connected"].toBool(),
                message["neutral"].toBool(), message["confirm"].toBool(), message["back"].toBool(),
                message["home"].toBool(), message["up"].toBool(), message["down"].toBool()});
        } else if (event == "captured" || event == "previewed") {
            qCInfo(overlayLog).nospace() << event << " ok=" << message["ok"].toBool()
                << " elapsedMs=" << message["elapsedMs"].toInt(-1)
                << " reason=" << message["reason"].toString().left(64);
            QImage frame;
            if (message["ok"].toBool() && temporary_) {
                QImageReader reader(temporary_->filePath("frame.png"), "png");
                const auto size = reader.size();
                if (size.width() > 0 && size.height() > 0 && size.width() <= 4096 && size.height() <= 4096) frame = reader.read();
            }
            const auto token = message["token"].toVariant().toULongLong();
            if (event == "previewed") view_.menuCaptureCompleted(token, frame);
            else exit_.captureCompleted(token, frame);
        } else if (event == "close-failed") {
            exit_.gracefulExitFailed(message["token"].toVariant().toULongLong(), {});
        }
    }
}
void AdventureOverlayService::minimize() {
    if(!canMinimize() || minimizing_ || !view_.menuOpen() || !view_.ready())return;
    minimizing_=true;handoffDeadline_.start();
    send({{"command","minimize"}});
}
void AdventureOverlayService::returnToGame(bool options) {
    if(!launch_.minimized() || returning_ || !canMinimize())return;
    returning_=true;shellReady_=false;handoffDeadline_.start();
    emit gameRequested();
    send({{"command","return"},{"options",options}});
}
void AdventureOverlayService::shellInput(const QJsonObject& input) {
    if(!input["connected"].toBool()){shellReady_=false;heldAction_.clear();return;}
    if(!shellReady_){if(input["neutral"].toBool())shellReady_=true;return;}
    const QList<QPair<QString,Action>> bindings{{"home",Action::Home},{"back",Action::Back},
        {"start",Action::SystemMenu},{"previousPage",Action::PreviousPage},{"nextPage",Action::NextPage},
        {"previousFace",Action::PreviousFace},{"nextFace",Action::NextFace},
        {"up",Action::Up},{"down",Action::Down},{"left",Action::Left},{"right",Action::Right},
        {"select",Action::LocalAction},{"secondary",Action::Secondary},{"recent",Action::ToggleContinue},
        {"confirm",Action::Confirm}};
    for(const auto& [key,action]:bindings)if(input[key].toBool()) {
        const bool direction=action==Action::Up || action==Action::Down || action==Action::Left || action==Action::Right;
        if(heldAction_!=key){heldAction_=key;repeat_.start();emit shellAction(action);}
        else if(direction && repeat_.elapsed()>350){repeat_.restart();emit shellAction(action);}
        return;
    }
    heldAction_.clear();
}
}
