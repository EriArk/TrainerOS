#pragma once
#include "core/model/SaveBackup.h"
#include <QThread>

namespace trainer {
using SaveTargetResolver = std::function<SaveTarget(const AdventureRegistration&)>;
struct ProcessCommand;
// Best-effort metadata only; failure never prevents ordinary play or changes saves.
QString observeSaveSession(ProcessCommand&,const QString& root,const AdventureRegistration&,const SaveTargetResolver&);
// Missing policy preserves the previous write-enabled installation. Corrupt policy fails closed.
bool saveWritesReadOnly(const QString& root);
QString setSaveWritesReadOnly(const QString& root, bool enabled);
// All filesystem work below runs on a worker. Targets come from verified adapters.
SaveBackupSnapshot inspectSaveBackups(const QString& root, const SaveTarget&, const SaveHealer& = {}, const MerchantReader& = {});
SaveBackupResult createSaveBackup(const QString& root, const AdventureRegistration&, const QString& token, const SaveTargetResolver&);
SaveBackupResult restoreSaveBackup(const QString& root, const AdventureRegistration&, const SaveBackup&, const QString& token, const SaveTargetResolver&);
SaveBackupResult healSaveParty(const QString& root, const AdventureRegistration&, const QString& token, const SaveTargetResolver&, const SaveHealer&);
SaveBackupResult purchaseSaveItems(const QString& root,const AdventureRegistration&,const QString&,const MerchantPurchase&,const SaveTargetResolver&,const MerchantBuyer&,const MerchantReader&);

SaveBackupResult moveSavePokemon(const QString&,const AdventureRegistration&,const QString&,const PartyMove&,const SaveTargetResolver&,const PartyMover&);

SaveBackupResult releaseSavePokemon(const QString&,const AdventureRegistration&,const QString&,const PokemonRelease&,const SaveTargetResolver&,const PokemonReleaser&);

SaveBackupResult changeSaveHeldItem(const QString&,const AdventureRegistration&,const QString&,const HeldItemChange&,const SaveTargetResolver&,const HeldItemWriter&);

class LocalSaveBackupService final : public SaveBackupService {
    Q_OBJECT
public:
    LocalSaveBackupService(QString root, SaveTargetResolver,
        std::function<bool(const AdventureRegistration&)> supports, QObject* parent = nullptr);
    ~LocalSaveBackupService() override;
    bool busy() const override { return busy_; }
    bool readOnly() const override { return readOnly_; }
    void setReadOnly(bool, QObject*, std::function<void(QString)>) override;
    bool supports(const AdventureRegistration& r) const override { return supports_(r); }
    void inspect(const AdventureRegistration&, QObject*, std::function<void(SaveBackupSnapshot)>) override;
    void create(const AdventureRegistration&, const QString&, QObject*, std::function<void(SaveBackupResult)>) override;
    void restore(const AdventureRegistration&, const SaveBackup&, const QString&, QObject*, std::function<void(SaveBackupResult)>) override;
    void heal(const AdventureRegistration&, const QString&, QObject*, std::function<void(SaveBackupResult)>) override;
    void configureHeldItems(HeldItemWriter writer) { heldItemWriter_=std::move(writer); }
    void changeHeldItem(const AdventureRegistration&,const QString&,const HeldItemChange&,QObject*,std::function<void(SaveBackupResult)>) override;
    void configureRelease(PokemonReleaser releaser) { releaser_=std::move(releaser); }
    void releasePokemon(const AdventureRegistration&,const QString&,const PokemonRelease&,QObject*,std::function<void(SaveBackupResult)>) override;
    void configureMovement(PartyMover mover) { mover_=std::move(mover); }
    void movePokemon(const AdventureRegistration&,const QString&,const PartyMove&,QObject*,std::function<void(SaveBackupResult)>) override;
    void configureHealing(SaveHealer healer) { healer_ = std::move(healer); }
    void configureShops(MerchantReader reader,MerchantBuyer buyer) {shops_=std::move(reader);buyer_=std::move(buyer);}
    void purchase(const AdventureRegistration&,const QString&,const MerchantPurchase&,QObject*,std::function<void(SaveBackupResult)>) override;
private:
    void run(std::function<SaveBackupResult()>, QObject*, std::function<void(SaveBackupResult)>);
    QString root_;
    SaveTargetResolver resolve_;
    std::function<bool(const AdventureRegistration&)> supports_;
    QThread thread_;
    QObject* worker_;
    bool busy_ = false, readOnly_ = false;
    SaveHealer healer_;
    PartyMover mover_;
    PokemonReleaser releaser_;
    HeldItemWriter heldItemWriter_;
    MerchantReader shops_;
    MerchantBuyer buyer_;
};
}
