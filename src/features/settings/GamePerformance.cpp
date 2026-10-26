#include "GamePerformance.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QSysInfo>
#include <QPointer>

namespace trainer {
namespace {
const QString helper="/var/opt/traineros/integrations/game-performance.py";
QString hash(const QString& s){return QString::fromLatin1(QCryptographicHash::hash(s.toUtf8(),QCryptographicHash::Sha256).toHex());}
QVariantMap row(QString key,QString title,QString value,QString detail,bool enabled=true){return {{"key",key},{"title",title},{"value",value},{"detail",detail},{"enabled",enabled}};}
QString json(const QJsonObject& o){return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));}
struct PowerLease {
    std::unique_ptr<QProcess> process;
    QByteArray output;
    bool ready=false;
    bool accepting=true;
    bool updates=false;
    QJsonObject update(const QJsonObject& config) {
        if(!ready || !accepting || !process || process->state()!=QProcess::Running)
            return {{"error","The power session is unavailable. Changes are saved for the next launch."},{"unavailable",true}};
        const auto packet=json(config).toUtf8()+'\n';
        if(process->write(packet)!=packet.size() || (process->bytesToWrite()>0 && !process->waitForBytesWritten(2000))) {
            accepting=false;process->closeWriteChannel();
            return {{"error","Could not send the profile. Reopen settings before trying again."},{"unavailable",true}};
        }
        QElapsedTimer timer;timer.start();
        while(timer.elapsed()<60000) {
            process->waitForReadyRead(100);output+=process->readAllStandardOutput();
            if(output.contains('\n')) {
                const auto reply=QJsonDocument::fromJson(output.left(output.indexOf('\n'))).object();
                output.remove(0,output.indexOf('\n')+1);
                if(reply.contains("applied") || reply.contains("error"))return reply;
                break;
            }
            if(process->state()==QProcess::NotRunning)break;
        }
        // Never accept a delayed acknowledgement as the next request's reply.
        accepting=false;process->closeWriteChannel();
        return {{"error","Power update was not acknowledged. The host is restoring its settings."},{"unavailable",true}};
    }
    QString start(const QJsonObject& config,const std::atomic_bool& cancel) {
        process=std::make_unique<QProcess>();
        process->start("/usr/bin/python3",{helper,"lease",json(config)});
        if(!process->waitForStarted(4000))return "Power service helper could not start.";
        QElapsedTimer timer;timer.start();
        while(timer.elapsed()<25000 && !cancel) {
            process->waitForReadyRead(100);output+=process->readAllStandardOutput();
            if(output.contains('\n')) {
                const auto reply=QJsonDocument::fromJson(output.left(output.indexOf('\n'))).object();output.remove(0,output.indexOf('\n')+1);
                if(reply["ready"].toBool()){ready=true;updates=reply["updates"].toInt()==1;return {};}
                return reply["error"].toString("Power profile could not be applied.");
            }
            if(process->state()==QProcess::NotRunning)break;
        }
        return cancel?QString("Launch cancelled."):QString("Power service did not acknowledge the profile.");
    }
    QString finish() {
        if(!process)return {};
        process->closeWriteChannel();
        if(process->state()!=QProcess::NotRunning&&!process->waitForFinished(30000)) {
            process->terminate();process->waitForFinished(5000);
        }
        output+=process->readAllStandardOutput();
        QString error;bool restored=false;
        for(const auto& line:output.split('\n')){const auto o=QJsonDocument::fromJson(line).object();if(o.contains("error"))error=o["error"].toString();restored|=o["restored"].toBool();}
        if(ready&&(!restored||process->exitCode()!=0||process->exitStatus()!=QProcess::NormalExit)&&error.isEmpty())error="Host settings need recovery; reopen Performance before playing again.";
        if(process->state()!=QProcess::NotRunning){process->kill();process->waitForFinished(1000);error="Host settings need recovery; reopen Performance before playing again.";}
        process.reset();return error;
    }
};
}
GamePerformance::GamePerformance(LibraryRepository& library,QObject* parent):QObject(parent),library_(library) {
    timeout_.setSingleShot(true);timeout_.setInterval(12000);
    connect(&timeout_,&QTimer::timeout,&probe_,&QProcess::kill);
    connect(&probe_,&QProcess::finished,this,[this](int code,QProcess::ExitStatus){
        timeout_.stop();caps_=QJsonDocument::fromJson(probe_.readAllStandardOutput()).object();
        if(code!=0||caps_.contains("error"))status_="Host power controls unavailable. Saved choices are kept; System default still works.";
        emit changed();
    });
    connect(&probe_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError){timeout_.stop();status_="Host power support is not installed.";emit changed();});
}
void GamePerformance::configure(const QString& directory,const QString& owner){if(directory_==directory&&owner_==owner)return;close();++editRevision_;applying_=false;directory_=directory;owner_=owner;caps_={};}
void GamePerformance::setLiveAvailable(bool available){
    if(liveAvailable_==available)return;
    const bool wasLive=live();liveAvailable_=available;
    if(live()&&!applying_)status_="Apply & save changes power settings now. Compatibility changes require a new launch.";
    if(wasLive&&!available&&!applying_)status_="Live changes are no longer available. The saved profile applies on the next launch.";
    emit changed();
}
QString GamePerformance::path(const QString& game) const{return directory_+"/game-performance/"+hash(owner_)+"/"+hash(game)+".json";}
bool GamePerformance::valid(const QJsonObject& p){
    if(p.isEmpty())return true;
    if(p["version"].toInt()!=1)return false;
    for(const auto& key:{"power","governor"})if(p.contains(key)&&(!p[key].isString()||p[key].toString().size()>64))return false;
    if(p.contains("gpu")&&(!p["gpu"].isDouble()||p["gpu"].toDouble()!=p["gpu"].toInt()||p["gpu"].toInt()<0||p["gpu"].toInt()>10000))return false;
    return QStringList{"auto","vulkan","opengl"}.contains(p["renderer"].toString("auto"))&&QStringList{"auto","off"}.contains(p["sync"].toString("auto"));
}
QJsonObject GamePerformance::snapshot(const QString& game) const {
    if(directory_.isEmpty()||owner_.isEmpty()||game.isEmpty())return {};
    QFile file(path(game));if(!file.open(QIODevice::ReadOnly))return {};
    const auto p=QJsonDocument::fromJson(file.read(8192)).object();return valid(p)?p:QJsonObject{};
}
void GamePerformance::begin(const QString& game){
    const auto r=library_.registration(game);if(!r||r->removed||owner_.isEmpty()||busy())return;
    game_=game;title_=r->adventure.title;pc_=r->adventure.adapterId=="pc-local";proton_=pc_&&r->integrationConfig["runner"]=="proton";
    ++editRevision_;
    draft_=snapshot(game);draft_["version"]=1;page_=focus_=0;status_="Saved for this game. Changes take effect on its next launch.";
    if(QSysInfo::kernelType()=="linux"&&QFileInfo::exists(helper)){probe_.start("/usr/bin/python3",{helper,"probe"});timeout_.start();}
    else status_="Power controls appear on supported Linux hosts. Changes apply on the next launch.";
    if(live())status_="Apply & save changes power settings now. Compatibility changes require a new launch.";
    emit changed();
}
void GamePerformance::close(){game_.clear();emit changed();}
void GamePerformance::selectPage(int p){if(p<0||p>1)return;page_=p;focus_=0;emit changed();}
QVariantList GamePerformance::rows() const {
    if(page_==1){
        if(!pc_)return {row("","Runtime defaults","Managed by emulator","This game's emulator owns graphics and compatibility settings.",false)};
        QVariantList r;
        if(proton_) {
            r<<row("renderer","DirectX 9–11",draft_["renderer"].toString("auto")=="opengl"?"WineD3D · OpenGL":draft_["renderer"]=="vulkan"?"DXVK · Vulkan":"Runtime default","DirectX 12 stays with the selected runtime's VKD3D.");
            r<<row("sync","Esync / Fsync",draft_["sync"]=="off"?"Disabled":"Runtime default","Disable only to diagnose compatibility. NTSync follows the runtime.");
        }
        r<<row("compatibility","Launch & compatibility","Open","Runner versions, game environment, components and launch options.");return r;
    }
    const bool power=!caps_["profiles"].toArray().isEmpty(),gpu=caps_["gpuModes"].toArray().contains("manual"),cpu=!caps_["governors"].toArray().isEmpty();
    const int mhz=draft_["gpu"].toInt();
    return {row("power","Power mode",draft_["power"].toString().isEmpty()?"System default":draft_["power"].toString(),"Host presets balance CPU limits, GPU and cooling.",power),
        row("gpu","GPU clock",mhz?QString::number(mhz)+" MHz":"Automatic",gpu?QString("Host range %1–%2 MHz · rounded by the device").arg(caps_["gpuMin"].toInt()).arg(caps_["gpuMax"].toInt()):"Manual GPU clocks are not exposed by this host.",gpu),
        row("governor","CPU governor",draft_["governor"].toString().isEmpty()?"Follow power mode":draft_["governor"].toString(),cpu?"CPU frequency limits follow the selected host preset.":"CPU controls are not exposed by this host.",cpu)};
}
void GamePerformance::adjust(int index,int direction){
    const auto r=rows();if(index<0||index>=r.size()||!r[index].toMap()["enabled"].toBool()||busy())return;
    focus_=index;const auto key=r[index].toMap()["key"].toString();
    if(key=="compatibility"){const auto game=game_;close();emit compatibilityRequested(game);return;}
    QStringList choices;
    if(key=="power"||key=="governor") {choices<<"";for(const auto& v:caps_[key=="power"?"profiles":"governors"].toArray())choices<<v.toString();}
    else if(key=="renderer")choices={"auto","vulkan","opengl"};
    else if(key=="sync")choices={"auto","off"};
    else if(key=="gpu") {
        QList<int> clocks{0};const int lo=caps_["gpuMin"].toInt(),hi=caps_["gpuMax"].toInt();
        if(lo>0&&hi>=lo&&hi<=10000){clocks<<lo;for(int n=((lo/50)+1)*50;n<hi;n+=50)clocks<<n;if(hi!=lo)clocks<<hi;}
        int current=clocks.indexOf(draft_[key].toInt());if(current<0)current=0;
        draft_[key]=clocks[(current+(direction<0?-1:1)+clocks.size())%clocks.size()];emit changed();return;
    }
    if(choices.isEmpty())return;
    int current=choices.indexOf(draft_[key].toString(choices[0]));if(current<0)current=0;
    draft_[key]=choices[(current+(direction<0?-1:1)+choices.size())%choices.size()];emit changed();
}
void GamePerformance::activate(int index){if(index==rows().size())save();else if(index==rows().size()+1)reset();else adjust(index,1);}
void GamePerformance::setGpuClock(int value){
    if(busy()||!caps_["gpuModes"].toArray().contains("manual"))return;
    const int lo=caps_["gpuMin"].toInt(),hi=caps_["gpuMax"].toInt();
    if(lo<=0||hi<lo||hi>10000)return;
    draft_["gpu"]=qBound(lo,((value+25)/50)*50,hi);focus_=1;emit changed();
}
void GamePerformance::reset(){if(busy())return;draft_={{"version",1}};status_=live()?"Apply & save to restore system settings now and clear this game's overrides.":"System defaults selected. Save to remove this game's overrides.";emit changed();}
void GamePerformance::save(){
    if(busy()||game_.isEmpty()||owner_.isEmpty()||!valid(draft_))return;
    QDir().mkpath(QFileInfo(path(game_)).absolutePath());QSaveFile f(path(game_));const auto bytes=QJsonDocument(draft_).toJson();
    if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit())status_="Could not save this game's profile.";
    else {
        status_="Saved for this game. The next launch uses this profile.";
        if(live()&&applyLive){
            applying_=true;
            const auto revision=editRevision_;const QPointer<GamePerformance> guard(this);
            const bool accepted=applyLive(game_,draft_,[guard,revision](QJsonObject reply){
                if(!guard || guard->editRevision_!=revision)return;
                auto& self=*guard;self.applying_=false;
                if(reply["unavailable"].toBool())self.liveAvailable_=false;
                if(reply["applied"].toBool()) {
                    self.caps_=reply["capabilities"].toObject();
                    self.status_="Power settings applied and profile saved. Compatibility changes take effect on the next launch.";
                } else self.status_="Profile saved. "+reply["error"].toString("Could not apply power settings now.");
                emit self.changed();
            });
            if(!accepted){applying_=false;status_="Profile saved. Live changes are unavailable; it will apply on the next launch.";}
        }
    }
    emit changed();
}
void GamePerformance::dispatch(Action a){
    if(a==Action::Back){close();return;}const int count=rows().size()+2;
    if(a==Action::Down)focus_=(focus_+1)%count;else if(a==Action::Up)focus_=(focus_+count-1)%count;
    else if(a==Action::Left||a==Action::Right){adjust(focus_,a==Action::Left?-1:1);return;}
    else if(a==Action::Confirm){activate(focus_);return;}
    else if(a==Action::PreviousPage||a==Action::NextPage){selectPage(1-page_);return;}
    emit changed();
}
ProcessCommand GamePerformance::prepare(ProcessCommand command,const QString& game) {
    const auto p=snapshot(game);
    const auto r=library_.registration(game);if(!r||r->removed)return command;
    sessionGame_=game;sessionOwner_=owner_;liveAvailable_=false;
    const bool proton=r->adventure.adapterId=="pc-local"&&r->integrationConfig["runner"]=="proton";
    const bool power=!p["power"].toString().isEmpty()||!p["governor"].toString().isEmpty()||p["gpu"].toInt()!=0;
    const auto original=command.prepare;
    command.prepare=[p,original,power,proton](ProcessCommand& c,const std::atomic_bool& cancel)->QString {
        if(original){const auto error=original(c,cancel);if(!error.isEmpty())return error;}
        if(cancel)return {};
        if(proton){
            QStringList env;
            if(p["renderer"]=="opengl")env<<"PROTON_USE_WINED3D=1";
            if(p["renderer"]=="vulkan")env<<"PROTON_USE_WINED3D=0";
            if(p["sync"]=="off")env<<"PROTON_NO_ESYNC=1"<<"PROTON_NO_FSYNC=1";
            if(!env.isEmpty()){env<<c.program;env<<c.arguments;c.program="/usr/bin/env";c.arguments=env;}
        }
        if(QSysInfo::kernelType()!="linux"||!QFileInfo::exists(helper))return power?QString("Game power support is not installed. Select System default to launch without overrides."):QString();
        auto lease=std::make_shared<PowerLease>();const auto final=c.finalize;
        c.finalize=[lease,final](const ProcessOutcome& result){const auto earlier=final?final(result):QString();const auto restored=lease->finish();return earlier.isEmpty()?restored:restored.isEmpty()?earlier:earlier+'\n'+restored;};
        const auto error=lease->start(p,cancel);
        if(error.isEmpty()){
            if(lease->updates)c.updateSettings=[lease](const QJsonObject& config){return lease->update(config);};
        }
        else if(!power){const auto recovery=lease->finish();return recovery;}
        return error;
    };
    return command;
}
}
