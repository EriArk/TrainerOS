#pragma once
#include "core/model/Merchant.h"

namespace trainer {
// Internal exact-Emerald operations on verified logical SaveBlock1 and SaveBlock2.
// Both candidate blocks must be committed together by the protected save writer.
struct EmeraldShopWrite { QByteArray data; QString error, message; QByteArray trainer; };
MerchantSnapshot readEmeraldShopBlock(const QByteArray& world, const QByteArray& trainer);
EmeraldShopWrite buyEmeraldShopBlock(const QByteArray& world, const QByteArray& trainer, const MerchantPurchase&, quint32 drinkRoll=4095);
}
