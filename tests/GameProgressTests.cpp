#include "integrations/progress/Gen3Progress.h"
#include "integrations/progress/GameProgressService.h"
#include <QtTest>
#include <QtEndian>
#include <QFile>
#include <QTemporaryDir>
#include <QSemaphore>
#include <QCryptographicHash>
#include <atomic>

using namespace trainer;
namespace {
const QString EmeraldHash = "a9dec84dfe7f62ab2220bafaef7479da0929d066ece16a6885f6226db19085af";
void put16(QByteArray& b, int p, quint16 v) { qToLittleEndian(v, b.data() + p); }
void put32(QByteArray& b, int p, quint32 v) { qToLittleEndian(v, b.data() + p); }
// Original synthetic records. No game/BIOS/save bytes are distributed.
QByteArray slot(Gen3Edition edition, quint32 counter, int rotation, int mask, int caught) {
    QByteArray small(edition == Gen3Edition::Emerald ? 0xf2c : 0xf24, 0);
    QByteArray large(edition == Gen3Edition::Emerald ? 0x3d88 : 0x3d68, 0);
    QByteArray boxes(0x83d0, 0);
    for (int i = 0; i < caught; ++i) small[0x28 + i / 8] = char(quint8(small[0x28 + i / 8]) | (1 << (i % 8)));
    const int flags = edition == Gen3Edition::Emerald ? 0x1270 : 0xee0;
    const int flag = edition == Gen3Edition::Emerald ? 0x867 : 0x820;
    for (int i = 0; i < 8; ++i) if (mask & (1 << i)) {
        const int at = flags + (flag + i) / 8;
        large[at] = char(quint8(large[at]) | (1 << ((flag + i) % 8)));
    }
    QByteArray result(14 * 0x1000, char(0xff));
    for (int id = 0; id < 14; ++id) {
        const QByteArray data = id == 0 ? small : id < 5 ? large.mid((id - 1) * 0xf80, 0xf80) : boxes.mid((id - 5) * 0xf80, 0xf80);
        const int at = ((id + rotation) % 14) * 0x1000;
        result.replace(at, data.size(), data);
        quint32 sum = 0;
        for (int i = 0; i < data.size(); i += 4) sum += qFromLittleEndian<quint32>(data.constData() + i);
        put16(result, at + 0xff4, id); put16(result, at + 0xff6, quint16((sum >> 16) + sum));
        put32(result, at + 0xff8, 0x08012025); put32(result, at + 0xffc, counter);
    }
    return result;
}
QByteArray save(Gen3Edition edition, quint32 a = 10, quint32 b = 11) {
    return slot(edition, a, 5, 0x09, 12) + slot(edition, b, 11, 0xa5, 241) + QByteArray(4 * 0x1000, char(0xff));
}
void write(const QString& path, const QByteArray& bytes) {
    QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(bytes), bytes.size());
}
AdventureRegistration record(const QString& id = "test") {
    AdventureRegistration r; r.adventure.id = id; r.adventure.adapterId = "retroarch";
    r.integrationConfig = {{"core", "mgba"}}; return r;
}
}
class GameProgressTests : public QObject {
    Q_OBJECT
private slots:
    void readsBothEditions() {
        for (auto edition : {Gen3Edition::Emerald, Gen3Edition::FireRed}) {
            const auto bytes = save(edition); const auto result = readGen3Progress(bytes, edition);
            QCOMPARE(result.availability, ProgressAvailability::Available);
            QCOMPARE(result.badgeMask.value(), 0xa5); QCOMPARE(result.caught.value(), 241);
            QCOMPARE(result.badgeSet, edition == Gen3Edition::Emerald ? QString("hoenn") : QString("kanto"));
            auto zero = slot(edition, 5, 0, 0, 0) + QByteArray(18 * 0x1000, char(0xff));
            const auto empty = readGen3Progress(zero, edition);
            QCOMPARE(empty.availability, ProgressAvailability::Available);
            QCOMPARE(empty.badgeMask.value(), 0); QCOMPARE(empty.caught.value(), 0);
        }
    }
    void rejectsInvalidAndSelectsIntactSlot() {
        auto bytes = save(Gen3Edition::Emerald);
        bytes[14 * 0x1000 + 100] ^= 1;
        const auto recovered = readGen3Progress(bytes, Gen3Edition::Emerald);
        QCOMPARE(recovered.badgeMask.value(), 9); QCOMPARE(recovered.caught.value(), 12);
        bytes[100] ^= 1;
        QCOMPARE(readGen3Progress(bytes, Gen3Edition::Emerald).availability, ProgressAvailability::Unreadable);
        for (int size : {0, 0x10000, 0x1ffff, 0x20001, 0x40000})
            QCOMPARE(readGen3Progress(QByteArray(size, 0), Gen3Edition::Emerald).availability, ProgressAvailability::Unreadable);
        QCOMPARE(readGen3Progress(save(Gen3Edition::FireRed), Gen3Edition::Emerald).availability, ProgressAvailability::Unreadable);
    }
    void validatesSlotIdentityAndCounterWrap() {
        auto wrapped = readGen3Progress(save(Gen3Edition::Emerald, 0xffffffff, 0), Gen3Edition::Emerald);
        QCOMPARE(wrapped.caught.value(), 241);
        auto older = readGen3Progress(save(Gen3Edition::Emerald, 12, 11), Gen3Edition::Emerald);
        QCOMPARE(older.caught.value(), 12);
        QCOMPARE(readGen3Progress(save(Gen3Edition::Emerald, 10, 10), Gen3Edition::Emerald).availability, ProgressAvailability::Unreadable);
        auto bytes = save(Gen3Edition::Emerald);
        // Two duplicate IDs and a mismatched save counter must not be mixed into
        // a fictitious complete slot, even though their payload checksums pass.
        put16(bytes, 0xff4, qFromLittleEndian<quint16>(bytes.constData() + 0x1ff4));
        put32(bytes, 14 * 0x1000 + 0xffc, 55);
        QCOMPARE(readGen3Progress(bytes, Gen3Edition::Emerald).availability, ProgressAvailability::Unreadable);
    }
    void requiresFingerprintAndSafeSource() {
        QTemporaryDir dir; const auto path = dir.filePath("save.srm"); const auto bytes = save(Gen3Edition::Emerald); write(path, bytes);
        auto target = SaveTarget{"test", "", path, EmeraldHash, "verified-context", {}, true};
        const auto resolver = [&](const AdventureRegistration&) { return target; };
        const auto result = inspectGameProgress(record(), resolver);
        QCOMPARE(result.availability, ProgressAvailability::Available);
        QCOMPARE(result.saveRevision, QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()));
        QFile unchanged(path); QVERIFY(unchanged.open(QIODevice::ReadOnly)); QCOMPARE(unchanged.readAll(), bytes); unchanged.close();
        target.contentRevision = QString(64, 'a');
        QCOMPARE(inspectGameProgress(record(), resolver).availability, ProgressAvailability::Unsupported);
        target.contentRevision = EmeraldHash;
        auto hack = record(); hack.adventure.kind = AdventureKind::RomHack;
        QCOMPARE(inspectGameProgress(hack, resolver).availability, ProgressAvailability::Unsupported);
        auto ds = record(); ds.adventure.adapterId = "melonds";
        QCOMPARE(inspectGameProgress(ds, resolver).availability, ProgressAvailability::Unsupported);
        QVERIFY(QFile::remove(path));
        QCOMPARE(inspectGameProgress(record(), resolver).availability, ProgressAvailability::Missing);
        write(path, QByteArray(0x20000, 0));
        const auto bad = inspectGameProgress(record(), resolver);
        QCOMPARE(bad.availability, ProgressAvailability::Unreadable); QVERIFY(!bad.caught); QVERIFY(!bad.badgeMask);
        write(path, bytes);
        int calls = 0;
        const auto replaced = inspectGameProgress(record(), [&](const AdventureRegistration&) {
            auto changed = target; if (++calls == 2) changed.contextRevision = "another-setup"; return changed;
        });
        QCOMPARE(replaced.availability, ProgressAvailability::Unreadable); QVERIFY(!replaced.caught);
    }
    void discardsPreviousAdventureAndRefreshesReplacement() {
        QTemporaryDir dir; const auto path = dir.filePath("save.srm"); write(path, save(Gen3Edition::Emerald));
        QSemaphore entered, release;
        std::atomic_bool blockNext{true};
        GameProgressService service([&](const AdventureRegistration& r) {
            if (r.adventure.id == "old" && blockNext.exchange(false)) { entered.release(); release.acquire(); }
            return SaveTarget{r.adventure.id, "", path, EmeraldHash, "context", {}, true};
        });
        service.refresh(record("old"));
        const bool started = entered.tryAcquire(1, 2000);
        if (!started) { release.release(); QVERIFY(started); }
        service.refresh(record("new")); release.release();
        QTRY_COMPARE(service.snapshot().availability, ProgressAvailability::Available);
        QCOMPARE(service.adventureId(), QString("new")); QCOMPARE(service.snapshot().caught.value(), 241);
        write(path, slot(Gen3Edition::Emerald, 1, 0, 0, 0) + QByteArray(18 * 0x1000, char(0xff)));
        service.refresh(record("new")); QTRY_COMPARE(service.snapshot().availability, ProgressAvailability::Available);
        QCOMPARE(service.snapshot().caught.value(), 0); QCOMPARE(service.snapshot().badgeMask.value(), 0);
        blockNext = true; service.refresh(record("old"));
        const bool blocked = entered.tryAcquire(1, 2000);
        if (!blocked) { release.release(); QVERIFY(blocked); }
        service.invalidate(); release.release(); QTest::qWait(100);
        QVERIFY(service.adventureId().isEmpty()); QVERIFY(!service.snapshot().caught);
    }
    void externalExamples() {
        const auto folder = qEnvironmentVariable("TRAINEROS_PROGRESS_SAMPLE_DIR");
        if (folder.isEmpty()) QSKIP("Optional private examples were not supplied; synthetic format cases run above.");
        for (const auto& name : {QString("emerald.sav"), QString("firered.sav")}) {
            QFile file(QDir(folder).filePath(name)); QVERIFY(file.open(QIODevice::ReadOnly));
            const auto result = readGen3Progress(file.readAll(), name.startsWith("emerald") ? Gen3Edition::Emerald : Gen3Edition::FireRed);
            QCOMPARE(result.availability, ProgressAvailability::Available); QCOMPARE(result.badgeMask.value(), 255); QCOMPARE(result.caught.value(), 386);
        }
    }
};
QTEST_GUILESS_MAIN(GameProgressTests)
#include "GameProgressTests.moc"
