// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "NativeSession.h"
#include "PreparedSession.h"
#include <QElapsedTimer>
#include <QProcess>
#include <QtEndian>
#include <array>
#include <functional>
#include <map>
#include <deque>
#include <cstring>

namespace traineros::ds {
// Two physical peers, each running the same two DetMP machines. Reuses the
// reference delayed InputFrame model (type 6), with explicit little-endian
// serialization, secure transport, bounded queues and full-state checkpoints.
// The common shell must supply accepted-party identity and prepared owner state.
class RemoteSession final {
public:
    struct OwnerSeed {uint32_t expectedSize=0;std::optional<std::vector<uint8_t>> bytes;};
    RemoteSession(const QJsonObject& config, const QString& admissionHash, unsigned owner,
                  std::function<void(QJsonObject)> endpoint, std::optional<OwnerSeed> seed=std::nullopt)
        : owner(owner), endpoint(std::move(endpoint)),seed(std::move(seed)) {
        host=config["host"].toBool();
        if (!config["host"].isBool() || owner>1 || host!=(owner==0)
                || !StateDigest(config["identity"].toString()) || !StateDigest(admissionHash)
                || !QFileInfo(config["python"].toString()).isExecutable()
                || !QFileInfo(config["helper"].toString()).isFile())
            throw std::runtime_error("Invalid local network preparation");
        admission=admissionHash;
        if(this->seed) {
            if(this->seed->expectedSize>MaxCartridgeSaveBytes || (this->seed->bytes &&
                    (this->seed->bytes->empty()||this->seed->bytes->size()!=this->seed->expectedSize)))
                throw std::runtime_error("Own cartridge state has an unexpected size");
            ownDigest=this->seed->bytes?StateHash(QByteArray(reinterpret_cast<const char*>(this->seed->bytes->data()),
                qsizetype(this->seed->bytes->size()))):QString("fresh");
        } else stateReceived=stateDelivered=true;
        QJsonObject channel{{"host",host},{"identity",config["identity"]}};
        if(config.contains("relayOnly"))channel["relayOnly"]=config["relayOnly"];
        if(!host)channel["code"]=config["code"];
        QObject::connect(&process,&QProcess::started,&process,[this,channel]{command(channel);});
        QObject::connect(&process,&QProcess::readyReadStandardOutput,&process,[this]{read();});
        QObject::connect(&process,&QProcess::readyReadStandardError,&process,[this]{process.readAllStandardError();});
        QObject::connect(&process,&QProcess::errorOccurred,&process,[this](auto){if(!closing)fail();});
        QObject::connect(&process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),&process,
                         [this](int,QProcess::ExitStatus){if(!closing&&!Finished())fail();});
        clock.start(); lastAdvance=clock.elapsed();
        process.start(config["python"].toString(),{"-I",config["helper"].toString()});
    }
    ~RemoteSession() {closing=true;process.disconnect();process.kill();process.waitForFinished(1000);}
    bool Ready() const {return running&&!failed;}
    bool StartupReady() const {return stateReceived&&stateDelivered&&!failed;}
    std::optional<std::vector<uint8_t>> TakePeerState() {
        if(!StartupReady()||!seed||peerStateTaken)throw std::runtime_error("Peer cartridge is not prepared");
        peerStateTaken=true;return std::move(peerState);
    }
    void MachinesReady(){localMachinesReady=true;maybeReady();}
    bool Failed() const {return failed;}
    QString Failure() const {return failure;}
    uint64_t VerifiedChecks() const {return verifiedChecks;}
    unsigned Delay() const {return delay;}
    bool Finishing() const {return endRequested;}
    std::optional<uint32_t> EndFrame() const {return endFrame;}
    bool ReturnSafe() const {
        if(!endFrame||failed)return false;
        for(const auto& verified:verifiedFrame)if(!verified||*verified<*endFrame)return false;
        return true;
    }
    void OwnerRetained() {if(!retained){retained=true;sendControl({{"kind","finished"}},"finished");}}
    bool Finished() const {return retained&&peerRetained&&retainedAcknowledged&&!failed;}
    void Tick(uint32_t next) {
        nextFrame=next;
        if(failed)return;
        if((!running && clock.elapsed()>180000) || (running && !retained && clock.elapsed()-lastAdvance>10000))fail();
        if(hello&&seed&&seed->bytes&&!stateDelivered) {
            while(!failed && sentStateBytes<seed->bytes->size() && pending.size()+awaitingSent.size()<16) {
                const auto length=std::min<size_t>(16378,seed->bytes->size()-sentStateBytes);
                QByteArray chunk(6+qsizetype(length),0);chunk[0]=10;chunk[1]=char(owner);
                qToLittleEndian<uint32_t>(sentStateBytes,chunk.data()+2);
                std::memcpy(chunk.data()+6,seed->bytes->data()+sentStateBytes,length);
                sentStateBytes+=uint32_t(length);send(chunk);
            }
        }
    }
    void Finish(uint32_t next) {
        nextFrame=next;
        if(!running||failed){fail();return;}
        if(!endRequested) {
            endRequested=true;
            sendControl({{"kind","end-request"},{"next",qint64(next)}});
        }
        chooseEnd();
    }
    bool Inputs(uint32_t frame,FrameInput live,std::vector<FrameInput>& result) {
        if(!Ready() || (endFrame && frame>*endFrame+8))return false;
        nextFrame=frame;
        const uint32_t target=frame+delay;
        if(!inputs[owner].count(target)) {
            if(inputs[owner].size()>=32){fail();return false;}
            live.frame=target;
            if(endRequested){live.keys=0xFFF;live.touching=false;live.x=live.y=0;}
            inputs[owner][target]=live;
            QByteArray bytes(15,0);bytes[0]=6;bytes[1]=char(owner);
            qToLittleEndian<uint32_t>(target,bytes.data()+2);qToLittleEndian<uint32_t>(live.keys,bytes.data()+6);
            bytes[10]=live.touching?1:0;qToLittleEndian<uint16_t>(live.x,bytes.data()+11);qToLittleEndian<uint16_t>(live.y,bytes.data()+13);
            send(bytes);
        }
        if(!inputs[0].count(frame)||!inputs[1].count(frame))return false;
        result={inputs[0].at(frame),inputs[1].at(frame)};return true;
    }
    void Committed(uint32_t frame) {
        inputs[0].erase(frame);inputs[1].erase(frame);nextFrame=frame+1;lastAdvance=clock.elapsed();
    }
    void Checkpoint(const FrameResult& value) {
        if(!value.stateHash||failed)return;
        if(value.seat>=2||localChecks.size()>=32){fail();return;}
        const auto key=std::make_pair(value.seat,value.frame);
        if(!localChecks.emplace(key,value.stateHash).second){fail();return;}
        QByteArray bytes(14,0);bytes[0]=7;bytes[1]=char(value.seat);
        qToLittleEndian<uint32_t>(value.frame,bytes.data()+2);qToLittleEndian<uint64_t>(value.stateHash,bytes.data()+6);
        send(bytes);compare(key);
    }
private:
    using Check=std::pair<uint32_t,uint32_t>;
    void fail(QString reason="connection or protocol") {
        if(failed)return;
        failed=true;failure=std::move(reason);process.kill();
    }
    void command(const QJsonObject& value) {
        if(failed||closing)return;
        const auto bytes=QJsonDocument(value).toJson(QJsonDocument::Compact)+'\n';
        if(bytes.size()>32768 || process.bytesToWrite()+bytes.size()>1024*1024
                || process.write(bytes)!=bytes.size())fail();
    }
    void send(const QByteArray& bytes,const QString& purpose={}) {
        if(bytes.isEmpty()||bytes.size()>16384||pending.size()+awaitingSent.size()>=64){fail();return;}
        awaitingSent.push_back(purpose);command({{"send",QString::fromLatin1(bytes.toBase64())}});
    }
    void sendControl(const QJsonObject& value,const QString& purpose={}) {
        send(QJsonDocument(value).toJson(QJsonDocument::Compact),purpose);
    }
    void begin(unsigned lag) {
        if(running||lag<4||lag>15){fail();return;}
        delay=lag;remoteFrame=lag;running=true;lastAdvance=clock.elapsed();
        for(unsigned seat=0;seat<2;++seat)for(unsigned frame=0;frame<delay;++frame){
            FrameInput f;f.frame=frame;inputs[seat][frame]=f;
        }
    }
    void ping() {pingAt=clock.elapsed();sendControl({{"kind","ping"},{"n",pings}});}
    void maybeReady() {
        if(!hello||!StartupReady())return;
        if(localMachinesReady&&!readyAnnounced) {
            readyAnnounced=true;sendControl({{"kind","machines-ready"}});
        }
        if(host&&readyAnnounced&&peerMachinesReady&&!pingStarted) {pingStarted=true;ping();}
    }
    void peerStateComplete() {
        if(peerState && StateHash(QByteArray(reinterpret_cast<const char*>(peerState->data()),qsizetype(peerState->size())))!=peerDigest){fail("owner state checksum");return;}
        stateReceived=true;
        sendControl({{"kind","owner-state-received"},{"owner",int(1-owner)},{"sha256",peerDigest}});
        maybeReady();
    }
    void chooseEnd() {
        if(host&&endRequested&&peerEndNext&&!endFrame) {
            endFrame=std::max(nextFrame,*peerEndNext)+delay+8;
            sendControl({{"kind","end"},{"frame",qint64(*endFrame)}});
        }
    }
    void compare(Check key) {
        if(!localChecks.count(key)||!remoteChecks.count(key))return;
        if(localChecks.at(key)!=remoteChecks.at(key)){
            fail(QString("checkpoint seat=%1 frame=%2").arg(key.first).arg(key.second));return;
        }
        ++verifiedChecks;
        verifiedFrame[key.first]=key.second;localChecks.erase(key);remoteChecks.erase(key);
    }
    void receive(const QByteArray& bytes) {
        if(bytes.size()>6 && quint8(bytes[0])==10) {
            if(!seed||!peerOffer||stateReceived||!peerState||quint8(bytes[1])!=1-owner
                    || qFromLittleEndian<uint32_t>(bytes.constData()+2)!=peerState->size()
                    || bytes.size()-6!=qsizetype(std::min<size_t>(16378,seed->expectedSize-peerState->size()))) {
                fail("owner state chunk");return;
            }
            peerState->insert(peerState->end(),bytes.begin()+6,bytes.end());
            if(peerState->size()==seed->expectedSize)peerStateComplete();
        } else if(bytes.size()==15 && quint8(bytes[0])==6) {
            const unsigned seat=quint8(bytes[1]);
            FrameInput f;f.frame=qFromLittleEndian<uint32_t>(bytes.constData()+2);
            f.keys=qFromLittleEndian<uint32_t>(bytes.constData()+6);const auto touch=quint8(bytes[10]);
            f.touching=touch==1;f.x=qFromLittleEndian<uint16_t>(bytes.constData()+11);f.y=qFromLittleEndian<uint16_t>(bytes.constData()+13);
            if(!running||seat!=1-owner||f.frame!=remoteFrame||f.keys>0xFFF||touch>1||f.x>255||f.y>191
                    || inputs[seat].size()>=32 || f.frame>uint64_t(nextFrame)+delay+16){fail();return;}
            ++remoteFrame;inputs[seat][f.frame]=f;
        } else if(bytes.size()==14 && quint8(bytes[0])==7) {
            const Check key{quint8(bytes[1]),qFromLittleEndian<uint32_t>(bytes.constData()+2)};
            const uint64_t hash=qFromLittleEndian<uint64_t>(bytes.constData()+6);
            if(!running||key.first>=2||!hash||key.second>uint64_t(nextFrame)+16||remoteChecks.size()>=32
                    || (verifiedFrame[key.first]&&key.second<=*verifiedFrame[key.first])
                    || !remoteChecks.emplace(key,hash).second){fail();return;}
            compare(key);
        } else {
            if(bytes.size()>4096){fail();return;}
            QJsonParseError error;const auto doc=QJsonDocument::fromJson(bytes,&error);const auto value=doc.object();
            if(error.error!=QJsonParseError::NoError||!doc.isObject()){fail();return;}
            const auto kind=value["kind"].toString();
            if(kind=="hello"&&!hello&&value["admission"]==admission&&value["owner"].toInt(-1)==int(1-owner)) {
                hello=true;
                if(seed)sendControl({{"kind","owner-state"},{"owner",int(owner)},
                    {"size",qint64(seed->expectedSize)},{"sha256",ownDigest},{"fresh",!seed->bytes.has_value()}});
                maybeReady();
            } else if(kind=="owner-state"&&hello&&seed&&!peerOffer&&!running) {
                if(value.size()!=5||value["owner"].toInt(-1)!=int(1-owner)
                        || value["size"]!=QJsonValue(qint64(seed->expectedSize))||!value["fresh"].isBool()) {
                    fail("owner state identity");return;
                }
                peerDigest=value["sha256"].toString();const bool fresh=value["fresh"].toBool();
                if((fresh&&peerDigest!="fresh")||(!fresh&&(!StateDigest(peerDigest)||!seed->expectedSize))) {
                    fail("owner state description");return;
                }
                peerOffer=true;
                if(fresh)peerStateComplete();else {peerState.emplace();peerState->reserve(seed->expectedSize);}
            } else if(kind=="owner-state-received"&&hello&&seed&&!stateDelivered
                      &&value["owner"].toInt(-1)==int(owner)&&value["sha256"]==ownDigest
                      &&(!seed->bytes||sentStateBytes==seed->expectedSize)) {
                stateDelivered=true;seed->bytes.reset();maybeReady();
            } else if(kind=="machines-ready"&&hello&&!peerMachinesReady&&StartupReady()) {
                peerMachinesReady=true;maybeReady();
            } else if(kind=="ping"&&hello&&!host&&!running&&value["n"].toInt(-1)==pings&&pings<3) {
                sendControl({{"kind","pong"},{"n",pings++}});
            } else if(kind=="pong"&&hello&&host&&!running&&value["n"].toInt(-1)==pings&&pings<3) {
                maxRtt=std::max(maxRtt,clock.elapsed()-pingAt);
                if(++pings<3)ping();else {
                    const auto lag=unsigned(qBound(qint64(4),(maxRtt+32)/33+2,qint64(15)));
                    sendControl({{"kind","start"},{"delay",int(lag)}});begin(lag);
                }
            } else if(kind=="start"&&hello&&!host&&!running&&pings==3) {
                begin(unsigned(value["delay"].toInt(0)));
            } else if(kind=="end-request"&&running&&!peerEndNext) {
                const auto frame=value["next"].toInteger(-1);
                if(frame<0||frame>qint64(nextFrame)+32||frame+32<nextFrame){fail();return;}
                peerEndNext=uint32_t(frame);Finish(nextFrame);chooseEnd();
            } else if(kind=="end"&&!host&&endRequested&&!endFrame) {
                const auto frame=value["frame"].toInteger(-1);
                if(frame<nextFrame||frame>qint64(nextFrame)+64){fail();return;}
                endFrame=uint32_t(frame);
            } else if(kind=="finished"&&endFrame&&!peerRetained) {
                peerRetained=true;
            } else fail();
        }
    }
    void read() {
        buffer+=process.readAllStandardOutput();
        if(buffer.size()>2*1024*1024){fail();return;}
        while(!failed && buffer.contains('\n')) {
            const auto at=buffer.indexOf('\n');if(at>32768){fail();return;}
            QJsonParseError parse;const auto doc=QJsonDocument::fromJson(buffer.left(at),&parse);buffer.remove(0,at+1);
            const auto value=doc.object();if(parse.error!=QJsonParseError::NoError||!doc.isObject()){fail();return;}
            const auto event=value["event"].toString();
            if(event=="ready"&&host&&!endpointSent) {
                const auto code=value["code"].toString();
                if(code.size()>160||!QRegularExpression("^[0-9]+(?:-[a-z]+){8}$").match(code).hasMatch()){fail();return;}
                endpointSent=true;endpoint({{"mode","online"},{"code",code}});
            } else if(event=="connected"&&!connected) {
                connected=true;sendControl({{"kind","hello"},{"admission",admission},{"owner",int(owner)}});
            } else if(event=="sent"&&!awaitingSent.empty()) {
                const auto seq=value["sequence"].toInteger(-1);
                if(seq!=sentSequence++){fail();return;}
                pending[seq]=awaitingSent.front();awaitingSent.pop_front();
            } else if(event=="ack"&&!pending.empty()) {
                const auto seq=value["sequence"].toInteger(-1);
                if(seq!=pending.begin()->first){fail();return;}
                if(pending.begin()->second=="finished")retainedAcknowledged=true;
                pending.erase(pending.begin());
            } else if(event=="data"&&connected) {
                const auto seq=value["sequence"].toInteger(-1);
                const auto raw=value["data"].toString().toLatin1();
                const auto decoded=QByteArray::fromBase64Encoding(raw,QByteArray::AbortOnBase64DecodingErrors);
                if(seq!=receivedSequence++||raw.size()>21848||!decoded){fail();return;}
                receive(decoded.decoded);if(!failed)command({{"consume",seq}});
            } else fail();
        }
    }
    unsigned owner,delay=0,remoteFrame=0,nextFrame=0;
    int pings=0;
    bool host=false,connected=false,hello=false,running=false,failed=false,closing=false,endpointSent=false;
    bool endRequested=false,retained=false,peerRetained=false,retainedAcknowledged=false;
    std::optional<uint32_t> peerEndNext,endFrame;
    std::array<std::optional<uint32_t>,2> verifiedFrame;
    std::array<std::map<uint32_t,FrameInput>,2> inputs;
    std::map<Check,uint64_t> localChecks,remoteChecks;
    std::deque<QString> awaitingSent;
    std::map<qint64,QString> pending;
    qint64 sentSequence=0,receivedSequence=0,lastAdvance=0,pingAt=0,maxRtt=0;
    QString admission,failure;
    uint64_t verifiedChecks=0;
    QByteArray buffer;
    QElapsedTimer clock;
    QProcess process;
    std::function<void(QJsonObject)> endpoint;
    std::optional<OwnerSeed> seed;
    std::optional<std::vector<uint8_t>> peerState;
    QString ownDigest,peerDigest;
    uint32_t sentStateBytes=0;
    bool peerOffer=false,stateReceived=false,stateDelivered=false,peerStateTaken=false;
    bool localMachinesReady=false,peerMachinesReady=false,readyAnnounced=false,pingStarted=false;
};
}
