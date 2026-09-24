#pragma once
#include "core/model/Merchant.h"

namespace trainer {
// Internal exact-Emerald semantic operations on verified logical SaveBlock1.
MerchantSnapshot readEmeraldShopBlock(const QByteArray& world, quint32 key);
MerchantWrite buyEmeraldShopBlock(const QByteArray& world, quint32 key, const MerchantPurchase&, quint32 drinkRoll=4095);
}
