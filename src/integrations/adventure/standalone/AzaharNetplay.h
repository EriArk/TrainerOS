#pragma once
#include "StandaloneAdapter.h"

namespace trainer::azahar {
inline constexpr auto sourceRevision="219d743a1e698101fe1ab8bef43e6563126c3c27";
QString bundleRoot();
QJsonObject bundle(const std::atomic_bool&);
QJsonObject netplayIdentity(const AdventureRegistration&,const std::atomic_bool&);
struct Request {
    QJsonObject expected,endpoint;
    bool host=false,online=false;
    QString identity,address;
};
ProcessCommand command(const AdventureRegistration&,const Request&,const QStringList& inputPrefix={});
QJsonObject event(const QByteArray&);
}
