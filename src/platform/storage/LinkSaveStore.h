#pragma once
#include "SaveBackupStorage.h"
#include <QJsonObject>
namespace trainer {
bool pendingLinkSave(const QString& root);
QJsonObject linkSaveStatus(const QString& root,const QString& id={});
QJsonObject inspectLinkPokemon(const QString& root,const AdventureRegistration&,int slot,const SaveTargetResolver&);
QJsonObject prepareLinkSave(const QString& root,const AdventureRegistration&,const QJsonObject& request,const SaveTargetResolver&);
QJsonObject commitLinkSave(const QString& root,const AdventureRegistration&,const QString& id,const QString& peerAfter,const SaveTargetResolver&);
QJsonObject finishLinkSave(const QString& root,const QString& id,const QString& peerAfter);
QJsonObject abortPreparedLinkSave(const QString& root,const AdventureRegistration&,const QString& id,const SaveTargetResolver&);
}
