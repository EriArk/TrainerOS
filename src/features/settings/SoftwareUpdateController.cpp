#include "SoftwareUpdateController.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSettings>
#include <QSaveFile>
#include <QFile>
#include <QUrl>
#include <algorithm>
#include <cmath>

namespace trainer {
namespace {
bool secureUrl(const QString& text) {
    const QUrl url(text);
    return url.isValid() && url.scheme()=="https" && !url.host().isEmpty() && url.userInfo().isEmpty() && !url.hasFragment();
}
QString bytes(qint64 n) { return n>=1024*1024 ? QString::number(double(n)/(1024*1024),'f',1)+" MB" : QString::number(double(n)/1024,'f',1)+" KB"; }
}
SoftwareUpdateController::SoftwareUpdateController(QObject* parent):QObject(parent) {
    QSettings settings;
    channel_=settings.value("softwareUpdates/channel","beta").toString();
    if(channel_!="beta" && channel_!="preview")channel_="beta";
    backgroundChecks_=settings.value("softwareUpdates/backgroundChecks",true).toBool();
    persistTimer_.setSingleShot(true);persistTimer_.setInterval(500);
    connect(&persistTimer_,&QTimer::timeout,this,&SoftwareUpdateController::saveState);
    periodic_.setInterval(6*60*60*1000);
    connect(&periodic_,&QTimer::timeout,this,[this]{if(backgroundChecks_)check(true);});
    killTimer_.setSingleShot(true);killTimer_.setInterval(1500);
    connect(&killTimer_,&QTimer::timeout,this,[this]{if(worker_.state()!=QProcess::NotRunning)worker_.kill();});
    deadline_.setSingleShot(true);
    connect(&deadline_,&QTimer::timeout,this,[this]{fail("The update service stopped responding. Try again.");});
    connect(&worker_,&QProcess::readyReadStandardOutput,this,&SoftwareUpdateController::readOutput);
    connect(&worker_,&QProcess::readyReadStandardError,this,[this]{worker_.readAllStandardError();});
    connect(&worker_,&QProcess::finished,this,&SoftwareUpdateController::finish);
    connect(&worker_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if(error==QProcess::FailedToStart){deadline_.stop();busy_=false;operation_.clear();status_="error";error_="The update service could not start.";publish();}
    });
}
SoftwareUpdateController::~SoftwareUpdateController() {
    saveState();
    worker_.disconnect(this);
    if(worker_.state()!=QProcess::NotRunning){worker_.terminate();if(!worker_.waitForFinished(1500)){worker_.kill();worker_.waitForFinished(1500);}}
}
void SoftwareUpdateController::configure(const SoftwareUpdateSource& source) {
    if(busy_)return;
    periodic_.stop();source_=source;
    configured_=!source.python.isEmpty() && QFileInfo::exists(source.helper) && QFileInfo::exists(source.publicKey)
        && secureUrl(source.betaUrl) && secureUrl(source.previewUrl) && !source.state.isEmpty() && !source.cache.isEmpty()
        && QDir::cleanPath(source.state)!=QDir::cleanPath(source.cache) && !source.installedVersion.isEmpty()
        && source.installedSequence>=0 && source.installedSequence<=9007199254740991LL
        && !source.target.isEmpty() && source.sqliteSchema>=0 && source.firstRunSchema>=0;
    status_=configured_?"idle":"unconfigured";candidate_={};result_={};hasTask_=false;error_.clear();lastChecked_.clear();
    notificationVersion_.clear();notifiedVersion_.clear();phase_.clear();done_=total_=0;notesOpen_=false;
    restoreState();
    if(configured_)periodic_.start();
    publish();
}
QString SoftwareUpdateController::installedVersion() const {return source_.installedVersion.isEmpty()?"Development build":source_.installedVersion;}
QString SoftwareUpdateController::sizeText() const {return candidate_.contains("downloadBytes")?bytes(qint64(candidate_["downloadBytes"].toDouble())):QString();}
double SoftwareUpdateController::progress() const {return total_>0?std::clamp(double(done_)/double(total_),0.0,1.0):-1.0;}
QString SoftwareUpdateController::message() const {
    if(!confirmation_.isEmpty())return confirmation_=="apply"?"Restart TrainerOS and apply the downloaded update? Select Confirm to continue.":confirmation_=="rollback"?"Restart TrainerOS and return to the previous compatible version? Personal files will be kept.":"Restart TrainerOS and repair its installed files? Personal files will be kept.";
    if(!error_.isEmpty())return error_;
    if(!persistenceError_.isEmpty())return persistenceError_;
    if(!operation_.isEmpty())return operation_=="context"?"Reading this installation…":"Checking installation and preparing your request…";
    if(status_=="restart-required")return "Saving your session before restarting TrainerOS…";
    if(status_=="audit-ready")return "Installed files verified. Personal files are unchanged.";
    if(status_=="repairable-drift")return "Some installed files need repair. Open Repair to restore them.";
    if(status_=="cache-cleared")return "Downloaded update files cleared. Installed versions and personal files are unchanged.";
    if(status_=="unconfigured")return "Updates are not available for this build yet.";
    if(status_=="checking")return "Checking for a TrainerOS update…";
    if(status_=="running") {
        if(!stopReason_.isEmpty())return stopReason_=="paused"?"Pausing download…":"Cancelling…";
        if(phase_=="download")return "Downloading · "+bytes(done_)+" / "+bytes(total_);
        if(phase_=="extract")return "Preparing the update…";
        if(phase_=="verify-archive" || phase_=="prepared")return "Verifying the update…";
        return "Checking the update before download…";
    }
    if(status_=="available")return "An update is available. Download when you are ready.";
    if(status_=="up-to-date")return "You have the latest available version for this channel.";
    if(status_=="prepared")return source_.manager.isEmpty()?"Download verified. Applying updates is not available in this build yet.":"Download verified. Apply when you are ready to restart TrainerOS.";
    if(status_=="paused")return "Download paused. Resume when you are ready.";
    if(status_=="cancelled")return "Download cancelled. Your installed version is unchanged.";
    if(status_=="blocked") {
        const auto reason=result_["reason"].toString();
        if(reason=="release-changed")return "The available update has changed. Check again before downloading.";
        if(reason=="updater-required")return "This update needs a newer installer. Your installed version is unchanged.";
        if(reason=="data-incompatible")return "This update is not compatible with your saved settings and data.";
        return "This update is not compatible with this installation.";
    }
    return "Check for a new TrainerOS version. Downloads and installation are manual.";
}
QVariantList SoftwareUpdateController::actions() const {
    const auto row=[](const QString& id,const QString& label,bool enabled){return QVariantMap{{"id",id},{"label",label},{"enabled",enabled}};};
    const bool canDownload=configured_ && !busy_ && (status_=="available" || status_=="paused" || ((status_=="error" || status_=="cancelled") && !candidate_.isEmpty()));
    const bool canPause=busy_ && preparing_ && phase_=="download" && stopReason_.isEmpty();
    return {row("channel",channel_=="beta"?"Channel: Beta":"Channel: Preview · experimental",!busy_),
        row("background",backgroundChecks_?"Background checks: On":"Background checks: Off",!busy_),
        row("check","Check for updates",configured_ && !busy_ && status_!="paused"),
        row("download",canPause?"Pause download":status_=="paused"?"Resume download":"Download update",canPause || canDownload),
        row("cancel","Cancel",operation_.isEmpty() && (busy_ || status_=="paused") && stopReason_.isEmpty()),
        row("notes","What's new",!notes().isEmpty()),row("downloads","Downloads",true),
        row("crates","View Crates",!transitionPending()),
        row("apply",confirmation_=="apply"?"Confirm apply and restart":"Apply and restart TrainerOS",!busy_ && !source_.manager.isEmpty() && status_=="prepared"),
        row("rollback",confirmation_=="rollback"?"Confirm rollback and restart":"Previous version",!busy_ && source_.rollbackAvailable),
        row("repair",confirmation_=="repair"?"Confirm repair and restart":"Repair TrainerOS",!busy_ && !source_.manager.isEmpty()),
        row("audit","Check installed files",!busy_ && !source_.manager.isEmpty()),
        row("clean-cache","Clear download cache",!busy_ && !source_.manager.isEmpty()),
        row("back",confirmation_.isEmpty()?"Back to System":"Cancel",!transitionPending())};
}
QVariantList SoftwareUpdateController::tasks() const {
    if(!hasTask_)return {};
    QVariantList commands;
    const auto add=[&](const QString& id,const QString& label){commands<<QVariantMap{{"id",id},{"label",label}};};
    if(busy_ && preparing_ && phase_=="download" && stopReason_.isEmpty())add("pause","Pause");
    if(!busy_ && (status_=="paused" || status_=="error" || status_=="cancelled"))add("resume","Retry / resume");
    if(operation_.isEmpty() && (busy_ || status_=="paused") && stopReason_.isEmpty())add("cancel","Cancel");
    add("open","View update");
    const QString state=busy_?"running":status_=="prepared"?"done":status_=="error" || status_=="blocked"?"failed":status_;
    QVariantMap row{{"id","traineros"},{"title","TrainerOS "+version()},{"platform","Software"},{"source","TrainerOS"},
        {"state",state},{"detail",message()},{"terminal",!busy_ && status_!="paused"},{"actions",commands}};
    if(progress()>=0)row["progress"]=progress();
    return {row};
}
void SoftwareUpdateController::publish(){focus_=std::clamp(focus_,0,int(actions().size())-1);if(configured_)persistTimer_.start();emit changed();}
QVariantMap SoftwareUpdateController::notification() const {
    if(notificationVersion_.isEmpty())return {};
    return {{"id","traineros-software-update"},{"provider","traineros-software"},{"name","TrainerOS update available"},
        {"detail",notificationVersion_+" · Open Settings to review"}};
}
void SoftwareUpdateController::dismissNotification(){notificationVersion_.clear();emit changed();}
void SoftwareUpdateController::check(bool background) {
    if(!configured_ || busy_ || transitionPending() || status_=="paused" || (background && (!backgroundChecks_ || hasTask_)))return;
    start(false,background);
}
void SoftwareUpdateController::start(bool prepare,bool background) {
    if(!configured_ || busy_)return;
    QStringList args{source_.helper,prepare?"prepare":"check","--feed-url",channel_=="beta"?source_.betaUrl:source_.previewUrl,
        "--key",source_.publicKey,"--channel",channel_,"--state",source_.state,"--installed-sequence",QString::number(source_.installedSequence),
        "--target",source_.target,"--sqlite",QString::number(source_.sqliteSchema),"--first-run",QString::number(source_.firstRunSchema),
        "--minimum-feed-sequence",QString::number(source_.minimumFeedSequence),"--minimum-release-sequence",QString::number(source_.minimumReleaseSequence)};
    if(prepare)args<<"--cache"<<source_.cache<<"--expected-sequence"<<QString::number(qint64(candidate_["sequence"].toDouble()));
    if(!source_.manager.isEmpty()) {
        args={source_.manager,"--root",source_.installationRoot,prepare?"prepare":"check","--channel",channel_};
        if(prepare)args<<"--sequence"<<QString::number(qint64(candidate_["sequence"].toDouble()));
    }
    output_.clear();result_={};error_.clear();stopReason_.clear();phase_.clear();done_=total_=0;protocolError_=false;
    preparing_=prepare;background_=background;busy_=true;status_=prepare?"running":"checking";
    if(prepare)hasTask_=true;else {candidate_={};hasTask_=false;}
    deadline_.start(prepare?120000:90000);
    saveState();worker_.start(source_.python,args);publish();
}
void SoftwareUpdateController::fail(const QString& message) {
    error_=message;protocolError_=true;
    if(worker_.state()!=QProcess::NotRunning){worker_.terminate();killTimer_.start();}
}
void SoftwareUpdateController::readOutput() {
    output_+=worker_.readAllStandardOutput();
    if(output_.size()>128*1024){output_.clear();fail("The update service returned an invalid response.");return;}
    while(output_.contains('\n')) {
        const auto line=output_.left(output_.indexOf('\n'));output_.remove(0,line.size()+1);
        if(line.trimmed().isEmpty())continue;
        QJsonParseError parse;const auto document=QJsonDocument::fromJson(line,&parse);
        if(parse.error!=QJsonParseError::NoError || !document.isObject()){fail("The update service returned an invalid response.");return;}
        const auto value=document.object();
        if(value.contains("phase")) {
            if(!preparing_ || !result_.isEmpty()){fail("Unexpected update progress.");return;}
            phase_=value["phase"].toString();done_=qint64(value["bytes"].toDouble());total_=qint64(value["totalBytes"].toDouble());
            if(!QStringList{"download","verify-archive","extract","prepared"}.contains(phase_) || done_<0 || total_<0 || done_>total_){fail("Invalid update progress.");return;}
            // Verification can legitimately read a large archive/cache on slow
            // storage without byte events. Only network inactivity gets 2 min.
            deadline_.start(phase_=="download"?120000:30*60*1000);publish();
        } else {
            if(!result_.isEmpty()){fail("The update service returned multiple results.");return;}
            result_=value;
        }
    }
}
void SoftwareUpdateController::finish(int code,QProcess::ExitStatus exitStatus) {
    readOutput();killTimer_.stop();deadline_.stop();busy_=false;
    if(!operation_.isEmpty()) {
        const auto operation=operation_;operation_.clear();
        if(code!=0 || exitStatus!=QProcess::NormalExit || protocolError_ || !output_.trimmed().isEmpty() || result_["status"].toString().isEmpty() || result_["status"]=="error") {
            status_="error";if(error_.isEmpty())error_="The installation operation could not finish. Your personal files are unchanged. Retry or use Setup and Recovery.";publish();return;
        }
        if(operation=="context") {
            if(result_["status"]!="context" || !result_["supported"].toBool()
                || QFileInfo(result_["dataDirectory"].toString()).absoluteFilePath()!=QFileInfo(expectedData_).absoluteFilePath()
                || QFileInfo(result_["executable"].toString()).canonicalFilePath()!=QFileInfo(expectedExecutable_).canonicalFilePath()
                || result_["installedVersion"].toString().isEmpty()) {
                status_="unconfigured";error_="This running build is not a verified managed installation.";publish();return;
            }
            auto source=source_;source.helper=QFileInfo(source.manager).dir().filePath("channels.py");
            source.publicKey=result_["publicKey"].toString();source.betaUrl=result_["betaUrl"].toString();source.previewUrl=result_["previewUrl"].toString();
            source.state=result_["state"].toString();source.cache=result_["cache"].toString();source.installedVersion=result_["installedVersion"].toString();
            source.installedSequence=qint64(result_["installedSequence"].toDouble(-1));source.target=result_["target"].toString();
            source.sqliteSchema=result_["sqliteSchema"].toInt(-1);source.firstRunSchema=result_["firstRunSchema"].toInt(-1);source.rollbackAvailable=result_["rollbackAvailable"].toBool();
            configure(source);return;
        }
        if(operation=="request" && result_["status"]=="restart-required") {
            const auto blocked=maintenanceGuard?maintenanceGuard():QString("Managed restart is not available.");
            if(!blocked.isEmpty()){status_="error";error_=blocked;publish();return;}
            status_="restart-required";saveState();publish();emit restartRequested();return;
        }
        if(operation=="audit" && (result_["status"]=="ready" || result_["status"]=="repairable-drift")) {
            status_=result_["status"]=="ready"?"audit-ready":"repairable-drift";
        } else if(operation=="clean-cache" && result_["status"]=="cache-cleared") {status_="cache-cleared";hasTask_=false;candidate_={};done_=total_=0;}
        else {status_="error";error_="Unexpected installation response. No successful operation was confirmed.";}
        publish();return;
    }
    if(!stopReason_.isEmpty()){status_=stopReason_;stopReason_.clear();error_.clear();publish();return;}
    const QString received=result_["status"].toString();
    const bool outcome=(code==0 && QStringList{"available","up-to-date","prepared"}.contains(received)) || (code==2 && received=="blocked");
    const bool identity=result_["channel"].toString()==channel_ && !result_["version"].toString().isEmpty()
        && result_["installed"].isBool() && !result_["installed"].toBool();
    if(protocolError_ || !output_.trimmed().isEmpty() || exitStatus!=QProcess::NormalExit || !outcome || !identity
        || (!preparing_ && received=="prepared") || (preparing_ && received=="available")
        || (received=="prepared" && result_["prepared"].toString().isEmpty())) {
        status_="error";
        if(error_.isEmpty()) {
            const auto detail=result_["error"].toString();
            if(detail.contains("Not enough space"))error_="There is not enough free space to prepare this update. Free some space and retry.";
            else if(detail.contains("expired") || detail.contains("not yet valid"))error_="Update information has expired or your system clock is incorrect. Check the time and try again.";
            else if(detail.contains("signature") || detail.contains("hash") || detail.contains("differs from signed"))error_="The update could not be verified. Your installed version is unchanged.";
            else if(detail.contains("Another release preparation"))error_="Another update operation is running. Try again when it finishes.";
            else error_="Could not check or prepare the update. Check your connection and try again.";
        }
        publish();return;
    }
    status_=received;candidate_=result_;lastChecked_=QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");
    if(status_=="available" && background_ && notifiedVersion_!=version()){notifiedVersion_=notificationVersion_=version();emit updateAvailable(version());}
    publish();
}
void SoftwareUpdateController::stop(const QString& reason) {
    if(!busy_){if(status_=="paused" && reason=="cancelled"){status_=reason;publish();}return;}
    if(!stopReason_.isEmpty())return;
    stopReason_=reason;worker_.terminate();killTimer_.start();publish();
}
void SoftwareUpdateController::command(const QString& command) {
    if(command=="pause" || command=="pause-all") {if(busy_ && preparing_ && phase_=="download")stop("paused");}
    else if(command=="cancel" || command=="cancel-all") {if(operation_.isEmpty())stop("cancelled");}
    else if(command=="resume" || command=="resume-all") {
        if(configured_ && !busy_ && (status_=="paused" || status_=="error" || status_=="cancelled") && !candidate_.isEmpty())start(true);
    }
}
void SoftwareUpdateController::activate(int index) {
    if(transitionPending())return;
    const auto rows=actions();if(index<0 || index>=rows.size())return;
    focus_=index;const auto row=rows[index].toMap();if(!row["enabled"].toBool()){publish();return;}
    const auto id=row["id"].toString();
    if(id=="apply" || id=="rollback" || id=="repair") {
        if(confirmation_!=id){confirmation_=id;publish();return;}
        confirmation_.clear();maintenance(id);return;
    }
    if(id=="audit" || id=="clean-cache"){confirmation_.clear();maintenance(id);return;}
    if(id=="back" && !confirmation_.isEmpty()){confirmation_.clear();publish();return;}
    confirmation_.clear();
    if(id=="channel") {channel_=channel_=="beta"?"preview":"beta";candidate_={};result_={};hasTask_=false;notesOpen_=false;error_.clear();lastChecked_.clear();notificationVersion_.clear();status_=configured_?"idle":"unconfigured";QSettings().setValue("softwareUpdates/channel",channel_);}
    else if(id=="background"){backgroundChecks_=!backgroundChecks_;QSettings().setValue("softwareUpdates/backgroundChecks",backgroundChecks_);}
    else if(id=="check")check();
    else if(id=="download"){if(busy_)command("pause");else start(true);}
    else if(id=="cancel")command("cancel");
    else if(id=="notes")notesOpen_=true;
    else if(id=="downloads")emit downloadsRequested();
    else if(id=="crates")emit cratesRequested();
    else if(id=="back")emit backRequested();
    publish();
}
void SoftwareUpdateController::closeNotes(){notesOpen_=false;publish();}
void SoftwareUpdateController::dispatch(Action action) {
    if(notesOpen_){if(action==Action::Back || action==Action::Confirm)closeNotes();else if(action==Action::Up || action==Action::Down)emit notesScrollRequested(action==Action::Up?-1:1);return;}
    if(action==Action::Back){if(transitionPending())return;if(!confirmation_.isEmpty()){confirmation_.clear();publish();}else emit backRequested();return;}
    if(action==Action::Up || action==Action::Down)focus_=std::clamp(focus_+(action==Action::Up?-1:1),0,int(actions().size())-1);
    if(action==Action::Confirm){activate(focus_);return;}
    publish();
}

