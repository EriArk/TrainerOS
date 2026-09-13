#pragma once
#include "core/model/Models.h"
#include <QObject>
#include <functional>

namespace trainer {
struct ArchiveResult {
    bool success = true;
    QList<HallOfFameEntry> entries;
    QString error;
};
struct ArchiveWriteResult { bool success = false; QString error; int revision = 0; };
QString validateArchiveEntry(const HallOfFameEntry&);
class HallOfFameRepository {
public:
    virtual ~HallOfFameRepository() = default;
    virtual ArchiveResult loadArchive() const = 0;
    virtual bool archiveEditable() const { return false; }
    virtual void saveArchiveAsync(const HallOfFameEntry&, QObject*, std::function<void(ArchiveWriteResult)> completed) {
        completed({false, "This archive is read-only."});
    }
};
class MockHallOfFameRepository final : public HallOfFameRepository {
public:
    ArchiveResult loadArchive() const override;
    bool archiveEditable() const override { return true; }
    void saveArchiveAsync(const HallOfFameEntry&, QObject*, std::function<void(ArchiveWriteResult)>) override;
    void setEmpty(bool empty) { empty_ = empty; }
    void failNextLoad() { fail_ = true; }
private:
    bool empty_ = false;
    mutable bool fail_ = false;
    mutable std::optional<QList<HallOfFameEntry>> entries_;
};
}
