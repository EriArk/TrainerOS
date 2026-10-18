#pragma once
#include <QByteArray>
#include <QHash>
#include <QVariantMap>
#include <QStringList>

namespace trainer {
// Reviewed local presentation metadata. It grants no install or launch capability.
struct CratesCatalogue {
    QHash<QString,QVariantMap> projects;
    QHash<QString,QStringList> contents;
    static bool read(const QByteArray&, const QStringList& crateIds, CratesCatalogue& result);
};
}
