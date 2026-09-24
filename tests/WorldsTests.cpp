#include "core/navigation/ShellController.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include <QtTest>
#include <algorithm>

using namespace trainer;
namespace {
class MutableLibrary final : public LibraryRepository {
public:
    MockLibraryRepository sample;
    QList<World> worldData = sample.worlds();
    QList<Adventure> adventureData = sample.adventures();
    QList<ResumePoint> points = sample.resumePoints();
    QVariantMap media;
    QVariantMap artwork(const QString&) const override { return media; }
    QList<World> worlds() const override { return worldData; }
    QList<Adventure> adventures() const override { return adventureData; }
    QList<ResumePoint> resumePoints() const override { return points; }
    HomeSnapshot home() const override { return sample.home(); }
};
class RecordingAdapter final : public AdventureAdapter {
public:
    MockAdventureAdapter mock;
    int launches = 0, resumes = 0;
    QString adventureId, pointId;
    QString id() const override { return mock.id(); }
    AdventureCapabilities capabilities(const Adventure& a) const override { return mock.capabilities(a); }
    ResumeAvailability resumeAvailability(const Adventure& a, const ResumePoint& p) const override { return mock.resumeAvailability(a, p); }
    AdventureResult launch(const Adventure& a) override { ++launches; adventureId = a.id; return mock.launch(a); }
    AdventureResult resume(const Adventure& a, const ResumePoint& point) override {
        ++resumes; adventureId = a.id; pointId = point.id; return mock.resume(a, point);
    }
};
void tap(WorldsController& worlds, Action action, int count = 1) {
    for (int i = 0; i < count; ++i) worlds.dispatch(action);
}
}
class WorldsTests : public QObject {
    Q_OBJECT
private slots:
    void randomWorldPairsStayStableWhileBrowsing() {
        MutableLibrary library;RecordingAdapter adapter;library.adventureData.clear();
        library.worldData={{"kanto","Kanto",{}},{"fiore","Fiore",{}},{"almia","Almia",{}},
            {"oblivia","Oblivia",{}},{"orre","Orre",{}},{"custom","Custom",{}}};
        WorldsController worlds(library,adapter);const auto initial=worlds.regionTiles();QCOMPARE(initial.size(),4);
        QSet<QString> ids;
        for(const auto& tile:initial) {
            const auto members=tile.toMap()["members"].toList();
            for(const auto& member:members) {
                const auto id=member.toMap()["id"].toString();QVERIFY(!ids.contains(id));ids.insert(id);
                if(id=="kanto" || id=="custom")QCOMPARE(members.size(),1);
            }
        }
        QCOMPARE(ids.size(),6);
        for(int i=0;i<12;++i){tap(worlds,Action::Right);tap(worlds,Action::Down);worlds.refresh();QCOMPARE(worlds.regionTiles(),initial);}
    }
    void diagonalPairsKeepIndependentWorldRoutes() {
        MutableLibrary library;RecordingAdapter adapter;
        library.worldData={{"kanto","Kanto",{}},{"fiore","Fiore",{}},{"almia","Almia",{}},{"other","Custom",{}}};
        library.adventureData.clear();WorldsController worlds(library,adapter);
        QCOMPARE(worlds.regionTiles().size(),3);
        tap(worlds,Action::Right);QCOMPARE(worlds.region()["id"].toString(),"fiore");
        tap(worlds,Action::Down);QCOMPARE(worlds.region()["id"].toString(),"almia");QCOMPARE(worlds.regionTileIndex(),1);
        tap(worlds,Action::Confirm);QCOMPARE(worlds.route(),"adventures");QCOMPARE(worlds.region()["id"].toString(),"almia");
        tap(worlds,Action::Back);tap(worlds,Action::Up);QCOMPARE(worlds.region()["id"].toString(),"fiore");
        tap(worlds,Action::Right);QCOMPARE(worlds.region()["id"].toString(),"other");
        for(int i=0;i<5;++i){Adventure a;a.id=QString::number(i);a.worldId="fiore";library.adventureData.append(a);}
        worlds.refresh();QCOMPARE(worlds.regionTiles().size(),4); // Large/custom Worlds are never silently merged.
        QCOMPARE(worlds.region()["id"].toString(),"other");
        library.adventureData.clear();
        std::reverse(library.worldData.begin(),library.worldData.end());
        worlds.refresh();QCOMPARE(worlds.regionTiles().size(),3);
        tap(worlds,Action::Right);QCOMPARE(worlds.region()["id"].toString(),"almia");
        tap(worlds,Action::Down);QCOMPARE(worlds.region()["id"].toString(),"fiore");
    }
    void wheelUsesEditionMetadataWithoutInventingMissingFields() {
        MutableLibrary library;
        library.media={{"marquee","file:///edition-logo.png"},{"image","file:///edition-image.png"},
            {"releasedate","20040916T000000"},{"desc","Full edition description"},{"players","1"}};
        RecordingAdapter adapter;WorldsController worlds(library,adapter);worlds.activate(2);
        QCOMPARE(worlds.adventures()[0].toMap()["logo"],"file:///edition-logo.png");
        QCOMPARE(worlds.detail()["screenshot"],"file:///edition-image.png");
        QCOMPARE(worlds.detail()["year"],"2004");QCOMPARE(worlds.detail()["description"],"Full edition description");
        QCOMPARE(worlds.detail()["synopsis"],"Full edition description");
        library.media.clear();worlds.refresh();
        QVERIFY(worlds.detail()["screenshot"].toString().isEmpty());
        QVERIFY(worlds.detail()["developer"].toString().isEmpty());
        QVERIFY(worlds.detail()["synopsis"].toString().isEmpty()); // Never substitute import/setup instructions for a synopsis.
    }
    void collectionSearchFiltersAndIdentity() {
        MutableLibrary library;
        library.adventureData.clear();
        for (int i = 0; i < 700; ++i) {
            Adventure a; a.id = QString::number(i); a.worldId = "hoenn";
            a.title = QString::fromUtf8("Pokémon Trail %1").arg(i); a.platformId = "gba";
            a.variant = i % 2 ? "Beta 2" : "English"; a.collectionOnly = i >= 650;
            library.adventureData.append(a);
        }
        library.adventureData[42].additionalWorldIds.append("kanto");
        RecordingAdapter adapter; WorldsController worlds(library, adapter);
        worlds.activate(2);
        tap(worlds, Action::Right, 90); QCOMPARE(worlds.adventureIndex(), 699);
        tap(worlds, Action::Left); QCOMPARE(worlds.adventureIndex(), 691);
        worlds.applySearch("  POKEMON  42 GBA ");
        QCOMPARE(worlds.query(), "POKEMON 42 GBA");
        QCOMPARE(worlds.adventures().size(), 17);
        QCOMPARE(worlds.detail()["id"].toString(), "42");
        tap(worlds, Action::ToggleContinue); QCOMPARE(worlds.filterLabel(), "Linked");
        for (const auto& row : worlds.adventures()) QVERIFY(!row.toMap()["missing"].toBool());
        // Search matches text tokens and variant metadata, not inferred ownership.
        worlds.applySearch("beta 2 649"); QCOMPARE(worlds.adventures().size(), 1);
        QCOMPARE(worlds.detail()["id"].toString(), "649");
        tap(worlds, Action::ToggleContinue); QCOMPARE(worlds.filterLabel(), "Missing");
        QVERIFY(worlds.adventures().isEmpty()); QCOMPARE(worlds.focusIndex(), 0);
        tap(worlds, Action::Confirm); QCOMPARE(worlds.route(), "regions");
        worlds.activate(2); worlds.applySearch("699");
        QCOMPARE(worlds.adventures().size(), 1); worlds.activate(0);
        const auto state = worlds.navigationState();
        WorldsController restored(library, adapter); restored.restoreNavigation(state);
        QCOMPARE(restored.route(), "detail"); QCOMPARE(restored.detail()["id"].toString(), "699");
        QCOMPARE(restored.filterLabel(), "Missing"); QCOMPARE(restored.query(), "699");
        tap(restored, Action::Back, 2); restored.activate(0);
        QCOMPARE(restored.query(), ""); QCOMPARE(restored.filterLabel(), "All");
        QCOMPARE(restored.detail()["id"].toString(), "42"); // Additional region relationship.
        tap(restored, Action::Back); restored.activate(2); restored.applySearch("");
        QCOMPARE(restored.adventures().size(), 50);
        tap(restored, Action::ToggleContinue); QCOMPARE(restored.adventures().size(), 700);
        QCOMPARE(restored.detail()["id"].toString(), "699");
        library.adventureData.removeLast(); restored.refresh();
        QCOMPARE(restored.detail()["id"].toString(), "0");
        QCOMPARE(adapter.launches, 0); QCOMPARE(adapter.resumes, 0);
    }
    void regionGroupingAndUnknownProgress() {
        MutableLibrary library;
        RecordingAdapter adapter;
        WorldsController worlds(library, adapter);
        QCOMPARE(worlds.regions().size(), 9);
        QCOMPARE(worlds.regions()[2].toMap()["count"].toInt(), 5);
        QCOMPARE(worlds.regions()[2].toMap()["status"].toString(), "In progress");
        worlds.activate(2);
        QCOMPARE(worlds.region()["name"].toString(), "Hoenn");
        QCOMPARE(worlds.adventures().size(), 5);
        QCOMPARE(worlds.adventures()[0].toMap()["kind"].toString(), "Original");
        QCOMPARE(worlds.adventures()[3].toMap()["kind"].toString(), "Remake");
        QCOMPARE(worlds.adventures()[4].toMap()["kind"].toString(), "ROM hack");
        worlds.activate(3);
        QCOMPARE(worlds.detail()["badges"].toString(), QString::fromUtf8("—"));
        QCOMPARE(worlds.detail()["status"].toString(), "Not recorded");
        tap(worlds, Action::Back, 2); worlds.activate(0); worlds.activate(0);
        QCOMPARE(worlds.detail()["badges"].toString(), "0"); // Known zero is different from unknown.
    }
    void layeredNavigationAndPerWorldSelection() {
        MutableLibrary library;
        RecordingAdapter adapter;
        WorldsController worlds(library, adapter);
        tap(worlds, Action::Left); tap(worlds, Action::Up);
        QCOMPARE(worlds.regionIndex(), 0);
        tap(worlds, Action::Right, 3);
        QCOMPARE(worlds.regionIndex(), 2); // Stop at grid edge.
        tap(worlds, Action::Confirm); tap(worlds, Action::Down, 4);
        QCOMPARE(worlds.adventureIndex(), 4);
        tap(worlds, Action::Down);
        QCOMPARE(worlds.focusIndex(), 0);
        tap(worlds, Action::Up); // Circular in both directions; B returns to regions.
        QCOMPARE(worlds.focusIndex(), 4);
        tap(worlds, Action::Confirm);
        QCOMPARE(worlds.route(), "detail");
        QCOMPARE(worlds.detail()["id"].toString(), "emerald-trails-demo");
        tap(worlds, Action::Back);
        QCOMPARE(worlds.route(), "adventures");
        QCOMPARE(worlds.focusIndex(), 4);
        tap(worlds, Action::Back); tap(worlds, Action::Left); tap(worlds, Action::Confirm);
        QCOMPARE(worlds.region()["name"].toString(), "Johto");
        tap(worlds, Action::Back); tap(worlds, Action::Right); tap(worlds, Action::Confirm);
        QCOMPARE(worlds.focusIndex(), 4);
        QCOMPARE(worlds.detail()["id"].toString(), "emerald-trails-demo");
    }
    void capabilitiesAndLatestResume() {
        MutableLibrary library;
        auto newer = library.points.at(1);
        newer.id = "emerald-newer"; newer.source.sourceId = newer.id;
        newer.savedAt = QDateTime::fromString("2026-09-07T01:00:00Z", Qt::ISODate);
        newer.location = "New trail";
        library.points.prepend(newer);
        RecordingAdapter adapter;
        WorldsController worlds(library, adapter);
        QSignalSpy messages(&worlds, &WorldsController::messageRequested);
        worlds.activate(2); worlds.activate(0); worlds.activate(0);
        QCOMPARE(adapter.resumes, 1);
        QCOMPARE(adapter.pointId, "emerald-newer");
        QCOMPARE(adapter.adventureId, "emerald-demo");
        QCOMPARE(adapter.launches, 0);
        QCOMPARE(worlds.route(), "detail");
        QVERIFY(messages.last().first().toString().contains("No Adventure was launched"));
        tap(worlds, Action::Back); worlds.activate(1); // Ruby only supports launch.
        QCOMPARE(worlds.actions().size(), 2);
        QCOMPARE(worlds.actions()[0].toMap()["id"].toString(), "launch");
        worlds.activate(0);
        QCOMPARE(adapter.launches, 1);
        QCOMPARE(adapter.resumes, 1);
        QCOMPARE(adapter.adventureId, "ruby-demo");
    }
    void unavailableActionsAndRetry() {
        MutableLibrary library;
        RecordingAdapter adapter;
        WorldsController worlds(library, adapter);
        QSignalSpy messages(&worlds, &WorldsController::messageRequested);
        worlds.activate(2); worlds.activate(2); // Sapphire needs setup.
        QCOMPARE(worlds.focusIndex(), 1);
        QVERIFY(!worlds.actions()[0].toMap()["enabled"].toBool());
        tap(worlds, Action::Left);
        QCOMPARE(worlds.focusIndex(), 1);
        worlds.activate(0); // A pointer/API call must not bypass disabled state.
        QCOMPARE(adapter.launches, 0);
        QCOMPARE(worlds.route(), "detail");
        tap(worlds, Action::Confirm);
        QCOMPARE(worlds.route(), "adventures");
        worlds.activate(1);
        adapter.mock.failNextLaunch(); worlds.activate(0);
        QVERIFY(messages.last().first().toString().contains("couldn't open"));
        QCOMPARE(worlds.route(), "detail");
        QCOMPARE(worlds.focusIndex(), 0);
        worlds.activate(0);
        QCOMPARE(adapter.launches, 2);
        QVERIFY(messages.last().first().toString().contains("Demo launch ready"));
    }
    void refreshKeepsIdentityAndRejectsRemovedRecords() {
        MutableLibrary library;
        RecordingAdapter adapter;
        WorldsController worlds(library, adapter);
        QSignalSpy messages(&worlds, &WorldsController::messageRequested);
        worlds.activate(2); worlds.activate(0);
        std::reverse(library.adventureData.begin(), library.adventureData.end());
        worlds.refresh();
        QCOMPARE(worlds.detail()["id"].toString(), "emerald-demo");
        QCOMPARE(worlds.adventureIndex(), 4);
        library.adventureData.removeLast(); // Remove selected record without refreshing the visible cache first.
        worlds.activate(0);
        QCOMPARE(adapter.resumes, 0);
        QCOMPARE(adapter.launches, 0);
        QCOMPARE(worlds.route(), "adventures");
        QCOMPARE(worlds.detail()["id"].toString(), "emerald-trails-demo");
        QVERIFY(messages.last().first().toString().contains("no longer available"));
        library.worldData.clear(); worlds.refresh();
        QCOMPARE(worlds.route(), "regions");
        QVERIFY(worlds.regions().isEmpty());
    }
    void emptyAndUnmatchedAdapter() {
        MutableLibrary library;
        RecordingAdapter adapter;
        WorldsController worlds(library, adapter);
        worlds.activate(3); // Sinnoh has no configured Adventures.
        QCOMPARE(worlds.route(), "adventures");
        QVERIFY(worlds.adventures().isEmpty());
        QCOMPARE(worlds.focusIndex(), 0);
        tap(worlds, Action::Confirm);
        QCOMPARE(worlds.route(), "regions");
        QCOMPARE(worlds.regionIndex(), 3);
        library.adventureData[0].adapterId = "unknown-provider";
        worlds.refresh(); worlds.activate(2); worlds.activate(0);
        QCOMPARE(worlds.actions().size(), 2);
        QCOMPARE(worlds.focusIndex(), 1);
        QVERIFY(!worlds.actions().first().toMap()["enabled"].toBool());
        library.worldData.clear(); worlds.refresh();
        QSignalSpy home(&worlds, &WorldsController::homeRequested);
        tap(worlds, Action::Back);
        QCOMPARE(home.size(), 0); // B on a primary page remains a no-op.
        tap(worlds, Action::Confirm);
        QCOMPARE(home.size(), 1);
    }
    void shellRestoresDetailAcrossGlobalLayers() {
        MutableLibrary library;
        MockTrainerRepository profiles;
        RecordingAdapter adapter;
        DevelopmentPlatformService platform;
        MockPokedexRepository dex;
        MockHallOfFameRepository shellArchive;
        MockAchievementProvider shellAchievements;
        ShellController shell(library, profiles, adapter, platform, dex, dex, shellArchive, shellAchievements);
        shell.goToPage(1); shell.activate(2); shell.activate(0);
        shell.dispatch(Action::Right); // Start instead of Continue.
        QCOMPARE(shell.focusIndex(), 1);
        shell.dispatch(Action::SystemMenu); shell.dispatch(Action::Confirm);
        shell.dispatch(Action::Back); shell.dispatch(Action::Back);
        QCOMPARE(shell.worlds()->route(), "detail");
        QCOMPARE(shell.focusIndex(), 1);
        shell.dispatch(Action::NextPage); shell.dispatch(Action::PreviousPage);
        QCOMPARE(shell.worlds()->detail()["id"].toString(), "emerald-demo");
        QCOMPARE(shell.focusIndex(), 1);
        shell.dispatch(Action::Confirm);
        QVERIFY(!shell.notice().isEmpty());
        shell.dispatch(Action::Back);
        QCOMPARE(shell.worlds()->route(), "detail");
        shell.dispatch(Action::Back);
        QCOMPARE(shell.worlds()->route(), "adventures");
        shell.dispatch(Action::Back);
        QCOMPARE(shell.worlds()->route(), "regions");
        QCOMPARE(shell.focusIndex(), 2);
    }
};
QTEST_GUILESS_MAIN(WorldsTests)
#include "WorldsTests.moc"
