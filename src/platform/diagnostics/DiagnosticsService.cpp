#include "DiagnosticsService.h"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QSysInfo>
#include <QDateTime>
#include <QDir>
#include <QSaveFile>
#include <QJsonDocument>
#include <QUuid>
#include <SDL.h>

namespace trainer {
DiagnosticsService::DiagnosticsService(QString directory, QObject* parent)
    : QObject(parent), directory_(std::move(directory)), worker_(new QObject) {
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
    thread_.start();
}
DiagnosticsService::~DiagnosticsService() {
    // Drain an explicitly requested report before stopping the worker on app teardown.
    QMetaObject::invokeMethod(worker_, [] {}, Qt::BlockingQueuedConnection);
    thread_.quit(); thread_.wait();
}
void DiagnosticsService::setWindow(QWindow* window) {
    if (window_) disconnect(window_, nullptr, this, nullptr);
    window_ = window;
    if (window_) {
        connect(window_, &QWindow::widthChanged, this, &DiagnosticsService::runtimeChanged);
        connect(window_, &QWindow::heightChanged, this, &DiagnosticsService::runtimeChanged);
        connect(window_, &QWindow::visibilityChanged, this, &DiagnosticsService::runtimeChanged);
        connect(window_, &QWindow::activeChanged, this, &DiagnosticsService::runtimeChanged);
        connect(window_, &QWindow::screenChanged, this, &DiagnosticsService::watchScreen);
    }
    watchScreen(window_ ? window_->screen() : nullptr);
}
void DiagnosticsService::watchScreen(QScreen* screen) {
    if (screen_) disconnect(screen_, nullptr, this, nullptr);
    screen_ = screen;
    if (screen_) {
        connect(screen_, &QScreen::geometryChanged, this, &DiagnosticsService::runtimeChanged);
        connect(screen_, &QScreen::logicalDotsPerInchChanged, this, &DiagnosticsService::runtimeChanged);
        connect(screen_, &QScreen::physicalDotsPerInchChanged, this, &DiagnosticsService::runtimeChanged);
        connect(screen_, &QScreen::refreshRateChanged, this, &DiagnosticsService::runtimeChanged);
    }
    emit runtimeChanged();
}
QJsonObject DiagnosticsService::runtime() const {
    SDL_version version; SDL_GetVersion(&version);
    QJsonObject result{{"os", QSysInfo::productType()}, {"osVersion", QSysInfo::productVersion()},
        {"kernel", QSysInfo::kernelType()}, {"kernelVersion", QSysInfo::kernelVersion()},
        {"cpuArchitecture", QSysInfo::currentCpuArchitecture()}, {"buildArchitecture", QSysInfo::buildCpuArchitecture()},
        {"qtVersion", qVersion()}, {"sdlVersion", QString("%1.%2.%3").arg(version.major).arg(version.minor).arg(version.patch)},
        {"qpaPlatform", QGuiApplication::platformName()}};
    if (window_) {
        result["window"] = QJsonObject{{"width", window_->width()}, {"height", window_->height()},
            {"devicePixelRatio", window_->devicePixelRatio()}, {"visible", window_->isVisible()},
            {"active", window_->isActive()}, {"fullscreen", window_->visibility() == QWindow::FullScreen}};
        if (auto* screen = window_->screen()) {
            const auto geometry = screen->geometry();
            result["qtReportedScreen"] = QJsonObject{{"width", geometry.width()}, {"height", geometry.height()},
                {"devicePixelRatio", screen->devicePixelRatio()}, {"refreshRate", screen->refreshRate()},
                {"logicalDpi", screen->logicalDotsPerInch()}};
        }
    }
    return result;
}
void DiagnosticsService::save(const QJsonObject& observations) {
    if (saving_) return;
    saving_ = true;
    const auto now = QDateTime::currentDateTimeUtc();
    const auto filename = "device-check-" + now.toString("yyyyMMdd-HHmmss") + "-" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".json";
    const auto payload = QJsonDocument(QJsonObject{{"schemaVersion", 1}, {"createdAtUtc", now.toString(Qt::ISODateWithMs)},
        {"applicationVersion", QCoreApplication::applicationVersion()}, {"runtime", runtime()}, {"input", observations},
        {"scope", "Software observations only; physical labels, display mode, comfort and ArmadaOS session behavior require device validation."}}).toJson();
    QMetaObject::invokeMethod(worker_, [this, filename, payload] {
        QString error;
        const auto path = QDir(directory_).filePath(filename);
        if (directory_.isEmpty() || !QDir::isAbsolutePath(directory_) || !QDir().mkpath(directory_))
            error = "The report folder isn't available. Check storage access and retry.";
        else {
            QSaveFile file(path);
            file.setDirectWriteFallback(false);
            if (!file.open(QIODevice::WriteOnly) || file.write(payload) != payload.size() || !file.commit())
                error = "The report couldn't be saved. Check storage access or free space and retry.";
        }
        QMetaObject::invokeMethod(this, [this, error, filename, path] {
            saving_ = false;
            if (error.isEmpty()) lastReportPath_ = path;
            emit completed(error, filename);
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
}
