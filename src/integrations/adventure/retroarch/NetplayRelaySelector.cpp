#include "NetplayRelaySelector.h"
#include "RetroArchNetplay.h"
#include "core/model/NetplayRelay.h"
#include <QElapsedTimer>
#include <QNetworkReply>
#include <QTcpSocket>
#include <algorithm>
#include <limits>
#include <memory>

namespace trainer::retroarch {
namespace {
// Three fresh TCP handshakes, excluding DNS, with a bounded per-sample wait.
class Samples final : public QObject {
public:
    Samples(QString endpoint,std::function<void(int)> done,QObject* parent):QObject(parent),endpoint_(endpoint),done_(std::move(done)) {next();}
private:
    void next() {
        auto* socket=new QTcpSocket(this);auto* timer=new QTimer(socket);timer->setSingleShot(true);
        auto clock=std::make_shared<QElapsedTimer>();clock->start();
        connect(socket,&QTcpSocket::hostFound,socket,[clock]{clock->restart();});
        const auto finish=[this,socket,timer,clock](bool ok) {
            socket->disconnect();timer->stop();socket->abort();socket->deleteLater();
            if(!ok) {auto done=std::move(done_);deleteLater();done(-1);return;}
            samples_.append(qBound(1,int(clock->elapsed()),5000));
            if(samples_.size()<3) {next();return;}
            std::sort(samples_.begin(),samples_.end());
            const int score=qMin(5000,samples_[1]+samples_[2]-samples_[0]);
            auto done=std::move(done_);deleteLater();done(score);
        };
        connect(socket,&QTcpSocket::connected,socket,[finish]{finish(true);});
        connect(socket,&QTcpSocket::errorOccurred,socket,[finish](QAbstractSocket::SocketError){finish(false);});
        connect(timer,&QTimer::timeout,socket,[finish]{finish(false);});timer->start(1500);
        socket->connectToHost(endpoint_.section('|',0,0),endpoint_.section('|',1,1).toUShort());
    }
    QString endpoint_;QList<int> samples_;std::function<void(int)> done_;
};
}
NetplayRelaySelector::NetplayRelaySelector(QObject* parent):QObject(parent) {
    deadline_.setSingleShot(true);repeat_.setInterval(1000);
    connect(&deadline_,&QTimer::timeout,this,[this]{
        const bool host=self_==owner_;stop();
        if(host)emit failed("Couldn't measure a relay for every player. Try again or choose a relay in Lobby options.");
    });
    connect(&repeat_,&QTimer::timeout,this,[this]{if(!pending_.isEmpty()||self_==owner_)return;emit report(owner_,local_);});
}
NetplayRelaySelector::~NetplayRelaySelector() {stop();}
void NetplayRelaySelector::stop() {
    ++generation_;deadline_.stop();repeat_.stop();
    if(probes_) {
        auto* old=probes_;probes_=nullptr;
        for(auto* reply:old->findChildren<QNetworkReply*>()){reply->disconnect();reply->abort();}
        for(auto* socket:old->findChildren<QTcpSocket*>()){socket->disconnect();socket->abort();}
        for(auto* timer:old->findChildren<QTimer*>())timer->stop();
        // stop() may run inside selected/failed, while a socket emits connected.
        old->deleteLater();
    }
    pending_.clear();players_.clear();reports_.clear();endpoints_.clear();attempts_.clear();local_={};complete_=false;
}
void NetplayRelaySelector::start(QString self,QString owner,QStringList players,QString preference) {
    stop();self_=std::move(self);owner_=std::move(owner);players_=std::move(players);preference_=std::move(preference);
    if(!netplayRelay::valid(preference_)||players_.isEmpty()||players_.size()>4||!players_.contains(self_)||!players_.contains(owner_)) {
        emit failed("The relay selection context changed. Prepare the lobby again.");return;
    }
    probes_=new QObject(this);pending_=preference_=="auto"?netplayRelay::regions():QStringList{preference_};
    // Two bounded directory/sample attempts need at most 19.2 s per region.
    // Leave time for the other players' reports; never retry indefinitely.
    deadline_.start(self_==owner_?25000:30000);
    const auto regions=pending_;const auto generation=generation_;
    for(const auto& region:regions)probe(region,generation);
}
void NetplayRelaySelector::probe(QString region,quint64 generation) {
    if(generation!=generation_||!pending_.contains(region))return;
    ++attempts_[region];
    QNetworkRequest request{QUrl(directory_+region)};request.setTransferTimeout(5000);
    auto* reply=network_.get(request);reply->setParent(probes_);reply->setReadBufferSize(4097);
    auto data=std::make_shared<QByteArray>();
    connect(reply,&QIODevice::readyRead,reply,[reply,data]{data->append(reply->readAll());if(data->size()>4096)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,data,region,generation]{
        data->append(reply->readAll());const auto error=reply->error();reply->deleteLater();
        if(generation!=generation_)return;
        const auto endpoint=data->size()<=4096?netplayRelayEndpoint(*data):QString();
        if(error!=QNetworkReply::NoError||endpoint.isEmpty()) {result(region,{},-1,generation);return;}
        new Samples(endpoint,[this,region,endpoint,generation](int score){result(region,endpoint,score,generation);},probes_);
    });
}
void NetplayRelaySelector::result(QString region,QString endpoint,int score,quint64 generation) {
    if(generation!=generation_||!pending_.contains(region))return;
    if(score<0&&attempts_.value(region)<2&&probes_) {
        // A single lost request/handshake must not discard an otherwise good
        // region. Resolve it afresh, including when the host chose it manually.
        QTimer::singleShot(200,probes_,[this,region,generation]{probe(region,generation);});
        return;
    }
    pending_.removeOne(region);
    local_[region]=score;if(score>=0)endpoints_[region]=endpoint;
    if(!pending_.isEmpty())return;
    if(self_==owner_) {reports_[self_]=local_;decide();}
    else {repeat_.start();emit report(owner_,local_);}
}
void NetplayRelaySelector::receive(const QString& peer,const QJsonObject& scores) {
    if(complete_||self_!=owner_||peer==self_||!players_.contains(peer)||reports_.contains(peer))return;
    const auto regions=preference_=="auto"?netplayRelay::regions():QStringList{preference_};
    if(scores.size()!=regions.size())return;
    for(const auto& region:regions) {
        const auto v=scores[region];const double n=v.toDouble(-2);
        if(!v.isDouble()||n< -1||n>5000||n!=int(n))return;
    }
    reports_[peer]=scores;decide();
}
QString NetplayRelaySelector::choose(const QStringList& players,const QMap<QString,QJsonObject>& reports,const QString& preference) {
    QString best;int bestWorst=std::numeric_limits<int>::max(),bestTotal=bestWorst;
    const auto regions=preference=="auto"?netplayRelay::regions():QStringList{preference};
    if(players.isEmpty()||!netplayRelay::valid(preference))return {};
    for(const auto& region:regions) {
        int worst=0,total=0;bool available=true;
        for(const auto& peer:players) {
            const int score=reports.value(peer).value(region).toInt(-1);
            if(score<0||score>5000){available=false;break;}
            worst=qMax(worst,score);total+=score;
        }
        if(available&&(worst<bestWorst||(worst==bestWorst&&total<bestTotal))) {best=region;bestWorst=worst;bestTotal=total;}
    }
    return best;
}
void NetplayRelaySelector::decide() {
    if(complete_||reports_.size()!=players_.size()||!pending_.isEmpty())return;
    complete_=true;deadline_.stop();const auto region=choose(players_,reports_,preference_);
    if(region.isEmpty()||!endpoints_.contains(region)) {
        emit failed("No selected relay is reachable by every player. Choose another relay or retry.");return;
    }
    emit selected(region,endpoints_[region]);
}
}
