#pragma once
#include "core/repository/LibraryRepository.h"
#include "platform/process/ProcessService.h"

namespace trainer::flycast {
inline constexpr auto sourceRevision="d0e47e572b1e7b355e88bda8308c89d0c5156cbf";
QString bundleRoot();
QJsonObject bundle(const std::atomic_bool&);
QJsonObject netplayIdentity(const AdventureRegistration&,const std::atomic_bool&);
struct Request {
    QJsonObject expected;
    bool host=false,online=false;
    QString address,token,key;
};
ProcessCommand command(const AdventureRegistration&,const Request&,const QStringList& inputPrefix={});
// Only explicit adapter markers may change session state; ordinary output is ignored.
QJsonObject event(const QByteArray& line);
}
