#include "AchievementCelebration.h"
#include <QDir>
#include <QImageReader>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QPointer>
#include <QSettings>
#include <QLoggingCategory>
#include <QtEndian>
#include <cmath>
#include <algorithm>

namespace trainer {
Q_LOGGING_CATEGORY(achievementLiveLog,"trainer.achievements")
namespace {
QImage nativeCapture(const QString& directory,const QString& id) {
    // Fresh, owner-only per-launch directory, never the user's screenshot folder.
    const QFileInfo root(directory);
    if(!root.isDir() || root.isSymLink())return {};
    QDir dir(directory);
    const auto files=dir.entryInfoList({"*-cheevo-"+id+".png"},QDir::Files|QDir::NoSymLinks);
    if(files.size()!=1)return {};
    const auto& file=files.first();
    if(file.canonicalPath()!=root.canonicalFilePath() || file.size()<8 || file.size()>16*1024*1024)return {};
    QFile input(file.absoluteFilePath());if(!input.open(QIODevice::ReadOnly))return {};
    const auto bytes=input.read(16*1024*1024+1);if(bytes.size()>16*1024*1024)return {};
    QBuffer buffer;buffer.setData(bytes);buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer,"PNG");const auto size=reader.size();
    if(size.width()<1 || size.height()<1 || size.width()>8192 || size.height()>8192
        || qint64(size.width())*size.height()>16*1024*1024)return {};
    reader.setScaledSize(size.scaled(960,540,Qt::KeepAspectRatio));
    const auto frame=reader.read();input.close();
    if(!frame.isNull())QFile::remove(file.absoluteFilePath());
    return frame;
}
}
AchievementCelebration::AchievementCelebration(LocalStateStore* store,AchievementProvider& provider,QObject* parent)
    :QObject(parent),store_(store),provider_(provider),worker_(new QObject) {
    worker_->moveToThread(&thread_);connect(&thread_,&QThread::finished,worker_,&QObject::deleteLater);thread_.start();
    duration_.setSingleShot(true);
    connect(&duration_,&QTimer::timeout,this,[this]{current_.clear();emit changed();QTimer::singleShot(250,this,&AchievementCelebration::next);});
    connect(&provider_,&AchievementProvider::snapshotChanged,this,[this]{if(!session_.isEmpty()&&!valid())end();else resolve();});
}
AchievementCelebration::~AchievementCelebration(){end();thread_.quit();thread_.wait();}
bool AchievementCelebration::valid() const {
    return store_ && store_->ready() && store_->ownerId()==owner_ && !session_.isEmpty()
        && provider_.context().providerId=="retroAchievements"
        && provider_.context().accountId.compare(account_,Qt::CaseInsensitive)==0;
}
void AchievementCelebration::begin(const QString& adventure,qint64 pid,const QVariantMap& controls) {
    end();if(!store_ || !store_->ready() || pid<=0 || controls.isEmpty())return;
    owner_=store_->ownerId();account_=controls["account"].toString();
    session_=controls["session"].toString()+":"+QString::number(pid);
    adventure_=adventure;directory_=controls["directory"].toString();
    if(!valid() || account_.isEmpty() || directory_.isEmpty() || !store_->registration(adventure))end();
    else qCInfo(achievementLiveLog)<<"owned runtime attached";
}
void AchievementCelebration::end() {
    ++generation_;duration_.stop();if(sink_)sink_->stop();audio_.close();
    session_.clear();parser_.clear();pending_.clear();seen_.clear();queue_.clear();current_.clear();
    game_.clear();hash_.clear();mode_.clear();loaded_=false;emit changed();
}
void AchievementCelebration::finish() {
    QStringList missed;
    if(valid())for(const auto& award:queue_)if(!award.value("played",false).toBool())missed.append(award["title"].toString());
    end();
    if(!missed.isEmpty())emit notice("Achievements earned: "+missed.mid(0,3).join(" · ")+(missed.size()>3?"…":""));
}
void AchievementCelebration::feed(const QByteArray& bytes) {
    if(!valid())return;
    for(const auto& event:parser_.feed(bytes)) {
        // rc_client starts in Hardcore and applies the launch preference before
        // loading a game. Only a change after admission invalidates this session.
        if(event.kind==RuntimeAchievementEvent::ModeChanged){if(loaded_){end();return;}continue;}
        if(event.kind==RuntimeAchievementEvent::Invalidated){end();return;}
        if(event.kind==RuntimeAchievementEvent::Identified) {
            if(!game_.isEmpty() && (game_!=event.game || hash_!=event.hash)){end();return;}
            game_=event.game;hash_=event.hash;
            qCInfo(achievementLiveLog)<<"runtime identified set"<<game_;
            provider_.observeRuntimeGame(adventure_,hash_,game_);continue;
        }
        if(event.kind==RuntimeAchievementEvent::Loaded) {
            loaded_=event.game==game_ && !hash_.isEmpty();mode_=event.mode;continue;
        }
        if(!loaded_ || event.id.isEmpty())continue;
        if(event.kind==RuntimeAchievementEvent::Awarded) {
            if(seen_.contains(event.id) || seen_.size()>=256)continue;
            seen_.insert(event.id);
            AchievementMoment m; m.owner=owner_;m.account=account_;m.set=game_;m.achievement=event.id;m.mode=mode_;
            m.adventure=adventure_;m.session=session_;m.contentHash=hash_;m.observedAt=QDateTime::currentDateTimeUtc();m.title=event.title;
            pending_[event.id].moment=m;
            const auto generation=generation_;
            QTimer::singleShot(1800,this,[this,id=event.id,generation]{
                if(generation==generation_ && pending_.contains(id)){pending_[id].failed=true;resolve();}
            });
        } else if(event.kind==RuntimeAchievementEvent::Captured)pending_[event.id].captured=true;
        else if(event.kind==RuntimeAchievementEvent::CaptureFailed)pending_[event.id].failed=true;
        // Capture callback can precede the award log. Keep only a bounded pair.
        if(pending_.size()>64){pending_.remove(event.id);continue;}
        resolve();
    }
}
void AchievementCelebration::resolve() {
    if(!valid() || !loaded_)return;
    bool matched=false;
    for(const auto& set:provider_.sets())if(set.id==game_ && set.gameId==game_ && set.adventureId==adventure_)matched=true;
    if(!matched)return;
    const auto snapshot=provider_.snapshot(game_);
    if(snapshot.context!=provider_.context() || snapshot.setId!=game_)return;
    const auto ids=pending_.keys();
    for(const auto& id:ids) {
        auto& p=pending_[id];if(p.reading || p.moment.achievement.isEmpty() || (!p.captured&&!p.failed))continue;
        for(const auto& definition:snapshot.definitions)if(definition.id==id) {
            p.moment.title=definition.title;p.moment.badge=definition.badgeName;
            collect(id);break;
        }
    }
}
void AchievementCelebration::collect(const QString& id) {
    auto& p=pending_[id];p.reading=true;const auto generation=generation_;
    const bool capture=p.captured;const auto directory=directory_;auto moment=p.moment;
    QPointer<AchievementCelebration> guard(this);
    QMetaObject::invokeMethod(worker_,[guard,generation,capture,directory,moment]() mutable {
        if(capture)for(int attempt=0;attempt<15;++attempt) {
            moment.frame=nativeCapture(directory,moment.achievement);
            if(!moment.frame.isNull()){moment.capturedAt=moment.observedAt;break;}
            QThread::msleep(80);
        }
        if(!guard)return;
        QMetaObject::invokeMethod(guard,[guard,generation,moment] {
            if(!guard || generation!=guard->generation_ || !guard->valid() || !guard->loaded_)return;
            guard->pending_.remove(moment.achievement);
            guard->store_->saveAchievementMomentAsync(moment,guard,[guard,generation,moment](bool inserted,QString error){
                if(!guard || generation!=guard->generation_ || !guard->valid())return;
                if(!error.isEmpty()){emit guard->notice(error);return;}
                if(!inserted)return;
                qCInfo(achievementLiveLog)<<"moment committed"<<moment.set<<moment.achievement<<"photo"<<!moment.frame.isNull();
                int points=-1;
                for(const auto& d:guard->provider_.snapshot(moment.set).definitions)if(d.id==moment.achievement)points=d.points;
                if(guard->queue_.size()<16)guard->queue_.enqueue(QVariantMap{{"title",moment.title},{"mode",moment.mode},
                    {"points",points},{"set",moment.set},{"id",moment.achievement},
                    {"icon",moment.badge.isEmpty()?QString():"https://media.retroachievements.org/Badge/"+moment.badge+".png"}});
                guard->next();
                guard->provider_.refreshAdventure(moment.adventure);
            });
        },Qt::QueuedConnection);
    },Qt::QueuedConnection);
}
void AchievementCelebration::setPresentable(bool value) {
    if(presentable_==value)return;presentable_=value;
    if(!value && !current_.isEmpty()){duration_.stop();queue_.prepend(current_);current_.clear();emit changed();}
    if(value)next();
}
void AchievementCelebration::next() {
    if(!valid() || !presentable_ || !current_.isEmpty() || queue_.isEmpty())return;
    current_=queue_.dequeue();provider_.acknowledgeLive(current_["set"].toString(),current_["id"].toString());
    qCInfo(achievementLiveLog)<<"celebration presented"<<current_["set"].toString()<<current_["id"].toString();
    if(!current_.value("played",false).toBool()){sound();current_["played"]=true;}
    duration_.start(5200);emit changed();
}
QByteArray AchievementCelebration::jinglePcm(int rate) {
    // Original warm C-major bloom: soft pluck, rising third, quiet sparkling fifth.
    const int count=rate*0.78;QByteArray bytes(count*2,Qt::Uninitialized);
    for(int i=0;i<count;++i) {
        const double t=double(i)/rate;double sample=0;
        const double notes[]{261.6256,329.6276,391.9954,523.2511};
        for(int n=0;n<4;++n) {
            const double dt=t-n*.045;if(dt<0)continue;
            const double env=(1-std::exp(-dt*180))*std::exp(-dt*(6.5+n*.6))*std::min(1.,std::max(0.,(.78-t)/.10));
            const double phase=2*3.141592653589793*(notes[n]*dt+3*(1-std::exp(-dt*30)));
            sample+=env*(std::sin(phase)+.13*std::sin(phase*2))*.18;
        }
        qToLittleEndian<qint16>(qint16(std::clamp(sample,-.9,.9)*32767),reinterpret_cast<uchar*>(bytes.data()+i*2));
    }
    return bytes;
}
void AchievementCelebration::sound() {
    const int volume=std::clamp(QSettings().value("achievements/volume",65).toInt(),0,100);
    if(!volume)return;
    if(sink_){sink_->stop();delete sink_;sink_=nullptr;}audio_.close();
    QAudioFormat format;format.setSampleRate(24000);format.setChannelCount(1);format.setSampleFormat(QAudioFormat::Int16);
    const auto output=QMediaDevices::defaultAudioOutput();if(!output.isFormatSupported(format))return;
    audio_.setData(jinglePcm());audio_.open(QIODevice::ReadOnly);
    sink_=new QAudioSink(output,format,this);sink_->setVolume(volume/100.0*.45);sink_->start(&audio_);
}
}
