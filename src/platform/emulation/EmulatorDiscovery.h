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
};
struct EmulatorDiscovery {
    QHash<QString, QJsonObject> profiles;
    QStringList notices;
};
EmulatorEnvironment installedEmulators(const QString& stateDirectory, const QString& libraryRoot);
EmulatorDiscovery prepareEmulators(const EmulatorEnvironment&);
}
