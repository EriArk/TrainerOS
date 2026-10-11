#pragma once
#include "MelonDsSession.h"

namespace trainer::melonds {
inline constexpr auto sourceRevision="906e9ebb27da8c6a715cd7abab4abfe8a8d29427";
QString bundleRoot();
QJsonObject bundle(const std::atomic_bool&);
QJsonObject netplayIdentity(const AdventureRegistration&,const std::atomic_bool&);
struct Request {
    QJsonObject expected,endpoint;
    bool host=false,online=false;
    QString identity,address;
};
ProcessCommand command(const AdventureRegistration&,const Request&,const QString& backupRoot,
                       const SaveTargetResolver&,const QStringList& inputPrefix={});
QJsonObject event(const QByteArray&);
}
