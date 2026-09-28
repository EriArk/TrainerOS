#pragma once
#include <QJsonObject>
#include "core/model/PartyMove.h"
namespace trainer {
QJsonObject emeraldLinkAccount(const QByteArray& save,const QString& hash);
PartyMoveResult sellEmeraldPokemon(const QByteArray& save,const QString& hash,int position,
    const QString& revision,const QJsonObject& incoming,int price,bool seller);
struct EmeraldLinkOffer { QJsonObject pokemon; QString error; };
// Exact Emerald semantic record, not a raw save or encrypted Pokemon blob.
EmeraldLinkOffer exportEmeraldLinkRecord(const QByteArray&);
QByteArray importEmeraldLinkRecord(const QJsonObject&);
EmeraldLinkOffer prepareEmeraldReceived(const QJsonObject&);
EmeraldLinkOffer emeraldLinkOffer(const QByteArray& save,const QString& hash,int slot);
PartyMoveResult tradeEmeraldPokemon(const QByteArray& save,const QString& hash,int slot,
    const QString& revision,const QJsonObject& incoming);
}
