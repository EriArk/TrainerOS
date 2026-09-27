#pragma once
#include "core/model/Merchant.h"
#include "core/model/PartySnapshot.h"

namespace trainer {
HeldItemBag readEmeraldHeldBag(const QByteArray& world,const QByteArray& trainer);
QByteArray exchangeEmeraldHeldBag(const QByteArray& world,const QByteArray& trainer,int give,int take,QString& error);
// Internal exact-Emerald operations on verified logical SaveBlock1 and SaveBlock2.
// Both candidate blocks must be committed together by the protected save writer.
struct EmeraldShopWrite { QByteArray data; QString error, message; QByteArray trainer; };
MerchantSnapshot readEmeraldShopBlock(const QByteArray& world, const QByteArray& trainer);
EmeraldShopWrite buyEmeraldShopBlock(const QByteArray& world, const QByteArray& trainer, const MerchantPurchase&, quint32 drinkRoll=4095);
}
