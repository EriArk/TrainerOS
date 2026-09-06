#pragma once
#include "core/model/Models.h"

namespace trainer {
struct ArchiveResult {
    bool success = true;
    QList<HallOfFameEntry> entries;
    QString error;
};
class HallOfFameRepository {
public:
    virtual ~HallOfFameRepository() = default;
    virtual ArchiveResult load() = 0;
};
class MockHallOfFameRepository final : public HallOfFameRepository {
public:
    ArchiveResult load() override;
    void setEmpty(bool empty) { empty_ = empty; }
    void failNextLoad() { fail_ = true; }
private:
    bool empty_ = false, fail_ = false;
};
}
