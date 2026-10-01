#pragma once
#include "core/input/Action.h"
#include <QObject>
#include <QThread>
#include <QVariantMap>
#include <QVariantList>
#include <QHash>
#include <QTimer>

namespace trainer {
class FluxerSession;
class SocialController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool contacts READ contacts NOTIFY changed)
    Q_PROPERTY(QString searchKind READ searchKind NOTIFY changed)
    Q_PROPERTY(QString query READ query NOTIFY changed)
    Q_PROPERTY(int searchFocus READ searchFocus NOTIFY changed)
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY changed)
    Q_PROPERTY(QVariantMap account READ account NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(QVariantList messages READ messages NOTIFY changed)
    Q_PROPERTY(QVariantList hints READ hints NOTIFY changed)
    Q_PROPERTY(QString draft READ draft NOTIFY changed)
    Q_PROPERTY(QString conversationName READ conversationName NOTIFY changed)
    Q_PROPERTY(bool conversation READ conversation NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(int messageIndex READ messageIndex NOTIFY changed)
    Q_PROPERTY(bool reading READ reading NOTIFY changed)
    Q_PROPERTY(QStringList menu READ menu NOTIFY changed)
    Q_PROPERTY(int menuIndex READ menuIndex NOTIFY changed)
    Q_PROPERTY(QString menuTitle READ menuTitle NOTIFY changed)
    Q_PROPERTY(QString menuDetail READ menuDetail NOTIFY changed)
public:
    explicit SocialController(QObject* parent = nullptr);
    ~SocialController() override;
    void setOwner(QString owner);
    void setFace(QString face);
    void dispatch(Action action);
    void showContacts();
    bool contacts() const { return contacts_; }
    QString searchKind() const { return searchKind_; }
    QString query() const { return query_; }
    int searchFocus() const { return searchFocus_; }
    QVariantList searchResults() const { return snapshot_.value("searchResults").toList(); }
    Q_INVOKABLE void setSearchKind(QString kind);
    Q_INVOKABLE void editSearch();
    Q_INVOKABLE void activateSearch(int index);

    void applyText(QString text);
    void preserveText(QString text);
    void closeMenu() { if(!menu_.isEmpty()){menu_.clear();emit changed();} }
    QVariantMap account() const;
    QVariantList rows() const;
    QVariantList messages() const { return snapshot_.value("messages").toList(); }
    QVariantList hints() const;
    QString draft() const { return drafts_.value(snapshot_.value("channel").toString()); }
    QString conversationName() const;
    bool conversation() const { return !snapshot_.value("channel").toString().isEmpty(); }
    int focusIndex() const {return focus_;}
    int messageIndex() const {return messageFocus_;}
    bool reading() const {return reading_;}
    QStringList menu() const {return menu_;}
    int menuIndex() const {return menuFocus_;}
    QString menuTitle() const {return menuTitle_;}
    QString menuDetail() const {return menuDetail_;}
    Q_INVOKABLE void activate(int index);
    Q_INVOKABLE void compose();
    Q_INVOKABLE void login();
    Q_INVOKABLE void selectMenu(int index);
signals:
    void changed();
    void textRequested(QString title, QString initial, int limit);
    void commandRequested(QString operation, QVariantMap args);
    void ownerRequested(QString owner, quint64 generation);
private:
    friend class SocialTests;
    void receive(quint64 generation, QVariantMap snapshot);
    void openMenu();
    void openPeople(QString mode);
    QVariantMap currentChat() const;
    void confirmAction(QString title, QString operation, QString id = {});
    void send();
    QThread thread_;
    FluxerSession* session_;
    QVariantMap snapshot_;
    QHash<QString,QString> drafts_;
    QString owner_, face_ = "chats", textPurpose_, textChannel_;
    QStringList menu_, menuCommands_;
    QString menuSubject_, query_;
    QString menuTitle_, menuDetail_, menuMode_, menuChannel_, textId_;
    QStringList pickedPeople_;
    QHash<QString,QString> editDrafts_;
    quint64 generation_ = 0;
    int focus_ = 0, messageFocus_ = 0, menuFocus_ = 0;
    bool reading_ = false, contacts_ = false, searchStarted_ = false;
    QString searchKind_ = "communities";
    int searchFocus_ = -1;
    QTimer selection_;
    QTimer draftSave_;
    QString draftFile_;
    void saveDrafts();
    void bindDrafts(const QString& accountId);
    void preview();
    void runSearch(int offset = 0);

};
}
