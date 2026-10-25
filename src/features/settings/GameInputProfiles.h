#pragma once
#include "core/input/Action.h"
#include "core/repository/LibraryRepository.h"
#include <QObject>
#include <QVariantList>
#include <QJsonObject>

namespace trainer {
// User input data belongs to Trainer + stable library game ID, not its executable path.
class GameInputProfiles final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(bool editing READ editing NOTIFY changed)
    Q_PROPERTY(QVariantMap radial READ radial NOTIFY radialChanged)
    Q_PROPERTY(QVariantMap draft READ draft NOTIFY changed)
    Q_PROPERTY(bool choosing READ choosing NOTIFY changed)
    Q_PROPERTY(int page READ page NOTIFY changed)
    Q_PROPERTY(int targetGroup READ targetGroup NOTIFY changed)
    Q_PROPERTY(QStringList targetGroups READ targetGroups CONSTANT)
    Q_PROPERTY(QVariantList visibleRows READ visibleRows NOTIFY changed)
    Q_PROPERTY(QString selectedButton READ selectedButton NOTIFY changed)
public:
    Q_INVOKABLE void selectRadialSlot(int index) {if(index>=0&&index<radial_["slots"].toList().size())emit radialSlotRequested(index);}
    QVariantMap draft() const {return draft_.toVariantMap();}
    bool choosing() const {return choosing_;}
    int page() const {return page_;}
    int targetGroup() const {return targetGroup_;}
    QStringList targetGroups() const {return {"Common","Mouse","Letters","Numbers","Function keys","Rings"};}
    QVariantList visibleRows() const;
    QString selectedButton() const;
    Q_INVOKABLE void setPage(int);
    Q_INVOKABLE void setTargetGroup(int);
    Q_INVOKABLE void selectBinding(const QString&);
    Q_INVOKABLE void back() {dispatch(Action::Back);}
    Q_INVOKABLE void focusRow(int index) {if(index>=0&&index<rows().size()){focus_=index;emit changed();}}
    QVariantMap radial() const {return radial_;}
    void setRadial(QVariantMap value) {radial_=value;emit radialChanged();}
    using QObject::QObject;
    void configure(LibraryRepository* library,const QString& directory,const QString& owner);
    void begin();
    QVariantList rows() const;
    QString title() const;
    QString status() const { return status_; }
    int focusIndex() const {return focus_;}
    bool editing() const {return !game_.isEmpty();}
    Q_INVOKABLE void activate(int index);
    void dispatch(Action);
    QJsonObject snapshot(const QString& game) const;
    static QJsonObject defaults();
    static QStringList targets(bool rings=true);
    static bool valid(const QJsonObject&);
signals:
    void radialSlotRequested(int index);
    void radialChanged();
    void changed();
    void backRequested();
private:
    QVariantMap radial_;
    QString path(const QString& game) const;
    QList<Adventure> games() const;
    bool save();
    LibraryRepository* library_=nullptr;
    QString directory_,owner_,game_,gameTitle_,status_,binding_,layer_="bindings";
    QJsonObject draft_;
    int focus_=0,returnFocus_=0,ring_=-1,slot_=-1;
    bool choosing_=false;
    int page_=0,targetGroup_=0;
};
}
