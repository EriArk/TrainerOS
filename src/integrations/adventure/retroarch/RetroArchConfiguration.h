#pragma once
#include "RetroArchAdapter.h"

namespace trainer::retroarch {
using Settings = QHash<QString, QString>;
Settings readSettings(const QString& path);
bool safePath(const QString& path);
bool enabled(const Settings&, const QString& key, bool fallback = false);
QString configuredPath(const Settings&, const QString& key, const QString& fallback = {});
QStringList contextFiles(const AdventureRegistration&, const RetroArchInstallation&, const Settings&);
bool supportedConfiguration(const AdventureRegistration&, const RetroArchInstallation&, const Settings&);
QString fileDigest(const QString& path, qint64 limit, const std::atomic_bool& cancelled);
}
