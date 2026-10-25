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
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <QStorageInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QRegularExpression>

namespace trainer {
namespace {
QByteArray tarEntry(const QByteArray& name,const QByteArray& data) {
    QByteArray header(512,'\0');
    const auto put=[&](int offset,const QByteArray& bytes){std::copy(bytes.begin(),bytes.end(),header.begin()+offset);};
    put(0,name.left(99));put(100,"0000600");put(108,"0000000");put(116,"0000000");
    put(124,QByteArray::number(data.size(),8).rightJustified(11,'0'));put(136,"00000000000");
    put(148,"        ");header[156]='0';put(257,"ustar");put(263,"00");
    unsigned sum=0;for(unsigned char c:header)sum+=c;put(148,QByteArray::number(sum,8).rightJustified(6,'0'));header[154]='\0';header[155]=' ';
    return header+data+QByteArray((512-data.size()%512)%512,'\0');
}
}
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
void DiagnosticsService::saveSupport(const QJsonObject& health) {
    if(saving_)return;saving_=true;
    QJsonArray systems;
    const QRegularExpression id("^[a-zA-Z0-9_-]{1,64}$");
    for(const auto& value:health["systems"].toArray()) {
        const auto row=value.toObject();if(!id.match(row["platform"].toString()).hasMatch()||!id.match(row["runtime"].toString()).hasMatch())continue;
        const QStringList states{"Needs attention","Needs verification","Setup checked"};
        systems.append(QJsonObject{{"platform",row["platform"]},{"runtime",row["runtime"]},{"state",states.contains(row["state"].toString())?row["state"].toString():QString("Unknown")},
            {"games",std::clamp(row["games"].toInt(),0,100000)},{"verified",std::clamp(row["verified"].toInt(),0,100000)}});
        if(systems.size()>=256)break;
    }
    const auto now=QDateTime::currentDateTimeUtc();
    const auto filename="traineros-support-"+now.toString("yyyyMMdd-HHmmss")+"-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".tar";
    auto report=QJsonObject{{"schemaVersion",1},{"applicationVersion",QCoreApplication::applicationVersion()},{"createdAtUtc",now.toString(Qt::ISODate)},
        {"runtime",runtime()},{"systems",systems},{"controllerConnected",health["controllerConnected"].toBool()}};
    QMetaObject::invokeMethod(worker_,[this,report,filename]() mutable {
        const auto base=QFileInfo(directory_).absolutePath();QStorageInfo storage(base);
        report["storage"]=QJsonObject{{"ready",storage.isReady()},{"freeBytes",double(storage.bytesAvailable())},{"totalBytes",double(storage.bytesTotal())}};
        const auto connection="support-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
        QString integrity="Unavailable";int schema=0;
        if(QFileInfo::exists(QDir(base).filePath("traineros.sqlite3"))){
            {auto db=QSqlDatabase::addDatabase("QSQLITE",connection);db.setDatabaseName(QDir(base).filePath("traineros.sqlite3"));db.setConnectOptions("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=1000");
                if(db.open()){QSqlQuery q(db);if(q.exec("PRAGMA quick_check(1)")&&q.next())integrity=q.value(0).toString()=="ok"?"ok":"Failed";
                    if(q.exec("PRAGMA user_version")&&q.next())schema=q.value(0).toInt();db.close();}}
            QSqlDatabase::removeDatabase(connection);
        }
        report["database"]=QJsonObject{{"integrity",integrity},{"schema",schema}};
        const auto json=QJsonDocument(report).toJson();
        const auto payload=tarEntry("diagnostics.json",json)+tarEntry("README.txt","TrainerOS support snapshot. No ROMs, BIOS, saves, account names, history, tokens, private paths or raw logs are included. Setup checks do not certify gameplay or physical hardware.\n")+QByteArray(1024,'\0');
        QString error;const auto path=QDir(directory_).filePath(filename);
        if(json.size()>128*1024||directory_.isEmpty()||!QDir::isAbsolutePath(directory_)||!QDir().mkpath(directory_))error="Support report folder is unavailable.";
        else {QSaveFile file(path);file.setDirectWriteFallback(false);if(!file.open(QIODevice::WriteOnly))error="Could not create the report.";
            else {file.setPermissions(QFile::ReadOwner|QFile::WriteOwner);if(file.write(payload)!=payload.size()||!file.commit())error="Could not save the complete report.";}}
        QMetaObject::invokeMethod(this,[this,error,path,filename]{saving_=false;if(error.isEmpty())lastReportPath_=path;emit completed(error,filename);},Qt::QueuedConnection);
    },Qt::QueuedConnection);
}
}
