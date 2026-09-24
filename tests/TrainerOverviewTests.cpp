#include "features/trainer/TrainerController.h"
#include "core/repository/OfflinePokedex.h"
#include "core/storage/LocalStateStore.h"
#include <QtTest>
#include <QTemporaryDir>
#include <limits>

using namespace trainer;
namespace {
class Library final : public LibraryRepository {
public:
    QList<Adventure> rows;
    QHash<QString,qint64> seconds;
    QList<World> worlds() const override { return {}; }
    QList<Adventure> adventures() const override { return rows; }
    QList<ResumePoint> resumePoints() const override { return {}; }
    HomeSnapshot home() const override { return {}; }
    std::optional<qint64> recordedSeconds(const QString& id) const override {
        return seconds.contains(id) ? std::optional<qint64>(seconds.value(id)) : std::nullopt;
    }
};
}
class TrainerOverviewTests final : public QObject {
    Q_OBJECT
private slots:
    void completeChoiceSearchCancelAndUnavailableReference() {
        OfflinePokedex reference; SpeciesPicker picker; picker.setReference(&reference);
        QSignalSpy chosen(&picker, &SpeciesPicker::selected), search(&picker, &SpeciesPicker::searchRequested);
        picker.begin("pecharunt"); QCOMPARE(picker.entries().size(), 1025); QCOMPARE(picker.focusIndex(), 1024);
        picker.dispatch(Action::Right); QCOMPARE(picker.focusIndex(), 1024);
        picker.dispatch(Action::Left); QCOMPARE(picker.focusIndex(), 1016);
        picker.applySearch("Flabebe"); QCOMPARE(picker.entries().size(), 1);
        picker.dispatch(Action::Confirm); QCOMPARE(chosen.size(), 1); QCOMPARE(chosen[0][0].toString(), "flabebe");
        picker.begin("flabebe"); picker.applySearch("#1025"); QCOMPARE(picker.entries()[0].toMap()["name"].toString(), "Pecharunt");
        picker.dispatch(Action::Back); QCOMPARE(chosen.size(), 1);
        picker.begin("flabebe"); picker.applySearch("zzzz"); QVERIFY(picker.entries().isEmpty());
        picker.dispatch(Action::Confirm); QCOMPARE(search.size(), 1);
        picker.dispatch(Action::ToggleContinue); QCOMPARE(chosen.size(), 2); QVERIFY(chosen[1][0].toString().isEmpty());
        MockPokedexRepository failed; failed.failNextLoad(); picker.setReference(&failed); picker.begin("eevee");
        QVERIFY(!picker.error().isEmpty()); QVERIFY(picker.entries().isEmpty()); picker.dispatch(Action::Back); QCOMPARE(chosen.size(), 2);
    }
    void aggregatesExcludeMissingContentAndUnknownFacts() {
        Library library; Adventure one; one.id="one"; one.worldId="kanto"; one.additionalWorldIds={"johto","kanto"};
        Adventure two=one; two.id="two"; two.worldId="johto";
        Adventure missing=one; missing.id="missing"; missing.worldId="paldea"; missing.collectionOnly=true;
        library.rows={one,two,one,missing}; library.seconds={{"one",120},{"missing",9999}};
        MockPokedexRepository journal; MockHallOfFameRepository archive; archive.setEmpty(true);
        const auto reference=journal.load();
        auto summary=trainerOverview(library,reference,journal,archive);
        QCOMPARE(summary.adventures,2); QCOMPARE(summary.worlds,2); QCOMPARE(summary.recordedSeconds.value(),120);
        QVERIFY(!summary.seen); QVERIFY(!summary.caught); QCOMPARE(summary.favorites.value(),3); QCOMPARE(summary.memories.value(),0);
        archive.failNextLoad(); summary=trainerOverview(library,{false,{}, {},"Unavailable"},journal,archive);
        QVERIFY(!summary.seen); QVERIFY(!summary.caught); QVERIFY(!summary.favorites); QVERIFY(!summary.memories);
        library.seconds.clear(); QVERIFY(!trainerOverview(library,reference,journal,archive).recordedSeconds);
        library.seconds={{"one",std::numeric_limits<qint64>::max()},{"two",100}};
        QCOMPARE(trainerOverview(library,reference,journal,archive).recordedSeconds.value(),std::numeric_limits<qint64>::max());
    }
    void personalJournalAndFullProfileChoiceRemainIndependent() {
        QTemporaryDir directory; QVERIFY(directory.isValid()); OfflinePokedex guide;
        {
            LocalStateStore store(directory.path(),nullptr,"user-library-v1"); store.open(); QTRY_VERIFY(store.ready());
            TrainerController trainer(store); trainer.configure(&store,&guide,&store,&store); trainer.refreshOverview();
            QCOMPARE(trainer.overview()[2].toMap()["label"].toString(),"FAVORITE MARKS");
            QCOMPARE(trainer.overview()[2].toMap()["value"].toString(),"0");
            trainer.beginEdit(); trainer.setDraftName("ERI"); trainer.activate(2); trainer.picker()->applySearch("Flabebe"); trainer.dispatch(Action::Confirm);
            QCOMPARE(trainer.draftFavorite(),QString::fromUtf8("Flabébé")); QVERIFY(!store.load());
            trainer.activate(3); QTRY_VERIFY(!trainer.saving()); QVERIFY(trainer.exists());
            QVERIFY(!store.progress("flabebe").seen); QVERIFY(!store.progress("flabebe").favorite);
            PokedexProgress record; record.seen=true; record.caught=true; bool done=false;
            store.saveRecordAsync("flabebe",record,this,[&](PokedexWriteResult result){QVERIFY(result.success);done=true;}); QTRY_VERIFY(done);
            trainer.refreshOverview(); QCOMPARE(trainer.overview()[2].toMap()["value"].toString(),"0");
            trainer.beginEdit(); trainer.activate(2); trainer.dispatch(Action::ToggleContinue); trainer.cancel();
            QCOMPARE(store.load()->favoritePokemonId,"flabebe");
        }
        LocalStateStore reopened(directory.path(),nullptr,"user-library-v1"); reopened.open(); QTRY_VERIFY(reopened.ready());
        QCOMPARE(reopened.load()->favoritePokemonId,"flabebe"); QVERIFY(reopened.progress("flabebe").caught.value());
        QVERIFY(!reopened.progress("flabebe").favorite);
    }
};
QTEST_GUILESS_MAIN(TrainerOverviewTests)
#include "TrainerOverviewTests.moc"
