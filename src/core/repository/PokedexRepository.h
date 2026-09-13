#pragma once
#include <QList>
#include <QHash>
#include <QStringList>
#include <optional>
#include <QObject>
#include <functional>

namespace trainer {
struct PokedexForm {
    QString id, name;
    QStringList types;
    int heightDm = 0, weightHg = 0; // Zero denotes unavailable reference data.
    QList<int> stats = {}; // HP, Attack, Defense, Sp. Atk, Sp. Def, Speed; zero is unknown.
};
struct PokedexEntry {
    QString id;
    int number;
    QString name;
    QStringList types;
    QStringList collectionIds;
    QList<PokedexForm> forms = {};
    QStringList familyIds = {};
};
struct PokedexCollection { QString id; QString name; };
struct PokedexCatalog {
    bool success = true;
    QList<PokedexEntry> entries;
    QList<PokedexCollection> collections;
    QString error;
    QString source = "Sample guide";
};
struct PokedexProgress {
    std::optional<bool> seen;
    std::optional<bool> caught;
    bool favorite = false;
    QString notes = {};
    int revision = 0;
};
struct PokedexWriteResult { bool success = false; QString error; int revision = 0; };
QString validatePokedexRecord(const QString&, const PokedexProgress&);
class PokedexReferenceProvider {
public:
    virtual ~PokedexReferenceProvider() = default;
    virtual PokedexCatalog load() = 0;
};
class PokedexProgressRepository {
public:
    virtual ~PokedexProgressRepository() = default;
    virtual PokedexProgress progress(const QString& entryId) const = 0;
    // Empty string means success. Failure must leave the old record intact.
    virtual void setFavoriteAsync(const QString& entryId, bool favorite, QObject* context,
                                  std::function<void(QString)> completed) = 0;
    virtual bool recordsEditable() const { return false; }
    virtual void saveRecordAsync(const QString&, const PokedexProgress&, QObject*, std::function<void(PokedexWriteResult)> completed) {
        completed({false,"Your field journal is read-only."});
    }
};
// One fixture implements two replaceable boundaries; it performs no I/O.
class MockPokedexRepository final : public PokedexReferenceProvider, public PokedexProgressRepository {
public:
    MockPokedexRepository();
    PokedexCatalog load() override;
    PokedexProgress progress(const QString& id) const override { return progress_.value(id); }
    QString setFavorite(const QString& id, bool favorite);
    void setFavoriteAsync(const QString& id, bool favorite, QObject*, std::function<void(QString)> completed) override {
        completed(setFavorite(id, favorite));
    }
    void failNextLoad() { failLoad_ = true; }
    void failNextWrite() { failWrite_ = true; }
    bool recordsEditable() const override { return true; }
    void saveRecordAsync(const QString&, const PokedexProgress&, QObject*, std::function<void(PokedexWriteResult)>) override;
private:
    PokedexCatalog catalog_;
    QHash<QString, PokedexProgress> progress_;
    bool failLoad_ = false, failWrite_ = false;
};
}
