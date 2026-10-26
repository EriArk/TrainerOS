#include "GamePerformance.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QSysInfo>

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
    QString start(const QJsonObject& config,const std::atomic_bool& cancel) {
        process=std::make_unique<QProcess>();
        process->start("/usr/bin/python3",{helper,"lease",json(config)});
        if(!process->waitForStarted(4000))return "Power service helper could not start.";
        QElapsedTimer timer;timer.start();
        while(timer.elapsed()<25000 && !cancel) {
            process->waitForReadyRead(100);output+=process->readAllStandardOutput();
            if(output.contains('\n')) {
                const auto reply=QJsonDocument::fromJson(output.left(output.indexOf('\n'))).object();output.remove(0,output.indexOf('\n')+1);
                if(reply["ready"].toBool()){ready=true;return {};}
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
void GamePerformance::configure(const QString& directory,const QString& owner){if(directory_==directory&&owner_==owner)return;close();directory_=directory;owner_=owner;caps_={};}
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
    draft_=snapshot(game);draft_["version"]=1;page_=focus_=0;status_="Saved for this game. Changes take effect on its next launch.";
    if(QSysInfo::kernelType()=="linux"&&QFileInfo::exists(helper)){probe_.start("/usr/bin/python3",{helper,"probe"});timeout_.start();}
    else status_="Power controls appear on supported Linux hosts. Changes apply on the next launch.";
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
void GamePerformance::reset(){draft_={{"version",1}};status_="System defaults selected. Save to remove this game's overrides.";emit changed();}
void GamePerformance::save(){
    if(game_.isEmpty()||owner_.isEmpty()||!valid(draft_))return;
    QDir().mkpath(QFileInfo(path(game_)).absolutePath());QSaveFile f(path(game_));const auto bytes=QJsonDocument(draft_).toJson();
    if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit())status_="Could not save this game's profile.";
    else status_="Saved for this game. The current session is unchanged; the next launch uses this profile.";
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
ProcessCommand GamePerformance::prepare(ProcessCommand command,const QString& game) const {
    const auto p=snapshot(game);if(p.isEmpty())return command;
    const auto r=library_.registration(game);if(!r||r->removed)return command;
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
        if(!power)return {};
        if(QSysInfo::kernelType()!="linux"||!QFileInfo::exists(helper))return "Game power support is not installed. Select System default to launch without overrides.";
        auto lease=std::make_shared<PowerLease>();const auto final=c.finalize;
        c.finalize=[lease,final](const ProcessOutcome& result){const auto earlier=final?final(result):QString();const auto restored=lease->finish();return earlier.isEmpty()?restored:restored.isEmpty()?earlier:earlier+'\n'+restored;};
        return lease->start(p,cancel);
    };
    return command;
}
}
