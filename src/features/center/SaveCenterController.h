#pragma once
#include "core/input/Action.h"
#include "core/model/SaveBackup.h"
#include "core/repository/LibraryRepository.h"
#include <QVariantList>

namespace trainer {
class SaveCenterController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString route READ route NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString query READ query NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY rowsChanged)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(bool readOnly READ readOnly NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool canCreate READ canCreate NOTIFY changed)
    Q_PROPERTY(bool confirming READ confirming NOTIFY changed)
    Q_PROPERTY(bool companion READ companion NOTIFY changed)
    Q_PROPERTY(QString restoreLabel READ restoreLabel NOTIFY changed)
    Q_PROPERTY(bool clinicOpen READ clinicOpen NOTIFY changed)
    Q_PROPERTY(QString treatment READ treatment NOTIFY changed)
    Q_PROPERTY(QString clinicMessage READ clinicMessage NOTIFY changed)
    Q_PROPERTY(bool canHeal READ canHeal NOTIFY changed)
    Q_PROPERTY(int partyCount READ partyCount NOTIFY changed)
    Q_PROPERTY(bool shopsOpen READ shopsOpen NOTIFY changed)
    Q_PROPERTY(QString shopRoute READ shopRoute NOTIFY changed)
    Q_PROPERTY(QVariantList shopCategories READ shopCategories NOTIFY changed)
    Q_PROPERTY(QString shopCategory READ shopCategory NOTIFY changed)
    Q_PROPERTY(QVariantList shopRecipients READ shopRecipients NOTIFY changed)
    Q_PROPERTY(QVariantList shopMoves READ shopMoves NOTIFY changed)
    Q_PROPERTY(int recipientIndex READ recipientIndex NOTIFY changed)
    Q_PROPERTY(int lessonMoveIndex READ lessonMoveIndex NOTIFY changed)
    Q_PROPERTY(QString shopGroup READ shopGroup NOTIFY changed)
    Q_PROPERTY(QVariantList merchants READ merchants NOTIFY changed)
    Q_PROPERTY(QVariantList shopStock READ shopStock NOTIFY changed)
    Q_PROPERTY(QVariantMap shopSelection READ shopSelection NOTIFY changed)
    Q_PROPERTY(int merchantIndex READ merchantIndex NOTIFY changed)
    Q_PROPERTY(int stockIndex READ stockIndex NOTIFY changed)
    Q_PROPERTY(int quantity READ quantity NOTIFY changed)
    Q_PROPERTY(int shopBalance READ shopBalance NOTIFY changed)
    Q_PROPERTY(QString shopMessage READ shopMessage NOTIFY changed)
    Q_PROPERTY(QString shopQuery READ shopQuery NOTIFY changed)
    Q_PROPERTY(QString shopLocation READ shopLocation NOTIFY changed)
    Q_PROPERTY(QStringList shopLocations READ shopLocations NOTIFY changed)
    Q_PROPERTY(int shopLocationIndex READ shopLocationIndex NOTIFY changed)
    Q_PROPERTY(QVariantList basketRows READ basketRows NOTIFY changed)
    Q_PROPERTY(QString basketTotal READ basketTotal NOTIFY changed)
    Q_PROPERTY(int basketIndex READ basketIndex NOTIFY changed)
    Q_PROPERTY(int basketCount READ basketCount NOTIFY changed)
