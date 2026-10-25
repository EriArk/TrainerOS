#pragma once
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QTimer>
#include <QSet>

namespace trainer {
// Runtime-only coordination. The transport supplies the authenticated peer key;
// this protocol never authorizes saved-Pokemon transactions or voice changes.
class GameParty final : public QObject {
    Q_OBJECT
public:
    explicit GameParty(QObject* parent=nullptr);
    void configure(QString name,QJsonArray games,QJsonObject current,bool available);
    // Read-only activity projection. Membership/admission stays in LobbyCoordinator.
    void setLobbyOffer(QJsonObject offer) {lobbyOffer_=std::move(offer);}
    void reset();
    void query(const QString& peer);
    void browseCompany(const QString& company);
    bool openCompany(const QString& company,QString policy,QStringList allowed);
    void setCompanyAccess(const QString& company,QString policy,QStringList allowed);
    QJsonArray companyOffers() const;
    void invite(const QString& peer);
    void requestJoin(const QString& peer);
    void answer(bool accept);
    void start();
    // Local admission from an authenticated, all-Ready Lobby launch. No invite
    // or peer packet may call this; the runtime validates the exact game first.
    bool bindLobby(QString ticket,QJsonObject game,QJsonArray members,QString self,QString owner,QString admission={},bool local=false);
    bool admitLobby(QString ticket,QString peer,QString name,QString admission,int seat);
    void releaseLobby(QString peer,QString admission);
    void ready(QJsonObject endpoint);
    // A private runtime credential goes only to its already accepted member.
    // Routes using this operation cannot admit new members after starting.
    bool readyMember(int slot,QJsonObject endpoint);
    void prepareLinked(int slot,QJsonObject endpoint);
    // Small runtime control messages use the already authenticated accepted
    // roster. Native acknowledgement and handoff ordering belong to runtime.
    bool sendRuntimeControl(int slot,const QJsonObject& control);
    void leave();
    void cancelInvite(const QString& peer);
    void disconnected(const QString& peer);
    void deliveryFailed(const QString& peer);
    void receive(QString peer,QString name,const QJsonObject&);
    QJsonObject state() const;
    QJsonObject offer(const QString& peer) const;
    QJsonObject pending() const;
    bool active() const {return !party_.isEmpty();}
    bool host() const {return active()&&host_.isEmpty();}
    bool running() const {return running_;}
    QJsonObject game() const {return game_;}
    QString hostPeer() const {return host_;}
    static QString encode(const QJsonObject&);
    static QJsonObject decode(const QString&);
signals:
    void outgoing(QString peer,QJsonObject packet);
    void changed();
    void startRequested(bool host,QJsonObject endpoint);
    void connectionReady(QJsonObject endpoint);
    void runtimeControlReceived(int senderSlot,QJsonObject control);
    void notice(QString text);
private:
    friend class GamePartyTests;
    struct Peer {QString name,boot,query; QJsonObject offer; qint64 expires=0,queried=0; bool invite=false;};
    struct Member {QString name,boot,request; int slot=0; bool accepted=false; qint64 expires=0;};
    struct Request {QString peer,name,boot,id,party; QJsonObject game; bool joining=false; qint64 expires=0;};
    void packet(const QString&,QString kind,QJsonObject={});
    void tick();
    void publish();
    void advertiseCompany();
    bool sameMember(const QString&) const;
    bool create();
    void clear();
    int freeSlot() const;
    bool supports(const QJsonObject&,const QString& peer={}) const;
    QString name_,boot_,party_,host_,hostBoot_,joiningPeer_,joiningId_;
    QJsonArray games_;
    QJsonObject current_,game_,endpoint_;
    QJsonObject lobbyOffer_;
    QJsonObject joiningGame_;
    QString joiningParty_;
    QMap<QString,qint64> seen_;
    QMap<QString,Peer> peers_;
    QMap<QString,Member> members_;
    QList<Request> requests_;
    QTimer timer_;
    bool available_=false,running_=false;
    bool lobbyBound_=false;
    QSet<QString> lobbyArmed_;
    int preparingSlot_=0,preparingPlayers_=0;
    QSet<int> preparedMembers_;
    QSet<int> endpointMembers_;
    qint64 joiningDeadline_=0;
    quint64 revision_=0,remoteRevision_=0;
    QJsonArray roster_;
    QString company_,access_="request";
    QStringList allowedMembers_;
    QMap<QString,qint64> companyQueries_;
    qint64 advertised_=0;
};
}