void SoftwareUpdateController::cancelRestart() {
    if(status_!="restart-required")return;
    status_="error";error_="Restart cancelled. Review the update again when you are ready.";publish();
}
void SoftwareUpdateController::attachInstallation(const QString& root,const QString& dataDirectory,const QString& executable) {
    if(busy_ || root.isEmpty())return;
    source_.installationRoot=QDir(root).absolutePath();source_.manager=QDir(root).filePath("bootstrap/packaging/software/installation.py");
    source_.python="/usr/bin/python3";expectedData_=dataDirectory;expectedExecutable_=executable;
    if(!QFileInfo::exists(source_.manager))return;
    operation_="context";busy_=true;output_.clear();result_={};protocolError_=false;error_.clear();deadline_.start(90000);
    worker_.start(source_.python,{source_.manager,"--root",source_.installationRoot,"context"});publish();
}
void SoftwareUpdateController::maintenance(const QString& command) {
    if(busy_ || source_.manager.isEmpty())return;
    if(command=="apply" || command=="rollback" || command=="repair") {
        const auto blocked=maintenanceGuard?maintenanceGuard():QString("Managed restart is not available.");
        if(!blocked.isEmpty()){error_=blocked;publish();return;}
    }
    operation_=(command=="audit" || command=="clean-cache")?command:"request";
    QStringList args{source_.manager,"--root",source_.installationRoot,operation_};
    if(operation_=="request")args<<"--operation"<<command<<"--channel"<<channel_;
    if(command=="apply")args<<"--sequence"<<QString::number(qint64(candidate_["sequence"].toDouble()));
    busy_=true;output_.clear();result_={};error_.clear();protocolError_=false;stopReason_.clear();preparing_=false;
    deadline_.start(command=="audit"?30*60*1000:120000);worker_.start(source_.python,args);publish();
}
void SoftwareUpdateController::saveState() {
    if(!configured_ || source_.state.isEmpty())return;
    QSaveFile file(QDir(source_.state).filePath("ui-job.json"));
    const auto bytes=QJsonDocument(QJsonObject{{"schema",1},{"channel",channel_},{"feed",channel_=="beta"?source_.betaUrl:source_.previewUrl},
        {"candidate",candidate_},{"lastChecked",lastChecked_},{"hasTask",hasTask_},{"status",status_}}).toJson(QJsonDocument::Compact);
    if(!QDir().mkpath(source_.state) || !file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit())
        persistenceError_="Download state could not be saved. Free some space; a restart may require checking again.";
    else persistenceError_.clear();
}
void SoftwareUpdateController::restoreState() {
    if(!configured_)return;
    QFile file(QDir(source_.state).filePath("ui-job.json"));if(!file.exists())return;
    if(!file.open(QIODevice::ReadOnly) || file.size()>128*1024){persistenceError_="Previous download state could not be read. Check for updates again.";return;}
    const auto doc=QJsonDocument::fromJson(file.readAll());const auto saved=doc.object();
    if(saved["schema"]!=1 || saved["channel"].toString()!=channel_ || saved["feed"].toString()!=(channel_=="beta"?source_.betaUrl:source_.previewUrl))return;
    lastChecked_=saved["lastChecked"].toString();
    const auto candidate=saved["candidate"].toObject();
    if(saved["hasTask"].toBool() && candidate["sequence"].toDouble()>source_.installedSequence && candidate["installed"]==false) {
        candidate_=candidate;hasTask_=true;status_=saved["status"]=="cancelled"?"cancelled":"paused";
        // A restored job is never trusted as verified/installed. Explicit Resume
        // rechecks the signed feed and payload cache before showing Prepared.
    }
}
}