public:
    bool readOnly() const {return service_ && service_->readOnly();}
    bool writing() const {return busy() && writing_;}
    explicit SaveCenterController(LibraryRepository&,QObject* parent=nullptr);
    void configure(SaveBackupService*);
    bool configured() const { return service_ != nullptr; }
    void begin(const QString& preferred={});
    void beginSelected(const QString& adventureId);
    void close();
    void applySearch(const QString&);
    void dispatch(Action);
    Q_INVOKABLE void activate(int index);
    Q_INVOKABLE void create();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void back();
    Q_INVOKABLE void search();
    Q_INVOKABLE void visitClinic();
    Q_INVOKABLE void heal();
    bool clinicOpen() const { return clinicOpen_; }
    QString treatment() const { return treatment_; }
    QString clinicMessage() const;
    bool canHeal() const { return snapshot_.canHeal && !busy() && !readOnly(); }
    int partyCount() const { return snapshot_.partyCount; }
    Q_INVOKABLE void visitShops();
    Q_INVOKABLE void shopActivate(int index);
    bool shopsOpen() const {return shopsOpen_;}
    QString shopRoute() const {return shopRoute_;}
    QString shopGroup() const {return shopGroup_;}
    QVariantList shopCategories() const;
    QString shopCategory() const { return shopCategory_; }
    Q_INVOKABLE void chooseShopCategory(int index);
    QVariantList shopRecipients() const;
    QVariantList shopMoves() const;
    int recipientIndex() const { return recipientIndex_; }
    int lessonMoveIndex() const { return lessonMoveIndex_; }
    QVariantList merchants() const;
    QVariantList shopStock() const;
    QVariantMap shopSelection() const;
    int merchantIndex() const {return merchantIndex_;}
    int stockIndex() const {return stockIndex_;}
    int quantity() const {return quantity_;}
    int shopBalance() const;
    QString shopMessage() const;
    QString shopQuery() const {return shopQuery_;}
    QString shopLocation() const {return shopLocation_;}
    QStringList shopLocations() const;
    int shopLocationIndex() const {return shopLocationIndex_;}
    void applyShopSearch(const QString&);
    QVariantList basketRows() const;
    QString basketTotal() const;
    int basketIndex() const {return basketIndex_;}
    int basketCount() const;
    Q_INVOKABLE void openBasket();
    bool shopModal() const {return shopsOpen_ && shopRoute_!="merchants" && shopRoute_!="stock";}
    void leaveClinic(){clinicOpen_=false;emit changed();}
    void leaveShops() {shopsOpen_=false;emit changed();}
    QString route() const { return route_; }
    QString title() const;
    QString message() const;
    QString query() const { return query_; }
    QVariantList rows() const;
    int focusIndex() const { return focus_; }
    bool busy() const { return service_ && service_->busy(); }
    bool canCreate() const { return route_=="copies" && snapshot_.hasSave && !snapshot_.token.isEmpty() && !busy(); }
    bool confirming() const { return confirming_; }
    bool companion() const { return companion_; }
    QString restoreLabel() const;
signals:
    void changed();
    void rowsChanged();
    void closeRequested();
    void searchRequested(const QString&);
    void restored(const QString& adventureId);
    void messageRequested(const QString&);
    void merchantDiscovered(const QString&);
    void shopSearchRequested(const QString&);
private:
    bool writing_ = false;
    void rebuild();
    void restore();
    void completed(const AdventureRegistration&,quint64,const SaveBackupResult&);
    LibraryRepository& library_;
    SaveBackupService* service_=nullptr;
    QList<Adventure> adventures_;
    AdventureRegistration selected_;
    SaveBackupSnapshot snapshot_;
    SaveBackup confirmation_;
    QString route_="adventures", query_, message_;
    bool open_=false, confirming_=false;
    bool companion_=false, refreshPending_=false;
    int focus_=0;
    quint64 generation_=0;
    bool clinicOpen_=false;
    QString treatment_="ready", clinicMessage_;
    void dispatchShop(Action);
    void purchase();
    const Merchant* merchant() const;
    const MerchantStock* stock() const;
    QList<int> stockRows() const;
    QList<int> merchantRows() const;
    void normalizeShopCategory();
    bool shopsOpen_=false;
    QString shopRoute_="merchants",shopMessage_;
    QString shopGroup_,shopCategory_="marts";
    int recipientIndex_=0,lessonMoveIndex_=0;
    int groupParentIndex_=0;
    int merchantIndex_=0,stockIndex_=0,quantity_=1;
    QString shopQuery_,shopLocation_,basketReturn_="stock";
    int shopLocationIndex_=0,basketIndex_=0;
    QList<MerchantBasketLine> basket_;
    void buyBasket();
    void resetShopContext();
};
}
