#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include "core/model/PartyMove.h"
namespace trainer {
// Flattened saved position: Party 0..5, Boxes 6 + box*30 + slot.
QJsonArray emeraldBattleBag(const QByteArray&,const QString&);
PartyMoveResult settleEmeraldBattle(const QByteArray&,const QString&,const QJsonObject& terms,
    const QString& outcome,const QJsonObject& used);
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
