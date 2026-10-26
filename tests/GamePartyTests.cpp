#include <QtTest>
#include <QUuid>
#include "features/adventure/GameParty.h"
#include "core/model/GamePlayers.h"

namespace trainer {
class GamePartyTests : public QObject {
    Q_OBJECT
    static QJsonObject game(int players=4) {
        return {{"id","runtime.test"},{"label","Test cartridge"},{"content","exact-digest"},{"players",players}};
    }
    struct Room {
        GameParty a,b,c,d,e;
        QMap<QString,GameParty*> people{{"a",&a},{"b",&b},{"c",&c},{"d",&d},{"e",&e}};
        Room(int players=4) {
            for(auto it=people.cbegin();it!=people.cend();++it) {
                const auto id=it.key();auto* node=it.value();node->configure(id,{game(players)},id=="a"?game(players):QJsonObject{},true);
                QObject::connect(node,&GameParty::outgoing,node,[this,id](QString target,QJsonObject p){
                    if(people.contains(target))people[target]->receive(id,id,p);
                });
            }
        }
    };
    struct CompanyRoom {
        GameParty a,b,c,d;
        QMap<QString,GameParty*> people{{"a",&a},{"b",&b},{"c",&c},{"d",&d}};
        CompanyRoom() {
            for(auto it=people.begin();it!=people.end();++it) {
                const auto id=it.key();auto* node=it.value();node->configure(id,{game()},game(),true);
                QObject::connect(node,&GameParty::outgoing,node,[this,id](QString target,QJsonObject p){
                    if(target.count(':')==2) {
                        for(auto it=people.begin();it!=people.end();++it)if(it.key()!=id)it.value()->receive(target+":"+id,id,p);
                    } else if(people.contains(target.section(':',-1)))people[target.section(':',-1)]->receive(target.section(':',0,2)+":"+id,id,p);
                });
            }
        }
    };
private slots:
    void emulatorAdmissionLocksEveryBuildAndSyncSetting() {
        for(const auto& id:QStringList{"runtime.retroarch.psx.pcsx_rearmed.v1","runtime.handheld.gba.v1",
                "runtime.ppsspp.psp.v1","runtime.dolphin.gc.v1","runtime.rmg.n64.v1","runtime.flycast-dojo.dc.v1",
                "runtime.melonds.nds.v1","runtime.azahar.3ds.v1","runtime.vita3k.lan.v1","runtime.armsx2.lan.v1"}) {
            const QJsonObject local{{"id",id},{"runtime","build"},{"core","core-build"},{"content","disc"},{"settings","locked"},{"firmware","bios"}};
            QVERIFY(sameMultiplayerGame(local,local));
            for(const auto& field:{"runtime","core","settings","firmware"}) {
                auto changed=local;changed[field]="other";QVERIFY(!sameMultiplayerGame(local,changed));
                changed.remove(field);QVERIFY(!sameMultiplayerGame(local,changed));
            }
        }
    }
    void nativeGroupLaunchDispatchesEveryAssignedSeat() {
        for(const auto& mode:{QString("runtime.flycast.online.v1"),QString("runtime.flycast.cabinet.v1"),QString("runtime.armsx2.lan.v1"),QString("runtime.dolphin.bba.v1"),QString("runtime.xemu.lan.v1")})for(int count:{3,4}) {
            GameParty host,b,c,d;QList<GameParty*> nodes{&host,&b,&c,&d};
            QJsonObject profile=game();profile["id"]=mode;profile["lateJoin"]=false;
            QJsonArray roster;for(int i=0;i<count;++i)roster.append(QJsonObject{{"id",QString::number(i+1)},{"seat",i+1},{"connected",true},{"prepared",true},{"ready",true}});
            QMap<int,int> launches;
            for(int i=0;i<count;++i) {
                auto* node=nodes[i];node->configure(QString::number(i+1),{profile},profile,true);
                connect(node,&GameParty::outgoing,node,[&,i](QString peer,QJsonObject packet){const int target=peer.mid(7).toInt()-1;if(target>=0&&target<count)nodes[target]->receive("online:"+QString::number(i+1),QString::number(i+1),packet);});
                connect(node,&GameParty::startRequested,node,[&,i](bool isHost,QJsonObject endpoint){++launches[i];if(i>0){QVERIFY(!isHost);QCOMPARE(endpoint["slot"].toInt(),i+1);QCOMPARE(endpoint["identity"].toObject(),profile);}});
            }
            const auto ticket=QUuid::createUuid().toString(QUuid::WithoutBraces);
            QVERIFY(host.bindLobby(ticket,profile,roster,"1","1"));
            for(int i=1;i<count;++i)QVERIFY(nodes[i]->bindLobby(ticket,profile,roster,QString::number(i+1),"1"));
            QCOMPARE(launches[0],1);
            host.ready({{"kind",mode.contains("online")?"dc-online-ready":mode.contains("armsx2")?"ps2-ready":mode.contains("dolphin")?"bba-ready":mode.contains("xemu")?"xbox-ready":"cabinet-ready"},{"identity",profile}});
            for(int i=1;i<count;++i)QCOMPARE(launches[i],1);
            host.tick();for(int i=1;i<count;++i){nodes[i]->tick();QCOMPARE(launches[i],1);}
            nodes[count-1]->leave();QVERIFY(host.running());
        }
    }
    void azaharGroupCredentialsGoOnlyToTheirAssignedGuest() {
        for(int count:{2,3,4}) {
            GameParty host,b,c,d;QList<GameParty*> nodes{&host,&b,&c,&d};
            auto profile=game();profile["id"]="runtime.azahar.3ds.v1";profile["lateJoin"]=false;
            QJsonArray roster;for(int i=0;i<count;++i)roster.append(QJsonObject{{"id",QString::number(i+1)},{"seat",i+1},{"connected",true},{"prepared",true},{"ready",true}});
            QMap<int,int> launches;
            for(int i=0;i<count;++i) {
                auto* node=nodes[i];node->configure(QString::number(i+1),{profile},profile,true);
                connect(node,&GameParty::outgoing,node,[&,i](QString peer,QJsonObject packet){const int target=peer.mid(7).toInt()-1;if(target>=0&&target<count)nodes[target]->receive("online:"+QString::number(i+1),QString::number(i+1),packet);});
                connect(node,&GameParty::startRequested,node,[&,i](bool,QJsonObject endpoint){
                    ++launches[i];if(i>0){QCOMPARE(endpoint["slot"].toInt(),i+1);QCOMPARE(endpoint["secret"].toString(),QString::number(i+1));}
                });
            }
            const auto ticket=QUuid::createUuid().toString(QUuid::WithoutBraces);
            QVERIFY(host.bindLobby(ticket,profile,roster,"1","1"));
            for(int i=1;i<count;++i)QVERIFY(nodes[i]->bindLobby(ticket,profile,roster,QString::number(i+1),"1"));
            for(int seat=count;seat>=2;--seat) {
                const QJsonObject packet{{"kind","azahar-ready"},{"identity",profile},{"secret",QString::number(seat)}};
                QVERIFY(host.readyMember(seat,packet));QVERIFY(!host.readyMember(seat,packet));
                for(int i=1;i<count;++i)QCOMPARE(launches.value(i),i+1>=seat?1:0);
            }
            host.tick();for(int i=1;i<count;++i){nodes[i]->tick();QCOMPARE(launches.value(i),1);}
        }
    }
    void firstLiveGuestAndRejoinNeverRestartTheSoloHost() {
        GameParty host,guest;const auto profile=game(2);host.configure("Host",{profile},profile,true);guest.configure("Guest",{profile},{},true);
        connect(&host,&GameParty::outgoing,&guest,[&](QString,QJsonObject p){guest.receive("online:1","Host",p);});
        connect(&guest,&GameParty::outgoing,&host,[&](QString,QJsonObject p){host.receive("online:2","Guest",p);});
        const auto ticket=QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QJsonObject owner{{"id","1"},{"seat",1},{"connected",true},{"prepared",true},{"ready",true}};
        QSignalSpy starts(&host,&GameParty::startRequested),joins(&guest,&GameParty::startRequested);
        QVERIFY(host.bindLobby(ticket,profile,{owner},"1","1"));QCOMPARE(starts.size(),1);
        host.ready({{"kind","ready"},{"identity",profile}});
        QString old;
        for(int i=0;i<2;++i) {
            const auto admission=QUuid::createUuid().toString(QUuid::WithoutBraces);
            QVERIFY(!host.admitLobby("stale","online:2","Guest",admission,2));
            QVERIFY(host.admitLobby(ticket,"online:2","Guest",admission,2));
            QVERIFY(!host.admitLobby(ticket,"online:3","Other",admission,2));
            host.releaseLobby("online:2",old);QCOMPARE(host.state()["members"].toArray().size(),2);
            const QJsonObject player{{"id","2"},{"seat",2},{"connected",true},{"admission",admission}};
            QVERIFY(guest.bindLobby(ticket,profile,{owner,player},"2","1",admission));
            QCOMPARE(joins.size(),i+1);QCOMPARE(joins.last()[1].toJsonObject()["slot"].toInt(),2);
            host.tick();guest.tick();QCOMPARE(starts.size(),1);QCOMPARE(joins.size(),i+1);
            guest.leave();host.releaseLobby("online:2",admission);old=admission;
            QVERIFY(host.running());QCOMPARE(host.state()["members"].toArray().size(),1);
        }
    }
    void incompatibleLiveGuestCannotEndHostGame() {
        GameParty host;const auto profile=game();host.configure("Host",{profile},profile,true);
        const auto ticket=QUuid::createUuid().toString(QUuid::WithoutBraces),admission=QUuid::createUuid().toString(QUuid::WithoutBraces);
        QSignalSpy starts(&host,&GameParty::startRequested);
        QVERIFY(host.bindLobby(ticket,profile,{QJsonObject{{"id","1"},{"seat",1},{"connected",true},{"prepared",true},{"ready",true}}},"1","1"));
        host.ready({{"kind","ready"}});QVERIFY(host.admitLobby(ticket,"online:2","Guest",admission,2));
        auto wrong=profile;wrong["settings"]="wrong";
        host.receive("online:2","Guest",{{"ns","org.traineros.party"},{"v",1},{"kind","lobby-armed"},{"target",ticket},{"party",ticket},
            {"boot",admission},{"expires",QDateTime::currentSecsSinceEpoch()+60},{"game",wrong}});
        QVERIFY(host.running());QCOMPARE(starts.size(),1);QCOMPARE(host.state()["members"].toArray().size(),1);
    }
    void lobbyAdmissionIsBilateralAndKeepsTheChosenSeats() {
        GameParty host,guest;const auto profile=game();
        auto otherBuild=profile;
        host.configure("Host",{profile},profile,true);guest.configure("Guest",{otherBuild},{},true);
        const auto ticket=QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QJsonArray roster{QJsonObject{{"id","2"},{"name","Guest"},{"seat",3},{"connected",true},{"prepared",true},{"ready",true}},
            QJsonObject{{"id","1"},{"name","Host"},{"seat",1},{"connected",true},{"prepared",true},{"ready",true}}};
        QList<QJsonObject> delayed;bool delay=true;
        connect(&host,&GameParty::outgoing,&guest,[&](QString peer,QJsonObject p){QCOMPARE(peer,QString("online:2"));guest.receive("online:1","Host",p);});
        connect(&guest,&GameParty::outgoing,&host,[&](QString peer,QJsonObject p){QCOMPARE(peer,QString("online:1"));if(delay)delayed.append(p);else host.receive("online:2","Guest",p);});
        QSignalSpy starts(&host,&GameParty::startRequested),joins(&guest,&GameParty::startRequested);
        QVERIFY(host.bindLobby(ticket,profile,roster,"1","1"));host.start();QCOMPARE(starts.size(),0);
        QVERIFY(guest.bindLobby(ticket,profile,roster,"2","1"));QCOMPARE(starts.size(),0);
        delay=false;for(const auto& p:delayed)host.receive("online:2","Guest",p);
        QCOMPARE(starts.size(),1);QVERIFY(host.pending().isEmpty());QVERIFY(guest.pending().isEmpty());
        host.ready({{"kind","ready"},{"identity",profile}});QCOMPARE(joins.size(),1);
        QCOMPARE(joins.first()[1].toJsonObject()["slot"].toInt(),3);
        for(const auto& p:delayed)host.receive("online:2","Guest",p);host.tick();guest.tick();QCOMPARE(starts.size(),1);QCOMPARE(joins.size(),1);
        host.leave();QVERIFY(!guest.active());
        auto unready=roster;auto m=unready[0].toObject();m["ready"]=false;unready[0]=m;
        QVERIFY(!host.bindLobby(ticket,profile,unready,"1","1"));
    }
    void lobbyAdmissionRejectsDifferentSettingsAndRecoversLostFirstHello() {
        GameParty host,guest;auto profile=game();auto different=profile;different["settings"]="different-settings";
        host.configure("Host",{profile},profile,true);guest.configure("Guest",{different},different,true);
        const auto ticket=QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QJsonArray roster{QJsonObject{{"id","1"},{"seat",1},{"connected",true},{"prepared",true},{"ready",true}},QJsonObject{{"id","2"},{"seat",2},{"connected",true},{"prepared",true},{"ready",true}}};
        connect(&host,&GameParty::outgoing,&guest,[&](QString,QJsonObject p){guest.receive("online:1","Host",p);});
        connect(&guest,&GameParty::outgoing,&host,[&](QString,QJsonObject p){host.receive("online:2","Guest",p);});
        QSignalSpy starts(&host,&GameParty::startRequested);
        QVERIFY(guest.bindLobby(ticket,different,roster,"2","1")); // First hello precedes host admission.
        QVERIFY(host.bindLobby(ticket,profile,roster,"1","1"));guest.tick();
        QCOMPARE(starts.size(),0);QVERIFY(!host.active());QVERIFY(!guest.active());
        guest.configure("Guest",{profile},profile,true);
        const auto next=QUuid::createUuid().toString(QUuid::WithoutBraces);
        QVERIFY(guest.bindLobby(next,profile,roster,"2","1"));QVERIFY(host.bindLobby(next,profile,roster,"1","1"));guest.tick();
        QCOMPARE(starts.size(),1);
    }
    void soloLobbyStartsOnceWithoutAnArtificialSecondPlayer() {
        GameParty host;const auto profile=game();host.configure("Host",{profile},profile,true);
        QSignalSpy starts(&host,&GameParty::startRequested);
        QVERIFY(host.bindLobby(QUuid::createUuid().toString(QUuid::WithoutBraces),profile,
            {QJsonObject{{"id","1"},{"seat",1},{"connected",true},{"prepared",true},{"ready",true}}},"1","1"));
        QCOMPARE(starts.size(),1);host.tick();host.start();QCOMPARE(starts.size(),1);
    }
    void privateRuntimeCredentialsAreDeliveredOnlyToTheirAcceptedMember() {
        Room r;auto privateGame=game(4);privateGame["lateJoin"]=false;
        for(auto it=r.people.cbegin();it!=r.people.cend();++it)
            it.value()->configure(it.key(),{privateGame},it.key()=="a"?privateGame:QJsonObject{},true);
        r.a.invite("b");r.b.answer(true);r.a.invite("c");r.c.answer(true);r.a.invite("d");r.d.answer(true);
        QSignalSpy b(&r.b,&GameParty::startRequested),c(&r.c,&GameParty::startRequested),d(&r.d,&GameParty::startRequested);
        const auto endpoint=[&](QString secret){return QJsonObject{{"kind","private-ready"},{"identity",privateGame},{"credential",secret}};};
        QVERIFY(!r.a.readyMember(3,endpoint("for-c")));
        r.a.start();QVERIFY(r.a.readyMember(3,endpoint("for-c")));
        QCOMPARE(b.size(),0);QCOMPARE(c.size(),1);QCOMPARE(d.size(),0);
        QCOMPARE(c.first().at(1).toJsonObject()["credential"].toString(),"for-c");
        QVERIFY(!r.a.readyMember(3,endpoint("duplicate")));QVERIFY(!r.a.readyMember(5,endpoint("unaccepted")));
        QVERIFY(r.a.readyMember(4,endpoint("for-d")));QVERIFY(r.a.readyMember(2,endpoint("for-b")));
        QCOMPARE(b.size(),1);QCOMPARE(c.size(),1);QCOMPARE(d.size(),1);
        QCOMPARE(b.first().at(1).toJsonObject()["credential"].toString(),"for-b");
        QCOMPARE(d.first().at(1).toJsonObject()["credential"].toString(),"for-d");
        r.a.leave();QVERIFY(!r.a.readyMember(2,endpoint("closed")));
    }
    void catalogueLimitsDoNotSplitAnExplicitNativePortFamily() {
        Room r;auto host=game(4),guest=game(2);host["runtimePlayers"]=4;guest["runtimePlayers"]=4;
        QVERIFY(sameMultiplayerGame(host,guest));QVERIFY(supportsMultiplayerSession(guest,host));
        auto changed=host;changed["runtimePlayers"]=2;QVERIFY(!supportsMultiplayerSession(guest,changed));
        changed=host;changed.remove("runtimePlayers");QVERIFY(!supportsMultiplayerSession(guest,changed));
        for(int value:{0,1,5}) {changed=host;changed["players"]=value;QVERIFY(!supportsMultiplayerSession(guest,changed));}
        for(const auto& field:{"content","settings"}) {
            changed=host;changed[field]="different";QVERIFY(!supportsMultiplayerSession(guest,changed));
        }
        changed=host;changed["runtime"]="different-architecture";QVERIFY(!supportsMultiplayerSession(guest,changed));
        QVERIFY(!sameMultiplayerGame(game(4),game(2))); // No inferred native capacity.
        r.a.configure("a",{host},host,true);r.b.configure("b",{guest},{},true);
        r.a.invite("b");QCOMPARE(r.b.pending()["game"].toObject(),host);r.b.answer(true);
        r.a.start();r.a.ready({{"kind","ready"}});QVERIFY(r.a.running());QVERIFY(r.b.running());
        QCOMPARE(r.b.state()["capacity"].toInt(),4);r.a.leave();
        r.a.configure("a",{guest},guest,true);r.b.configure("b",{host},{},true);
        r.a.invite("b");r.b.answer(true);QCOMPARE(r.a.state()["capacity"].toInt(),2);
        host["controllerModes"]=QJsonArray{"pass-p1"};guest["controllerModes"]=QJsonArray{"pass-p1"};
        auto turns=multiplayerModeProfile(host,"pass-p1");QVERIFY(supportsMultiplayerSession(guest,turns));
        turns["players"]=3;QVERIFY(!supportsMultiplayerSession(guest,turns));
    }
    void sharedControllerInvitationsRequireTheLocalCapabilityAndExactMode() {
        Room r(2);auto base=game(2);base["controllerModes"]=QJsonArray{"separate","pass-p1"};
        const auto selected=multiplayerModeProfile(base,"pass-p1");QVERIFY(!selected.isEmpty());
        QVERIFY(supportsMultiplayerSession(base,selected));QVERIFY(!sameMultiplayerGame(base,selected));
        QVERIFY(!supportsMultiplayerSession(game(2),selected));
        auto changed=selected;changed["lateJoin"]=true;QVERIFY(!supportsMultiplayerSession(base,changed));
        changed=selected;changed["playMode"]="unreviewed";QVERIFY(!supportsMultiplayerSession(base,changed));
        changed=selected;changed["content"]="another-build";QVERIFY(!supportsMultiplayerSession(base,changed));
        r.a.configure("a",{base},selected,true);r.b.configure("b",{base},{},true);
        r.a.invite("b");QCOMPARE(r.b.pending()["game"].toObject(),selected);
        r.b.answer(true);r.a.start();r.a.ready({{"kind","ready"}});
        QVERIFY(r.a.running());QVERIFY(r.b.running());QCOMPARE(r.a.game(),selected);QCOMPARE(r.b.game(),selected);
        r.a.leave();r.b.configure("b",{game(2)},{},true);r.a.invite("b");QVERIFY(r.b.pending().isEmpty());
    }
    void runtimeControlUsesOnlyTheRunningAcceptedRosterAndCurrentBoot() {
        Room r(2);
        const QJsonObject control{{"channel","hotseat-v1"},{"epoch",1},{"action","release"}};
        QSignalSpy host(&r.a,&GameParty::runtimeControlReceived),guest(&r.b,&GameParty::runtimeControlReceived);
        QJsonObject captured;
        connect(&r.b,&GameParty::outgoing,&r.b,[&](QString,QJsonObject p){if(p["kind"]=="runtime-control")captured=p;});
        QVERIFY(!r.a.sendRuntimeControl(2,control));
        r.a.invite("b");r.b.answer(true);
        QVERIFY(!r.b.sendRuntimeControl(1,control));
        r.a.start();r.a.ready({{"kind","ready"},{"port",55435}});
        QVERIFY(r.b.running());
        QVERIFY(r.a.sendRuntimeControl(2,control));QCOMPARE(guest.size(),1);QCOMPARE(guest[0][0].toInt(),1);
        QVERIFY(r.b.sendRuntimeControl(1,control));QCOMPARE(host.size(),1);QCOMPARE(host[0][0].toInt(),2);
        QCOMPARE(host[0][1].toJsonObject(),control);
        QVERIFY(!r.a.sendRuntimeControl(3,control));QVERIFY(!r.b.sendRuntimeControl(2,control));
        QVERIFY(!r.b.sendRuntimeControl(1,{{"channel","other"}}));
        QVERIFY(!r.b.sendRuntimeControl(1,{{"channel","hotseat-v1"},{"body",QString(800,'x')}}));
        for(const auto& field:{"party","boot","target"}) {
            auto stale=captured;stale[field]=QUuid::createUuid().toString(QUuid::WithoutBraces);
            r.a.receive("b","b",stale);
        }
        r.a.receive("c","c",captured);QCOMPARE(host.size(),1);
        r.a.leave();r.a.receive("b","b",captured);QCOMPARE(host.size(),1);
        QVERIFY(!r.b.sendRuntimeControl(1,control));
    }
    void connectionAvailabilityGatesBothDirectionsAndCompanyAdmission() {
        GameParty a,b;auto local=game();local["connections"]=QJsonArray{"nearby"};
        a.configure("a",{local},local,true);b.configure("b",{game()},game(),true);
        QJsonObject invitation;
        connect(&a,&GameParty::outgoing,&b,[&](QString target,QJsonObject p){
            if(p["kind"]=="invite")invitation=p;
            b.receive(target.section(':',0,0)+":a","a",p);
        });
        connect(&b,&GameParty::outgoing,&a,[&](QString target,QJsonObject p){a.receive(target.section(':',0,0)+":b","b",p);});
        QSignalSpy outgoing(&a,&GameParty::outgoing),started(&a,&GameParty::startRequested);
        a.invite("online:b");QVERIFY(!a.active());QCOMPARE(outgoing.size(),0);
        QVERIFY(!a.openCompany("friends","request",{}));
        b.query("online:a");QVERIFY(!b.offer("online:a")["joinable"].toBool());
        b.requestJoin("online:a");QVERIFY(!b.state()["joining"].toBool());
        // The guest must also enforce its local route when an older/forged
        // remote offer advertises broader availability.
        b.configure("b",{local},local,true);a.configure("a",{game()},game(),true);
        a.invite("online:b");QVERIFY(b.pending().isEmpty());a.leave();
        a.invite("nearby:b");QVERIFY(!b.pending().isEmpty());b.answer(true);
        QCOMPARE(a.state()["members"].toArray().size(),2);
        a.start();QCOMPARE(started.size(),1);a.leave();
        // Availability changed after consent: no late launch through an old
        // accepted roster. Core/content identity itself remains compatible.
        a.invite("nearby:b");b.answer(true);
        auto online=game();online["connections"]=QJsonArray{"online"};
        a.configure("a",{online},online,true);a.start();QCOMPARE(started.size(),1);
        QVERIFY(sameMultiplayerGame(local,online));
        auto malformed=game();malformed["connections"]="nearby";
        QVERIFY(!multiplayerConnectionAllowed(malformed,"nearby"));
    }
    void invitedGuestHasBoundedTimeToSaveWithoutExtendingPacketReplay() {
        Room r;QJsonObject packet;
        connect(&r.a,&GameParty::outgoing,&r.a,[&](QString,QJsonObject value){if(value["kind"]=="invite")packet=value;});
        r.a.invite("b");const auto now=QDateTime::currentSecsSinceEpoch();
        QVERIFY(r.a.members_["b"].expires>=now+295);QVERIFY(r.b.requests_.first().expires>=now+295);
        QVERIFY(packet["expires"].toInteger()<=now+90);QCOMPARE(packet["waitSeconds"].toInt(),300);
        r.a.cancelInvite("b");QVERIFY(r.b.pending().isEmpty());
        // An old host has no waitSeconds extension. A hostile value cannot
        // reserve an unbounded time, and an expired envelope is never revived.
        r.a.invite("b");r.b.requests_.clear();r.b.seen_.clear();packet.remove("waitSeconds");
        r.b.receive("a","a",packet);QVERIFY(r.b.requests_.first().expires<=now+61);
        r.b.requests_.clear();r.b.seen_.clear();packet["waitSeconds"]=99999;
        r.b.receive("a","a",packet);QVERIFY(r.b.requests_.first().expires<=now+301);
        r.b.requests_.clear();r.b.seen_.clear();packet["expires"]=now-1;
        r.b.receive("a","a",packet);QVERIFY(r.b.pending().isEmpty());
    }
    void linkedPreparationHasOneBoundFinalLaunchAndCannotBeReplayed_data() {
        QTest::addColumn<QString>("mode");
        QTest::newRow("gb")<<QString("sameboy-linked-pair-battery-v1");
        QTest::newRow("gba")<<QString("mgba-linked-party-v2");
    }
    void linkedPreparationHasOneBoundFinalLaunchAndCannotBeReplayed() {
        QFETCH(QString,mode);
        Room r(2);auto linked=game(2);linked["settings"]=mode;linked["lateJoin"]=false;
        r.a.configure("a",{linked},linked,true);r.b.configure("b",{linked},{},true);
        r.a.invite("b");r.b.answer(true);r.a.start();
        QSignalSpy started(&r.b,&GameParty::startRequested),ready(&r.b,&GameParty::connectionReady);
        QSignalSpy packets(&r.a,&GameParty::outgoing);
        r.a.ready({{"kind","ready"},{"port",55435}});QCOMPARE(started.size(),0);
        r.a.prepareLinked(2,{{"kind","linked-prepare"},{"identity",linked},{"preparation",QJsonObject{{"code","private"}}}});
        QCOMPARE(started.size(),1);QCOMPARE(started.first()[1].toJsonObject()["slot"].toInt(),2);
        const auto initial=packets.last()[1].toJsonObject();
        r.b.receive("a","a",initial);QCOMPARE(started.size(),1);
        auto forged=initial;forged["endpoint"]=QJsonObject{{"kind","ready"}};
        forged["boot"]="00000000-0000-4000-8000-000000000001";r.b.receive("a","a",forged);QCOMPARE(ready.size(),0);
        forged=initial;forged["endpoint"]=QJsonObject{{"kind","ready"}};forged["slot"]=1;r.b.receive("a","a",forged);QCOMPARE(ready.size(),0);
        r.a.ready({{"kind","ready"},{"identity",linked},{"port",55435}});
        QCOMPARE(started.size(),1);QCOMPARE(ready.size(),1);
        r.a.ready({{"kind","ready"},{"port",55435}});QCOMPARE(ready.size(),1);
        r.b.reset();r.b.receive("a","a",initial);QCOMPARE(started.size(),1);QVERIFY(!r.b.active());
    }
    void linkedCredentialsArePrivateAndCancelledSeatsCompactAtStart() {
        Room r;auto linked=game();linked["settings"]="mgba-linked-party-v2";linked["lateJoin"]=false;
        for(auto* node:r.people)node->configure("player",{linked},linked,true);
        r.a.invite("b");r.b.answer(true);r.a.invite("c");r.c.answer(true);r.a.invite("d");r.d.answer(true);
        r.b.leave(); // The remaining machines must become P2/P3, not P3/P4.
        r.a.start();
        QSignalSpy cStart(&r.c,&GameParty::startRequested),dStart(&r.d,&GameParty::startRequested);
        QSignalSpy cReady(&r.c,&GameParty::connectionReady),dReady(&r.d,&GameParty::connectionReady);
        QSignalSpy sent(&r.a,&GameParty::outgoing);
        auto endpoint=QJsonObject{{"kind","linked-prepare"},{"identity",linked},{"preparation",QJsonObject{{"token","c-private"}}}};
        r.a.prepareLinked(2,endpoint);
        QCOMPARE(sent.size(),1);QCOMPARE(sent[0][0].toString(),QString("c"));
        QCOMPARE(cStart.size(),1);QCOMPARE(dStart.size(),0);
        QCOMPARE(cStart[0][1].toJsonObject()["machines"].toInt(),3);
        r.a.ready({{"kind","ready"},{"identity",linked}});QCOMPARE(cReady.size(),0);
        endpoint["preparation"]=QJsonObject{{"token","d-private"}};r.a.prepareLinked(3,endpoint);
        QCOMPARE(dStart.size(),1);QCOMPARE(dStart[0][1].toJsonObject()["slot"].toInt(),3);
        r.a.prepareLinked(3,endpoint);QCOMPARE(dStart.size(),1);
        r.a.invite("e");QVERIFY(r.e.pending().isEmpty());
        auto altered=sent.last()[1].toJsonObject();altered["endpoint"]=QJsonObject{{"kind","ready"},{"identity",linked},{"machines",4}};
        r.d.receive("a","a",altered);QCOMPARE(dReady.size(),0);
        r.a.ready({{"kind","ready"},{"identity",linked}});QCOMPARE(cReady.size(),1);QCOMPARE(dReady.size(),1);
    }
    void threePlayerPartyRejectsFourthAndReusesVacatedSeat() {
        Room r(3);
        r.a.invite("b");r.b.answer(true);r.a.invite("c");r.c.answer(true);
        QCOMPARE(r.a.state()["capacity"].toInt(),3);QCOMPARE(r.a.state()["free"].toInt(),0);
        r.a.invite("d");QVERIFY(r.d.pending().isEmpty());
        r.b.leave();QCOMPARE(r.a.state()["free"].toInt(),1);
        r.a.invite("d");QVERIFY(!r.d.pending().isEmpty());r.d.answer(true);
        const auto members=r.a.state()["members"].toArray();QCOMPARE(members.size(),3);
        QSet<int> seats;for(const auto& m:members)seats.insert(m.toObject()["slot"].toInt());
        QCOMPARE(seats,QSet<int>({1,2,3}));
        QSignalSpy cStarted(&r.c,&GameParty::startRequested),dStarted(&r.d,&GameParty::startRequested);
        r.a.start();r.a.ready({{"kind","ready"},{"port",55435}});
        QCOMPARE(cStarted.size(),1);QCOMPARE(dStarted.size(),1);
    }
    void localRenamingDoesNotBreakInvitationCompatibility() {
        Room r(2);
        auto renamed=game(2);renamed["label"]="My own title";
        r.b.configure("b",{renamed},{},true);
        r.a.invite("b");QVERIFY(!r.b.pending().isEmpty());
        r.b.answer(true);QVERIFY(r.b.active());
        r.a.start();QSignalSpy started(&r.b,&GameParty::startRequested);
        r.a.ready({{"kind","ready"},{"port",55435}});QCOMPARE(started.size(),1);
        r.b.leave();renamed["content"]="different-game";
        r.b.configure("b",{renamed},{},true);r.a.invite("b");
        QVERIFY(r.b.pending().isEmpty());
    }
    void catalogueCountsNeverInventNetworkSupport() {
        for(const auto& value:QStringList{"1","1 player","1-1","single-player","Solo"}) {
            const QVariantMap metadata{{"players",value}};
            QVERIFY(gamePlayers(metadata).solo());QVERIFY(!permitsMultiplayer(metadata,game()));
        }
        for(const auto& value:QStringList{"","unknown","0","1-0","1 offline / 4 online","1+","1-2-4"}) {
            const QVariantMap metadata{{"players",value}};
            QCOMPARE(gamePlayers(metadata).maximum,0);
            QVERIFY(permitsMultiplayer(metadata,game())); // An exact adapter can still establish support.
            QVERIFY(!permitsMultiplayer(metadata,{}));
        }
        const auto four=gamePlayers({{"players","1–4 players"}});
        QCOMPARE(four.minimum,1);QCOMPARE(four.maximum,4);QCOMPARE(four.label(),QString("1–4 players"));
        const auto turns=gamePlayers({{"players","2 (alternating)"}});
        QCOMPARE(turns.maximum,2);QCOMPARE(turns.mode,QString("Taking turns"));
        QVERIFY(!permitsMultiplayer({{"players","1-8"}},{}));
        QCOMPARE(gamePlayers({{"players","2-3000"}}).maximum,3000); // Does not become party capacity.
    }
    void localSoloMetadataDoesNotHideIndependentMachines() {
        const QVariantMap solo{{"players","1"}};
        for(const auto& id:QStringList{"runtime.ppsspp.adhoc.v1","runtime.ppsspp.lumines-eu.v1",
            "runtime.melonds.nds.v1","runtime.azahar.3ds.v1","runtime.vita3k.lan.v1",
            "runtime.armsx2.lan.v1","runtime.dolphin.bba.v1"}) {
            const QJsonObject profile{{"id",id},{"players",2}};
            QVERIFY2(permitsMultiplayer(solo,profile),qPrintable(id));
            QCOMPARE(profile["players"].toInt(),2);
        }
        for(const auto& settings:QStringList{"mgba-linked-party-v2","sameboy-linked-pair-battery-v1","dcgb-linked-pair-volatile-v1"})
            QVERIFY(permitsMultiplayer(solo,{{"id","runtime.retroarch.link.v1"},{"transport","rollback"},{"settings",settings}}));
        QVERIFY(permitsMultiplayer(solo,{{"id","runtime.handheld.gb-gen1-en.v1"},{"transport","netpacket"}}));
        for(const auto& id:QStringList{"runtime.dolphin.gc.v1","runtime.dolphin.wii.v1","runtime.rmg.n64.v1",
            "runtime.flycast-dojo.dc.v1","runtime.retroarch.psx.pcsx_rearmed.v1","runtime.future.v1"})
            QVERIFY2(!permitsMultiplayer(solo,{{"id",id}}),qPrintable(id));
        QVERIFY(!permitsMultiplayer(solo,{}));
    }
    void soloMetadataExcludesInvitesReverseJoinAndCompanyAdmission() {
        Room r;
        const auto configure=[&](GameParty& node,const QVariantMap& metadata,bool current) {
            const auto profile=permitsMultiplayer(metadata,game())?game():QJsonObject{};
            node.configure("Trainer",profile.isEmpty()?QJsonArray{}:QJsonArray{profile},current?profile:QJsonObject{},true);
        };
        configure(r.a,{{"players","1"}},true);
        r.a.invite("b");QVERIFY(r.b.pending().isEmpty());
        QVERIFY(!r.a.openCompany("group","request",{}));
        r.b.query("a");QVERIFY(!r.b.offer("a")["joinable"].toBool());
        configure(r.a,{{"players","1-4"}},true);
        configure(r.b,{{"players","1"}},false);
        r.a.invite("b");QVERIFY(r.b.pending().isEmpty());
        // A stale pending invitation is also rechecked against refreshed metadata.
        configure(r.b,{},false);r.a.invite("c");QVERIFY(!r.c.pending().isEmpty());
        configure(r.c,{{"players","1"}},false);r.c.answer(true);QVERIFY(!r.c.active());
        configure(r.a,{{"players","1"}},true);r.a.invite("d");QVERIFY(r.d.pending().isEmpty());
    }
    void nativeRoomClosesAdmissionAtStartWithoutChangingOtherProfiles() {
        Room r;
        auto native=game();native["lateJoin"]=false;
        for(auto it=r.people.begin();it!=r.people.end();++it)it.value()->configure(it.key(),{native},it.key()=="a"?native:QJsonObject{},true);
        r.a.invite("b");r.b.answer(true);r.a.start();
        QCOMPARE(r.a.state()["free"].toInt(),0);
        r.a.invite("c");QVERIFY(r.c.pending().isEmpty());
        r.c.query("a");QVERIFY(!r.c.offer("a")["joinable"].toBool());
        QSignalSpy launched(&r.b,&GameParty::startRequested);
        r.a.ready({{"kind","dolphin-ready"},{"port",2626}});
        QCOMPARE(launched.size(),1);QCOMPARE(launched.first()[1].toJsonObject()["slot"].toInt(),2);
        r.a.leave();r.a.invite("c");QVERIFY(!r.c.pending().isEmpty());
    }
    void independentPartiesShareCompanyWithoutSharingCapacityOrEndpoint() {
        CompanyRoom r;
        QVERIFY(r.a.openCompany("group","request",{}));QVERIFY(r.c.openCompany("group","request",{}));
        QCOMPARE(r.b.companyOffers().size(),2);
        r.b.requestJoin("online:company:group:a");r.d.requestJoin("online:company:group:c");
        QVERIFY(!r.a.pending().isEmpty());QVERIFY(!r.c.pending().isEmpty());
        r.a.answer(true);r.c.answer(true);
        QCOMPARE(r.a.state()["members"].toArray().size(),2);QCOMPARE(r.c.state()["members"].toArray().size(),2);
        QVERIFY(r.a.state()["party"]!=r.c.state()["party"]);
        QSignalSpy b(&r.b,&GameParty::startRequested),d(&r.d,&GameParty::startRequested);
        r.a.start();r.a.ready({{"password","private"},{"address","host-a"}});
        QCOMPARE(b.count(),1);QCOMPARE(d.count(),0);
        r.c.start();r.c.ready({{"password","other-private"},{"address","host-c"}});
        QCOMPARE(d.count(),1);QCOMPARE(b.count(),1);
        QCOMPARE(b.first()[1].toJsonObject()["address"].toString(),QString("host-a"));
        QCOMPARE(d.first()[1].toJsonObject()["address"].toString(),QString("host-c"));
        const auto other=r.d.state();
        for(const auto& v:r.d.companyOffers()){QVERIFY(!v.toObject().contains("endpoint"));QVERIFY(!v.toObject().contains("password"));}
        r.a.leave();QCOMPARE(r.b.companyOffers().size(),1);QVERIFY(r.c.active());
        QCOMPARE(r.d.state(),other);QVERIFY(r.d.running());QCOMPARE(d.count(),1);
        r.b.requestJoin("online:company:group:c");r.c.answer(true);
        QCOMPARE(b.count(),2);QCOMPARE(b.last()[1].toJsonObject()["address"].toString(),QString("host-c"));
        QCOMPARE(b.last()[1].toJsonObject()["slot"].toInt(),3);
        QCOMPARE(d.count(),1);QVERIFY(r.d.running());
    }
    void onlySelectedCompanyMembersSkipOrganizerPrompt() {
        CompanyRoom r;r.a.openCompany("group","selected",{"b"});
        r.d.requestJoin("online:company:group:a");QVERIFY(!r.a.pending().isEmpty());QVERIFY(!r.d.active());
        r.b.requestJoin("online:company:group:a");QVERIFY(r.b.active());
        QCOMPARE(r.a.pending()["name"].toString(),QString("d")); // did not accept another pending person
        QCOMPARE(r.a.state()["members"].toArray().size(),2);
        r.a.setCompanyAccess("group","closed",{});
        r.c.requestJoin("online:company:group:a");QVERIFY(!r.c.active());
        r.a.answer(true);QVERIFY(!r.d.active());
    }
    void groupAnnouncementsExpireAndOtherCompanyCannotAdmit() {
        CompanyRoom r;r.a.openCompany("group","selected",{"b"});
        QJsonObject advert=r.b.offer("online:company:group:a");
        advert["company"]="other";r.b.receive("online:company:other:a","a",advert);
        r.b.requestJoin("online:company:other:a");QVERIFY(!r.b.active());QVERIFY(r.a.pending().isEmpty());
        r.b.leave();r.b.peers_["online:company:group:a"].expires=1;r.b.tick();
        for(const auto& v:r.b.companyOffers())QVERIFY(v.toObject()["company"]!="group");
        r.a.reset();QVERIFY(r.a.state()["company"].toString().isEmpty());
    }
    void backgroundFailureIsQuietButRequestedInviteReportsFailure() {
        GameParty a;a.configure("Trainer",{game()},game(),true);
        QSignalSpy notices(&a,&GameParty::notice),packets(&a,&GameParty::outgoing);
        a.query("online:friend");a.deliveryFailed("online:friend");
        QCOMPARE(notices.count(),0);a.query("online:friend");QCOMPARE(packets.count(),1);
        a.invite("online:other");a.deliveryFailed("online:other");
        QCOMPARE(notices.count(),1);QCOMPARE(a.state()["members"].toArray().size(),1);
    }
    void fourMembersWaitForExplicitStartAndReceiveSameEndpoint() {
        Room r;QSignalSpy host(&r.a,&GameParty::startRequested),b(&r.b,&GameParty::startRequested),c(&r.c,&GameParty::startRequested),d(&r.d,&GameParty::startRequested);
        for(const auto& id:{"b","c","d"}){r.a.invite(id);QVERIFY(!r.people[id]->pending().isEmpty());r.people[id]->answer(true);}
        QCOMPARE(r.a.state()["free"].toInt(),0);QCOMPARE(host.count(),0);QCOMPARE(b.count(),0);
        QCOMPARE(r.b.state()["members"].toArray().size(),4);
        r.a.invite("e");QVERIFY(r.e.pending().isEmpty());
        r.a.start();QCOMPARE(host.count(),1);QCOMPARE(b.count(),0);
        r.a.ready({{"address","relay.test"},{"kind","ready"},{"port",55435}});
        for(auto* spy:{&b,&c,&d}){QCOMPARE(spy->count(),1);QCOMPARE(spy->first()[1].toJsonObject()["address"].toString(),QString("relay.test"));}
        QCOMPARE(b.first()[1].toJsonObject()["slot"].toInt(),2);
        QCOMPARE(c.first()[1].toJsonObject()["slot"].toInt(),3);
        QCOMPARE(d.first()[1].toJsonObject()["slot"].toInt(),4);
        r.a.ready({{"address","relay.test"}});QCOMPARE(b.count(),1);
    }
    void capacityCancellationAndDeclineReleaseOnlyReservedSlot() {
        Room r(2);r.a.invite("b");r.a.invite("c");QVERIFY(r.c.pending().isEmpty());
        r.a.cancelInvite("b");QVERIFY(r.b.pending().isEmpty());QCOMPARE(r.a.state()["free"].toInt(),1);
        r.a.invite("c");r.c.answer(false);QCOMPARE(r.a.state()["free"].toInt(),1);
        r.a.invite("b");r.b.answer(true);r.a.cancelInvite("b");QCOMPARE(r.a.state()["free"].toInt(),0);
        r.b.leave();QCOMPARE(r.a.state()["free"].toInt(),1);
    }
    void reverseRequestNeedsHostAcceptanceAndMatchingEdition() {
        Room r;r.b.query("a");QVERIFY(r.b.offer("a")["joinable"].toBool());
        r.b.requestJoin("a");QVERIFY(r.a.pending()["joining"].toBool());QVERIFY(!r.b.active());
        r.a.answer(true);QVERIFY(r.b.active());QCOMPARE(r.b.state()["members"].toArray().size(),2);
        r.c.configure("c",{QJsonObject{{"id","runtime.test"},{"content","other-edition"}}},{},true);
        r.c.query("a");r.c.requestJoin("a");QVERIFY(r.a.pending().isEmpty());QVERIFY(!r.c.active());
    }
    void lateAdmissionCannotSubstituteGameAndReplayCannotReprompt() {
        Room r;QJsonObject invite;
        connect(&r.a,&GameParty::outgoing,&r.a,[&](QString,QJsonObject p){if(p["kind"]=="invite")invite=p;});
        r.a.invite("b");r.b.answer(false);r.b.receive("a","a",invite);QVERIFY(r.b.pending().isEmpty());
        r.b.query("a");r.b.requestJoin("a");
        auto admitted=invite;admitted["kind"]="admitted";admitted["request"]=r.b.joiningId_;
        admitted["game"]=QJsonObject{{"id","runtime.other"}};r.b.receive("a","a",admitted);QVERIFY(!r.b.active());
        r.b.leave();r.a.answer(true);QVERIFY(!r.b.active());
    }
    void expiredRequestsBootAndTransportLossDoNotLaunchOrFreeLiveSeats() {
        Room r;r.a.invite("b");r.b.answer(true);r.a.start();r.a.ready({{"address","relay.test"}});
        r.a.disconnected("b");QVERIFY(r.a.running());QCOMPARE(r.a.state()["members"].toArray().size(),2);
        r.b.disconnected("a");QVERIFY(r.b.running());
        QSignalSpy changes(&r.c,&GameParty::changed);r.c.tick();QCOMPARE(changes.count(),0);
        r.a.invite("c");r.a.members_["c"].expires=1;r.a.tick();QVERIFY(r.c.pending().isEmpty());
        r.a.leave();QVERIFY(!r.a.active());
    }
    void envelopesAreBoundedAndSeparateFromSavedLink() {
        Room r;QJsonObject sent;connect(&r.a,&GameParty::outgoing,&r.a,[&](QString,QJsonObject p){sent=p;});
        r.a.query("b");QCOMPARE(GameParty::decode(GameParty::encode(sent)),sent);
        sent["ns"]="org.traineros.link";QVERIFY(GameParty::decode(GameParty::encode(sent)).isEmpty());
        QVERIFY(GameParty::decode(QString(2001,'a')).isEmpty());
    }
};
}
QTEST_GUILESS_MAIN(trainer::GamePartyTests)
#include "GamePartyTests.moc"
