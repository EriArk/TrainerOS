#pragma once
#include "platform/device/NetworkService.h"
#include "core/input/Action.h"
#include <QVariantList>
#include <QTimer>
namespace trainer {
class NetworkController final:public QObject {
    Q_OBJECT
    Q_PROPERTY(bool bluetooth READ bluetooth NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString prompt READ prompt NOTIFY changed)
    Q_PROPERTY(QString confirmLabel READ confirmLabel NOTIFY changed)
    Q_PROPERTY(QString actionLabel READ actionLabel NOTIFY changed)
    Q_PROPERTY(bool canForget READ canForget NOTIFY changed)
public:
    explicit NetworkController(QObject* parent=nullptr);
    void configure(NetworkService* service);
    void setActive(bool active);
    bool bluetooth() const{return bluetooth_;}
    bool busy() const{return busy_;}
    int focusIndex() const{return focus_;}
    QVariantList rows() const{return rows_;}
    QString status() const{return status_;}
    QString prompt() const{return prompt_;}
    QString confirmLabel() const;
    QString actionLabel() const {return selected().value("connected").toBool()?"Disconnect":bluetooth_&&!selected().value("saved").toBool()?"Pair":"Connect";}
    bool canForget() const {return selected().value("saved").toBool();}
    void dispatch(Action action);
    void applyText(const QString& text);
    void cancelText();
    void leave();
    Q_INVOKABLE void selectFace(bool bluetooth);
    Q_INVOKABLE void activate(int index);
    Q_INVOKABLE void toggleRadio(){if(!busy_&&prompt_.isEmpty()&&textMode_.isEmpty())emit radioRequested(bluetooth_?3:2);}
signals:
    void changed();
    void backRequested();
    void textRequested(const QString& title,int limit,bool secret);
    void closeKeyboardRequested();
    void radioRequested(int index);
private:
    void run(const QString& op,const QJsonObject& extra={});
    void receive(const QJsonObject& event);
    void confirm();
    QVariantMap selected() const{return rows_.value(focus_).toMap();}
    NetworkService* service_=nullptr;
    QTimer poll_;
    QVariantList rows_;
    QJsonObject pending_;
    QString status_,prompt_,textMode_,confirmation_;
    bool bluetooth_=false,busy_=false,active_=false;
    int focus_=0;
};
}
