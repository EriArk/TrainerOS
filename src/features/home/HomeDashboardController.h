#pragma once
#include "core/input/Action.h"
#include "core/repository/LibraryRepository.h"
#include "integrations/achievements/AchievementProvider.h"
#include "features/social/SocialController.h"
#include <QObject>

namespace trainer {
// Read-only projections of existing services. This object owns focus, not data.
class HomeDashboardController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList chats READ chats NOTIFY changed)
    Q_PROPERTY(QVariantList activities READ activities NOTIFY changed)
    Q_PROPERTY(QVariantList achievements READ achievements NOTIFY changed)
    Q_PROPERTY(bool genericLayout READ genericLayout NOTIFY changed)
    Q_PROPERTY(QString focus READ focus NOTIFY changed)
public:
    HomeDashboardController(LibraryRepository&,AchievementProvider&,SocialController&,QObject* parent=nullptr);
    QVariantList chats() const;
    QVariantList activities() const;
    QVariantList achievements() const;
    void setContext(QString adventure,bool generic);
    bool genericLayout() const {return generic_;}
    QString focus() const{return focus_;}
    void select(const QString&);
    void move(Action);
    QVariantMap target(const QString&) const;
    void reset(){focus_="play";emit changed();}
signals:
    void changed();
    void scrollRequested(int direction);
private:
    QList<QStringList> zones() const;
    LibraryRepository& library_;
    AchievementProvider& provider_;
    SocialController& social_;
    QString focus_="play",adventure_;
    bool generic_=false;
};
}
