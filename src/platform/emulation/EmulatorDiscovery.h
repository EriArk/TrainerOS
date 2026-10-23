#pragma once
#include <QHash>
#include <QJsonObject>
#include <QStringList>

namespace trainer {
// Installation-time/startup snapshot. Never run discovery from a capability
// getter or while drawing a page. Inventory is injectable for content-free tests.
struct EmulatorEnvironment {
    QString home, configHome, stateDirectory, libraryRoot;
    QHash<QString, QString> executables, flatpaks;
    QStringList nativeCoreDirectories;
    QString controllerName;
    QHash<QString, int> controllerButtons; // melonDS raw SDL button/hat encoding
    QHash<QString, QString> switchBindings; // Eden native SDL parameter packages
};
struct EmulatorDiscovery {
    QHash<QString, QJsonObject> profiles;
    QStringList notices;
};
EmulatorEnvironment installedEmulators(const QString& stateDirectory, const QString& libraryRoot, bool readController = true);
EmulatorDiscovery prepareEmulators(const EmulatorEnvironment&);
// Resolve Flatpak's selected branch when `info APP` is ambiguous.
QString currentFlatpakDeployment(const QString& app, const QStringList& installations);
}
