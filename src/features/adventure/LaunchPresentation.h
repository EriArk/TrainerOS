#pragma once
#include <QObject>
#include <QVariantMap>
namespace trainer {
class AdventureLaunchController;class LaunchPreparation;class RuntimeMultiplayer;
class SocialController;class LibraryRepository;class AdventureExitPresentation;
// Presentation of existing launch owners, never a second launcher.
class LaunchPresentation final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
public:
    LaunchPresentation(LibraryRepository&,LaunchPreparation&,AdventureLaunchController&,
                       RuntimeMultiplayer&,SocialController&,AdventureExitPresentation&);
    QVariantMap state() const;
    bool visible() const;
    Q_INVOKABLE void cancel();
signals:
    void changed();
private:
    LibraryRepository& library_;LaunchPreparation& preparation_;AdventureLaunchController& lifecycle_;
    RuntimeMultiplayer& multiplayer_;SocialController& social_;AdventureExitPresentation& exit_;
};
}
