#pragma once
#include <QByteArray>
#include <QString>
#include <QList>
#include <functional>

namespace trainer {
enum class MerchantCurrency { Money, Coins, BattlePoints, Ash, BerryPowder };
struct MerchantMoveSlot { QString name; bool available=false; };
struct MerchantRecipient {
    int slot=-1; QString identity, name, reason; bool available=false;
    QList<MerchantMoveSlot> moves;
};
struct MerchantStock {
    int itemId=0, price=0, owned=0, maximum=0;
    QString name, pocket;
    QString kind="item";
    QList<MerchantRecipient> recipients;
};
struct Merchant {
    QString id, name="???", location, category, group;
    MerchantCurrency currency=MerchantCurrency::Money;
    bool discovered=false, available=false;
    QList<MerchantStock> stock;
    QString availability;
    int balance=0;
    QString section;
};
struct MerchantSnapshot {
    bool supported=false;
    int balance=0;
    QString error, lineage, discoveryNotice;
    QList<Merchant> merchants;
};
struct MerchantPurchase { QString merchantId; int itemId=0, quantity=0; QString kind="item"; int partySlot=-1, moveSlot=-1; QString recipientIdentity; };
struct MerchantWrite { QByteArray data; QString error, message; };
using MerchantReader=std::function<MerchantSnapshot(const QByteArray&,const QString&)>;
using MerchantBuyer=std::function<MerchantWrite(const QByteArray&,const QString&,const MerchantPurchase&)>;
}
