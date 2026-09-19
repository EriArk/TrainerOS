#include "AdventureOverlayService.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonDocument>

namespace trainer {
AdventureOverlayService::AdventureOverlayService(ProcessService& game, AdventureLaunchController& launch,
        AdventureExitPresentation& view, const QString& helper, QObject* parent)
    : QObject(parent), game_(game), exit_(launch.exitController()), view_(view), helper_(helper) {
    heartbeat_.setInterval(250);
    retry_.setInterval(1000); retry_.setSingleShot(true);
    stopDeadline_.setInterval(1500); stopDeadline_.setSingleShot(true);
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
    active_ = false; retry_.stop(); heartbeat_.stop();
    exit_.setAvailable(false); view_.setInputIsolated(false);
    if (helperProcess_.state() != QProcess::NotRunning) {
        send({{"command", "stop"}}); helperProcess_.closeWriteChannel(); stopDeadline_.start();
    }
}
void AdventureOverlayService::lost() {
    heartbeat_.stop(); exit_.setAvailable(false); view_.setInputIsolated(false);
    if (exit_.phase() == AdventureExitController::Phase::Closing)
        exit_.gracefulExitFailed(attempt_, "The exit connection was lost. Your game has not been forced to stop.");
    if (active_ && game_.active()) retry_.start();
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
        if (event == "ready") exit_.setAvailable(true);
        else if (event == "request") { if (!exit_.requestExit()) send({{"command", "cancel"}}); }
        else if (event == "released") view_.setInputIsolated(false);
        else if (event == "failed") lost();
        else if (event == "input") {
            view_.updateInput(message["epoch"].toString().toULongLong(), {message["connected"].toBool(),
                message["neutral"].toBool(), message["confirm"].toBool(), message["back"].toBool()});
        } else if (event == "captured") {
            QImage frame;
            if (message["ok"].toBool() && temporary_) {
                QImageReader reader(temporary_->filePath("frame.png"), "png");
                const auto size = reader.size();
                if (size.width() > 0 && size.height() > 0 && size.width() <= 4096 && size.height() <= 4096) frame = reader.read();
            }
            exit_.captureCompleted(message["token"].toVariant().toULongLong(), frame);
        } else if (event == "close-failed") {
            exit_.gracefulExitFailed(message["token"].toVariant().toULongLong(), {});
        }
    }
}
}
