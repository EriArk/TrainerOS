#include "integrations/progress/Gen3Progress.h"
#include "integrations/progress/EmeraldParty.h"
#include "integrations/progress/GameProgressService.h"
#include "features/home/BadgeAssets.h"
#include <QImage>
#include <QtTest>
#include <QtEndian>
#include <QFile>
#include <QTemporaryDir>
#include <QSemaphore>
#include <QCryptographicHash>
#include <atomic>
#include <algorithm>

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
QByteArray pokemonFixture(quint32 personality = 0, int species = 25, bool egg = false, bool party = true) {
    QByteArray bytes(party ? 100 : 80, 0), growth(12,0), attacks(12,0), ev(12,0), misc(12,0);
    put32(bytes,0,personality); put32(bytes,4,0x12345678);
    bytes[8]=char(0xbb); bytes[9]=char(0xff); bytes[18]=2; bytes[19]=egg ? 6 : 2;
    put16(growth,0,species); put16(growth,2,215); put32(growth,4,125); growth[8]=3; // Charcoal; Pikachu level 5, three PP Ups.
    put16(attacks,0,33); attacks[8]=40; // Tackle, maximum 56 PP in Emerald.
    put32(misc,4,0x3fffffffu | (egg ? 0x40000000u : 0));
    std::array<QByteArray,4> logical{growth,attacks,ev,misc};
    std::array<int,4> order{0,1,2,3};
    for(quint32 i=0;i<personality%24;++i) std::next_permutation(order.begin(),order.end());
    QByteArray clear;for(auto index:order) clear+=logical[index];
    quint16 sum=0;for(int i=0;i<48;i+=2)sum=quint16(sum+qFromLittleEndian<quint16>(clear.constData()+i));
    put16(bytes,28,sum);
    for(int i=0;i<48;i+=4)put32(bytes,32+i,qFromLittleEndian<quint32>(clear.constData()+i)^personality^0x12345678u);
    if(party) { bytes[84]=5; put16(bytes,86,0);for(int i=0;i<6;++i)put16(bytes,88+2*i,20+i); }
    return bytes;
}
}
class GameProgressTests : public QObject {
    Q_OBJECT
private slots:
    void emeraldPartyDecryptsEveryPermutationAndPreservesBoxPositions() {
        for(int permutation=0;permutation<24;++permutation) {
            QByteArray world(0x3d88,0),boxes(0x83d0,0);world[0x234]=1;boxes[0]=13;
            world.replace(0x238,100,pokemonFixture(permutation));
            boxes.replace(4+(13*30+29)*80,80,pokemonFixture(permutation,25,false,false));
            boxes[0x8344+13*9]=char(0xbb);boxes[0x8344+13*9+1]=char(0xff);
            const auto result=readEmeraldParty(world,boxes);
            QVERIFY(result.error.isEmpty());QCOMPARE(result.party.size(),6);QCOMPARE(result.boxes.size(),14);
            QCOMPARE(result.currentBox,13);QCOMPARE(result.boxes[13].name,"A");
            const auto p=result.party[0];QCOMPARE(p.kind,PokemonSlotKind::Known);QCOMPARE(p.speciesId,"pikachu");
            QCOMPARE(p.level,5);QCOMPARE(p.hp,std::optional<int>(0));QCOMPARE(p.condition,"Fainted");
            QCOMPARE(p.moves[0].pp,40);QCOMPARE(p.moves[0].maxPp,56);
            QCOMPARE(p.item,"Charcoal");
            QCOMPARE(p.stats[3],24);QCOMPARE(p.stats[5],23);
            QCOMPARE(result.party[1].kind,PokemonSlotKind::Empty);
            QCOMPARE(result.boxes[13].members[28].kind,PokemonSlotKind::Empty);
            const auto boxed=result.boxes[13].members[29];QCOMPARE(boxed.kind,PokemonSlotKind::Known);
            QVERIFY(!boxed.hp.has_value());QCOMPARE(boxed.condition,"Stored");
            QCOMPARE(boxed.stats[0],20); // ((2*35+31)*5/100)+5+10.
        }
    }
    void emeraldPartyContainsDamageAndHidesEggSpecies() {
        QByteArray world(0x3d88,0),boxes(0x83d0,0);world[0x234]=3;
        world.replace(0x238,100,pokemonFixture(2,25,true));
        auto damaged=pokemonFixture();damaged[35]=char(quint8(damaged[35])^1);
        world.replace(0x238+100,100,damaged);world.replace(0x238+200,100,pokemonFixture());
        const auto result=readEmeraldParty(world,boxes);QVERIFY(result.error.isEmpty());
        QCOMPARE(result.party[0].kind,PokemonSlotKind::Egg);QVERIFY(result.party[0].speciesId.isEmpty());
        QCOMPARE(result.party[1].kind,PokemonSlotKind::Unreadable);QCOMPARE(result.party[2].kind,PokemonSlotKind::Known);
        QVERIFY(!readEmeraldParty(world.left(0x3000),boxes).error.isEmpty());
        QVERIFY(!readEmeraldParty(world,boxes+QByteArray(1,0)).error.isEmpty());
        world[0x234]=7;QVERIFY(!readEmeraldParty(world,boxes).error.isEmpty());
        world[0x234]=0;boxes[0]=14;QVERIFY(!readEmeraldParty(world,boxes).error.isEmpty());
    }
    void privateEmeraldPartyExample() {
        const auto path=qEnvironmentVariable("TRAINEROS_EMERALD_SAMPLE");if(path.isEmpty())QSKIP("Private save is optional");
        QFile f(path);QVERIFY(f.open(QIODevice::ReadOnly));const auto bytes=f.readAll();
        const auto p=readGen3Progress(bytes,Gen3Edition::Emerald);
        QCOMPARE(p.availability,ProgressAvailability::Available);QVERIFY(p.party.has_value());QVERIFY(p.party->error.isEmpty());
        int known=0,bad=0;
        for(const auto& mon:p.party->party) {known+=mon.kind==PokemonSlotKind::Known;bad+=mon.kind==PokemonSlotKind::Unreadable;qInfo()<<mon.speciesName<<mon.level<<mon.hp.value_or(-1);}
        for(const auto& box:p.party->boxes)for(const auto& mon:box.members)bad+=mon.kind==PokemonSlotKind::Unreadable;
        QVERIFY(known>0);QCOMPARE(bad,0);
    }
    void badgeArtwork() {
        const auto kanto = BadgeAssets::entries("kanto-frlg", 0x21);
        QCOMPARE(kanto.size(), 8);
        QStringList ids;
        for (const auto& row : kanto) ids.append(row.toMap()["id"].toString());
        QCOMPARE(ids, QStringList({"boulder", "cascade", "thunder", "rainbow", "soul", "marsh", "volcano", "earth"}));
        QCOMPARE(kanto[0].toMap()["state"], "earned");
        QCOMPARE(kanto[4].toMap()["state"], "unearned");
        QCOMPARE(kanto[5].toMap()["state"], "earned");
        for (const auto& set : {QString("kanto-frlg"), QString("hoenn-rse")}) {
            for (const auto& row : BadgeAssets::entries(set, 255)) {
                const auto url = row.toMap()["image"].toString();
                QVERIFY(url.startsWith("qrc:/badges/"));
                QImage image(url.mid(3));
                QCOMPARE(image.size(), QSize(192, 192));
                QVERIFY(image.hasAlphaChannel());
                QVERIFY(image.pixelColor(0, 0).alpha() == 0);
                int pixels = 0;
                for (int y = 0; y < image.height(); ++y)
                    for (int x = 0; x < image.width(); ++x) pixels += image.pixelColor(x, y).alpha() > 128;
                QVERIFY(pixels > 2500 && pixels < 31000); // Not an empty image, clipped sheet or opaque square.
            }
        }
        const auto zero = BadgeAssets::entries("hoenn-rse", 0);
        QCOMPARE(zero.first().toMap()["id"], "stone");
        QCOMPARE(zero.last().toMap()["id"], "rain");
        for (const auto& row : zero) QCOMPARE(row.toMap()["state"], "unearned");
        for (const auto& row : BadgeAssets::entries("kanto-frlg", std::nullopt)) {
            QCOMPARE(row.toMap()["state"], "unknown");
            QVERIFY(row.toMap()["image"].toString().isEmpty());
        }
        for (const auto& unsupported : {"kanto", "hoenn", "unova-b2w2", "paldea", ""})
            QVERIFY(BadgeAssets::entries(unsupported, 255).isEmpty());
    }
    void readsBothEditions() {
        for (auto edition : {Gen3Edition::Emerald, Gen3Edition::FireRed}) {
            const auto bytes = save(edition); const auto result = readGen3Progress(bytes, edition);
            QCOMPARE(result.availability, ProgressAvailability::Available);
            QCOMPARE(result.badgeMask.value(), 0xa5); QCOMPARE(result.caught.value(), 241);
            QCOMPARE(result.badgeSet, edition == Gen3Edition::Emerald ? QString("hoenn-rse") : QString("kanto-frlg"));
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
