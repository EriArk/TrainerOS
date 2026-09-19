#include "core/navigation/ShellController.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include <QtTest>

using namespace trainer;
namespace {
class MutableArchive : public HallOfFameRepository {
public:
    MockHallOfFameRepository sample;
    ArchiveResult value = sample.loadArchive();
    ArchiveResult loadArchive() const override { return value; }
};
class StaleProvider : public AchievementProvider {
public:
    MockAchievementProvider sample;
    AchievementContext claimedContext = sample.context();
    QString wrongSet;
    AchievementContext context() const override { return claimedContext; }
    QList<AchievementSet> sets() const override { return sample.sets(); }
    AchievementSnapshot snapshot(const QString& id) const override {
        auto value = sample.snapshot(id);
        if (!wrongSet.isEmpty()) value.setId = wrongSet;
        return value;
    }
    void refresh(const QString&) override {}
};
void achievements(HallOfFameController& hall, int index = 0) {
    hall.switchFace();
    hall.activate(index);
}
}
class HallOfFameTests : public QObject {
    Q_OBJECT
private slots:
    void independentFacesPersistAndReconcileBackgroundChanges() {
        MutableArchive archive;
        MockAchievementProvider provider;
        provider.holdRequests(true);
        HallOfFameController hall(archive, provider);
        hall.activate(1);
        const auto memory = hall.detail()["title"];
        hall.switchFace();
        QCOMPARE(hall.route(), "sets");
        hall.dispatch(Action::Back);
        QCOMPARE(hall.route(), "sets"); // B never flips the pair.
        hall.dispatch(Action::Up);
        QCOMPARE(hall.zone(), "list"); // No invisible legacy rail.
        hall.activate(0); hall.activate(1); hall.dispatch(Action::Right);
        QCOMPARE(hall.focusIndex(), 1);
        hall.switchFace();
        QCOMPARE(hall.route(), "archive-detail");
        QCOMPARE(hall.detail()["title"], memory);
        provider.setNextResult(AchievementState::Offline);
        provider.refresh("emerald-sample"); // Finishes while the other face is visible.
        QCOMPARE(hall.detail()["title"], memory);
        const auto navigation = hall.navigationState();
        HallOfFameController restored(archive, provider);
        restored.restoreNavigation(navigation);
        QCOMPARE(restored.route(), "archive-detail");
        QCOMPARE(restored.detail()["title"], memory);
        restored.switchFace();
        QCOMPARE(restored.route(), "achievement-detail");
        QCOMPARE(restored.rowIndex(), 1);
        QCOMPARE(restored.focusIndex(), 0); // Persisted Refresh is disabled while loading.
        restored.switchFace();
        provider.finishRefresh("emerald-sample");
        QCOMPARE(restored.detail()["title"], memory);
        restored.switchFace();
        QVERIFY(restored.status().contains("Offline"));
        QCOMPARE(restored.rowIndex(), 1);
        archive.value.entries.clear(); restored.refreshArchive(); // Archive disappears off-screen.
        restored.switchFace();
        QCOMPARE(restored.route(), "archive-list");
        QCOMPARE(restored.zone(), "actions");
        provider.setAccount({});
        restored.switchFace();
        QCOMPARE(restored.route(), "achievements");
        QVERIFY(restored.rows().isEmpty());
        QCOMPARE(restored.zone(), "actions");
        QCOMPARE(restored.detail()["summary"].toString(), "Unlocks unavailable");
        restored.dispatch(Action::Back); restored.dispatch(Action::Back);
        QCOMPARE(restored.route(), "sets");
        restored.activateControl("actions", 0);
        QCOMPARE(restored.route(), "sets"); // Refresh cannot change faces either.
    }
    void legacyNavigationAndInvalidFaceState() {
        MutableArchive archive;
        MockAchievementProvider provider;
        HallOfFameController hall(archive, provider);
        hall.restoreNavigation({{"route", "sets"}, {"zone", "rail"}, {"rail", 1},
            {"archiveView", QJsonObject{{"route", "achievement-detail"}, {"zone", "rail"}, {"action", 999}}}});
        QCOMPARE(hall.route(), "sets"); QCOMPARE(hall.zone(), "list");
        hall.switchFace();
        QCOMPARE(hall.route(), "archive-list"); QCOMPARE(hall.zone(), "list");
        hall.activate(2); hall.switchFace();
        HallOfFameController restored(archive, provider);
        restored.restoreNavigation(hall.navigationState());
        QCOMPARE(restored.route(), "sets");
        restored.switchFace();
        QCOMPARE(restored.route(), "archive-detail"); QCOMPARE(restored.rowIndex(), 2);
    }
    void shellPairsRespectOverlaysAndKeepTheSharedChoice() {
        MockLibraryRepository library;
        MockTrainerRepository profiles;
        MockAdventureAdapter adapter;
        DevelopmentPlatformService platform;
        MockPokedexRepository dex;
        MockHallOfFameRepository archive;
        MockAchievementProvider provider;
        provider.enableAccountPreview();
        ShellController shell(library, profiles, adapter, platform, dex, dex, archive, provider);
        shell.goToPage(4); shell.activate(1);
        const auto choice = shell.currentAdventureId();
        QVERIFY(shell.pairedNavigationAvailable());
        shell.dispatch(Action::NextFace);
        QCOMPARE(shell.hall()->route(), "sets");
        shell.activate(0); shell.activate(1);
        shell.dispatch(Action::ToggleContinue);
        QVERIFY(shell.drawerOpen()); QVERIFY(!shell.pairedNavigationAvailable());
        shell.dispatch(Action::PreviousFace);
        QCOMPARE(shell.hall()->route(), "achievement-detail");
        shell.dispatch(Action::Back);
        QCOMPARE(shell.currentAdventureId(), choice);
        shell.dispatch(Action::SystemMenu); shell.dispatch(Action::NextFace);
        QCOMPARE(shell.hall()->route(), "achievement-detail");
        shell.dispatch(Action::Back); shell.dispatch(Action::Secondary);
        QVERIFY(shell.hall()->account()->isOpen());
        shell.dispatch(Action::NextFace); QVERIFY(!shell.hall()->isArchive());
        shell.dispatch(Action::Back); shell.dispatch(Action::PreviousFace);
        QCOMPARE(shell.hall()->route(), "archive-detail");
        QCOMPARE(shell.hall()->rowIndex(), 1);
        shell.dispatch(Action::LocalAction);
        QVERIFY(shell.hall()->editor()->isOpen());
        shell.dispatch(Action::NextFace); QVERIFY(shell.hall()->isArchive());
        shell.dispatch(Action::Back);
        shell.dispatch(Action::PreviousPage); shell.dispatch(Action::NextPage);
        QCOMPARE(shell.hall()->route(), "archive-detail");
        const auto saved = shell.navigationState();
        ShellController restored(library, profiles, adapter, platform, dex, dex, archive, provider);
        restored.restoreNavigation(saved);
        QCOMPARE(restored.page(), 4);
        QCOMPARE(restored.hall()->route(), "archive-detail");
        restored.dispatch(Action::NextFace);
        QCOMPARE(restored.hall()->route(), "achievement-detail");
        QCOMPARE(restored.hall()->rowIndex(), 1);
        QCOMPARE(restored.currentAdventureId(), choice);
    }
    void submittedMemoryDoesNotStealTheOtherFace() {
        MockLibraryRepository library;
        MockTrainerRepository profiles;
        MockAdventureAdapter adapter;
        DevelopmentPlatformService platform;
        MockPokedexRepository dex;
        MockHallOfFameRepository archive;
        MockAchievementProvider provider;
        ShellController shell(library, profiles, adapter, platform, dex, dex, archive, provider);
        shell.goToPage(4);
        shell.dispatch(Action::LocalAction);
        auto* editor = shell.hall()->editor();
        editor->activate(0); editor->activate(0); editor->submit();
        QVERIFY(editor->saving());
        shell.dispatch(Action::PreviousPage); shell.dispatch(Action::NextPage);
        shell.dispatch(Action::NextFace);
        shell.activate(0); shell.activate(1);
        const auto achievement = shell.hall()->detail()["title"];
        QTRY_VERIFY(!editor->saving());
        QCOMPARE(shell.hall()->route(), "achievement-detail");
        QCOMPARE(shell.hall()->detail()["title"], achievement);
        shell.dispatch(Action::PreviousFace);
        QCOMPARE(shell.hall()->route(), "archive-detail");
        QCOMPARE(shell.hall()->rows().size(), 5);
    }
    void archiveDetailUnknownsAndStableRefresh() {
        MutableArchive archive;
        MockAchievementProvider provider;
        HallOfFameController hall(archive, provider);
        QCOMPARE(hall.rows().size(), 4);
        hall.activate(0);
        QCOMPARE(hall.team().size(), 6);
        QCOMPARE(hall.team().first().toMap()["name"].toString(), "Typhlosion");
        hall.dispatch(Action::Back); hall.dispatch(Action::Down); hall.activate(1);
        QVERIFY(!hall.team().last().toMap()["level"].toString().contains("0"));
        QCOMPARE(hall.detail()["source"].toString(), "Imported record");
        archive.value.entries[1].completedAt = QDateTime::fromString("2026-09-04T12:00:00Z", Qt::ISODate);
        hall.refreshArchive();
        QCOMPARE(hall.rowIndex(), 0);
        QCOMPARE(hall.detail()["title"].toString(), "Pokémon Emerald");
        archive.value.entries.removeAt(1); hall.refreshArchive();
        QCOMPARE(hall.route(), "archive-list");
        hall.activate(2); // Unknown date/team record now last.
        QCOMPARE(hall.detail()["date"].toString(), "Date not recorded");
        QCOMPARE(hall.detail()["time"].toString(), "Time not recorded");
        for (const auto& member : hall.team()) QVERIFY(!member.toMap()["known"].toBool());
    }
    void localArchiveSurvivesFailureAndEmptyRetry() {
        MutableArchive archive;
        MockAchievementProvider provider;
        HallOfFameController hall(archive, provider);
        QSignalSpy messages(&hall, &HallOfFameController::messageRequested);
        archive.value = {false, {}, "Archive unavailable"}; hall.refreshArchive();
        QCOMPARE(hall.rows().size(), 4);
        QCOMPARE(messages.size(), 1);
        archive.value = {}; hall.refreshArchive();
        QCOMPARE(hall.zone(), "actions");
        archive.value = archive.sample.loadArchive(); hall.activate(0);
        QCOMPARE(hall.zone(), "list"); QCOMPARE(hall.rows().size(), 4);
        provider.setAccount({}); achievements(hall);
        QVERIFY(hall.rows().isEmpty());
        QVERIFY(hall.detail()["summary"].toString().contains("unavailable"));
        hall.switchFace();
        QCOMPARE(hall.rows().size(), 4);
    }
    void separateUnlockStatesModesAndDates() {
        MutableArchive archive;
        MockAchievementProvider provider;
        HallOfFameController hall(archive, provider);
        achievements(hall);
        QCOMPARE(hall.rows().size(), 5);
        QVERIFY(hall.detail()["summary"].toString().contains("Standard"));
        hall.activate(1);
        QCOMPARE(hall.detail()["summary"].toString(), "Unlocked · Hardcore");
        hall.dispatch(Action::Back); hall.activate(2);
        QCOMPARE(hall.detail()["summary"].toString(), "Locked");
        hall.dispatch(Action::Back); hall.activate(3);
        QCOMPARE(hall.detail()["summary"].toString(), "Not recorded");
        hall.dispatch(Action::Back); hall.activate(4);
        QVERIFY(hall.detail()["time"].toString().contains("Date not recorded"));
        hall.dispatch(Action::Back); hall.dispatch(Action::Back);
        QCOMPARE(hall.route(), "sets");
        hall.activate(1); // The same definition IDs in another set have separate navigation.
        QCOMPARE(hall.rowIndex(), 0);
        hall.dispatch(Action::Back); hall.activate(0);
        QCOMPARE(hall.rowIndex(), 4);
    }
    void loadingOfflineErrorAndRecovery() {
        MutableArchive archive;
        MockAchievementProvider provider;
        HallOfFameController hall(archive, provider);
        achievements(hall); hall.activate(1);
        provider.holdRequests(true); provider.setNextResult(AchievementState::Offline);
        hall.activate(1); // Detail's Refresh action.
        QVERIFY(hall.status().contains("Refreshing"));
        QVERIFY(!hall.actions()[1].toMap()["enabled"].toBool());
        QCOMPARE(hall.focusIndex(), 0);
        hall.dispatch(Action::Right); QCOMPARE(hall.focusIndex(), 0);
        provider.finishRefresh("emerald-sample");
        QVERIFY(hall.status().contains("Offline"));
        QCOMPARE(hall.rows().size(), 5); QCOMPARE(hall.rowIndex(), 1);
        provider.setNextResult(AchievementState::Error); hall.activate(1); provider.finishRefresh("emerald-sample");
        QVERIFY(hall.status().contains("Refresh failed")); QCOMPARE(hall.rows().size(), 5);
        hall.activate(1); provider.finishRefresh("emerald-sample");
        QVERIFY(hall.status().contains("sample records"));
        hall.dispatch(Action::Back); hall.dispatch(Action::Back); hall.activate(2);
        QVERIFY(hall.rows().isEmpty());
        QVERIFY(!hall.actions()[1].toMap()["enabled"].toBool());
        hall.dispatch(Action::Back); QCOMPARE(hall.rowIndex(), 2);
    }
    void contextAndRequestIsolation() {
        MutableArchive archive;
        MockAchievementProvider provider;
        HallOfFameController hall(archive, provider);
        achievements(hall);
        provider.holdRequests(true); provider.refresh("emerald-sample");
        provider.setAccount("another-sample");
        provider.finishRefresh("emerald-sample"); // A response from the previous account cannot complete.
        QVERIFY(hall.rows().isEmpty());
        provider.refresh("emerald-sample"); provider.finishRefresh("emerald-sample");
        QCOMPARE(hall.rows().size(), 5);
        for (const auto& row : hall.rows()) QCOMPARE(row.toMap()["subtitle"].toString(), "Not recorded");
        hall.dispatch(Action::Back);
        QCOMPARE(hall.detail()["summary"].toString(), "Unlock status not recorded");
        StaleProvider stale;
        stale.claimedContext.accountId = "different-account";
        HallOfFameController other(archive, stale); achievements(other);
        QVERIFY(other.rows().isEmpty());
        stale.claimedContext = stale.sample.context(); stale.wrongSet = "another-set";
        emit stale.snapshotChanged("emerald-sample");
        QVERIFY(other.rows().isEmpty());
    }
    void asyncRefreshAndGlobalShellState() {
        MockLibraryRepository library;
        MockTrainerRepository profiles;
        MockAdventureAdapter adapter;
        DevelopmentPlatformService platform;
        MockPokedexRepository dex;
        MutableArchive archive;
        MockAchievementProvider provider;
        ShellController shell(library, profiles, adapter, platform, dex, dex, archive, provider);
        const auto home = shell.home();
        shell.goToPage(4); achievements(*shell.hall()); shell.activate(1);
        shell.dispatch(Action::SystemMenu); shell.dispatch(Action::Back);
        QCOMPARE(shell.hall()->route(), "achievement-detail");
        shell.dispatch(Action::PreviousPage); shell.dispatch(Action::NextPage);
        QCOMPARE(shell.hall()->rowIndex(), 1);
        shell.activate(1);
        QVERIFY(shell.hall()->status().contains("Refreshing"));
        QTRY_VERIFY(shell.hall()->status().contains("sample records"));
        QCOMPARE(shell.home(), home);
        QCOMPARE(archive.loadArchive().entries.size(), 4);
        shell.dispatch(Action::Back); shell.dispatch(Action::Back); shell.dispatch(Action::Back);
        QCOMPARE(shell.page(), 4); QCOMPARE(shell.hall()->route(), "sets");
    }
};
QTEST_GUILESS_MAIN(HallOfFameTests)
#include "HallOfFameTests.moc"
