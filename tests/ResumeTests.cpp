#include "core/navigation/ShellController.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include <QtTest>

using namespace trainer;
namespace {
class Library final : public LibraryRepository {
public:
    MockLibraryRepository sample;
    QList<ResumePoint> points = sample.resumePoints();
    QList<PlaySession> sessions;
    QList<World> worlds() const override { return sample.worlds(); }
    QList<Adventure> adventures() const override { return sample.adventures(); }
    QList<ResumePoint> resumePoints() const override { return points; }
    QList<PlaySession> recentSessions() const override { return sessions; }
    HomeSnapshot home() const override { return sample.home(); }
};
class Adapter final : public AdventureAdapter {
public:
    MockAdventureAdapter mock;
    int launches = 0, resumes = 0;
    std::function<void()> beforeResume;
    QString id() const override { return mock.id(); }
    AdventureCapabilities capabilities(const Adventure& a) const override { return mock.capabilities(a); }
    ResumeAvailability resumeAvailability(const Adventure& a, const ResumePoint& p) const override { return mock.resumeAvailability(a, p); }
    AdventureResult launch(const Adventure& a) override { ++launches; return mock.launch(a); }
    AdventureResult resume(const Adventure& a, const ResumePoint& p) override {
        ++resumes;
        if (beforeResume) beforeResume();
        return mock.resume(a, p);
    }
};
struct Fixture {
    Library library;
    MockTrainerRepository profiles;
    Adapter adapter;
    DevelopmentPlatformService platform;
    MockPokedexRepository dex;
    MockHallOfFameRepository archive;
    MockAchievementProvider achievements;
    ShellController shell{library, profiles, adapter, platform, dex, dex, archive, achievements};
    void select() { shell.dispatch(Action::ToggleContinue); shell.dispatch(Action::Confirm); }
};
}
class ResumeTests : public QObject {
    Q_OBJECT
private slots:
    void availability_data() {
        QTest::addColumn<ResumeAvailability>("status"); QTest::addColumn<QString>("label");
        QTest::newRow("exact without screenshot") << ResumeAvailability::Exact << "Saved moment";
        QTest::newRow("launch only") << ResumeAvailability::LaunchOnly << "Choose a save in Adventure";
        QTest::newRow("unverified") << ResumeAvailability::Stale << "Saved moment needs checking";
        QTest::newRow("deleted") << ResumeAvailability::Missing << "Saved moment unavailable";
        QTest::newRow("integration changed") << ResumeAvailability::Incompatible << "Resume setup changed";
    }
    void availability() {
        QFETCH(ResumeAvailability, status); QFETCH(QString, label);
        Fixture f; f.library.points[1].availability = status; f.shell.refreshLibrary();
        const auto row = f.shell.resumePoints().first().toMap();
        QCOMPARE(row["previewLabel"].toString(), label);
        QVERIFY(!row.contains("source")); QVERIFY(!row.contains("adapterPayload"));
        f.select(); QVERIFY(f.shell.notice().isEmpty()); QCOMPARE(f.adapter.launches, 0); QCOMPARE(f.adapter.resumes, 0);
        f.shell.dispatch(Action::Confirm);
        QCOMPARE(f.adapter.resumes, status == ResumeAvailability::Exact ? 1 : 0);
        QCOMPARE(f.adapter.launches, status == ResumeAvailability::LaunchOnly ? 1 : 0);
        if (status != ResumeAvailability::Exact && status != ResumeAvailability::LaunchOnly) {
            QVERIFY(f.shell.notice().contains("choose a save"));
            f.shell.dispatch(Action::Back); f.shell.dispatch(Action::Confirm);
            QCOMPARE(f.adapter.launches, 1); QCOMPARE(f.adapter.resumes, 0);
        }
    }
    void replacementBetweenSelectionAndPressNeverLaunches() {
        Fixture f; f.select();
        QCOMPARE(f.shell.home()["action"].toString(), "Resume Adventure");
        f.library.points[1].source.revision = "replacement";
        // Only the provider snapshot changed; the displayed cached assessment
        // still offers Resume. Exercise the activation-time comparison.
        f.shell.dispatch(Action::Confirm);
        QCOMPARE(f.adapter.resumes, 0); QCOMPARE(f.adapter.launches, 0);
        QVERIFY(f.shell.notice().contains("changed"));
        QCOMPARE(f.shell.home()["adventureId"].toString(), "emerald-demo");
        f.adapter.mock.setSource("emerald-1", "replacement");
        f.shell.dispatch(Action::Back); f.select(); f.shell.dispatch(Action::Confirm);
        QCOMPARE(f.adapter.resumes, 1); // Explicitly choosing the refreshed card authorizes this revision.
    }
    void replacementAcrossRefreshAndRestartKeepsPinnedRevision() {
        Fixture f; f.select();
        const auto saved = f.shell.navigationState();
        f.library.points[1].source.revision = "replacement";
        f.adapter.mock.setSource("emerald-1", "replacement");
        f.shell.refreshLibrary();
        QCOMPARE(f.shell.home()["milestone"].toString(), "Saved moment needs checking");
        f.shell.restoreNavigation(saved);
        f.shell.dispatch(Action::Confirm);
        QCOMPARE(f.adapter.resumes, 0); QCOMPARE(f.adapter.launches, 0);
        QVERIFY(!f.shell.notice().isEmpty());
        auto legacy = saved; legacy.remove("homeResumeSource");
        f.shell.restoreNavigation(legacy); f.shell.dispatch(Action::Confirm);
        QCOMPARE(f.adapter.resumes, 0); QCOMPARE(f.adapter.launches, 0);
    }
    void removedPointAndAdapterChangeAreDistinct() {
        Fixture f; f.select(); f.library.points.removeAt(1); f.shell.refreshLibrary();
        QCOMPARE(f.shell.home()["milestone"].toString(), "Saved moment unavailable");
        f.shell.dispatch(Action::Confirm); QCOMPARE(f.adapter.resumes, 0); QCOMPARE(f.adapter.launches, 0);
        Fixture changed; changed.select(); changed.adapter.mock.setIntegrationRevision("mock-v2");
        QCOMPARE(changed.shell.home()["milestone"].toString(), "Resume setup changed");
        changed.shell.dispatch(Action::Confirm); QCOMPARE(changed.adapter.resumes, 0); QCOMPARE(changed.adapter.launches, 0);
    }
    void adapterRevalidatesAfterPresentationCheck() {
        Fixture f; f.select();
        f.adapter.beforeResume = [&] { f.adapter.mock.setSource("emerald-1", ""); };
        f.shell.dispatch(Action::Confirm);
        QCOMPARE(f.adapter.resumes, 1); QCOMPARE(f.adapter.launches, 0);
        QVERIFY(f.shell.notice().contains("unavailable"));
        QVERIFY(!f.shell.notice().contains("Demo resume ready"));
    }
    void asynchronousSnapshotKeepsFocusAndDeduplicates() {
        Fixture f;
        f.shell.dispatch(Action::ToggleContinue); f.shell.dispatch(Action::Right);
        QCOMPARE(f.shell.resumePoints()[f.shell.focusIndex()].toMap()["id"].toString(), "crystal-1");
        auto observation = f.library.points[0]; observation.observedAt = QDateTime::currentDateTimeUtc();
        observation.summary = "New observation, same source";
        QTimer::singleShot(0, &f.shell, [&] {
            f.library.points.append(observation);
            f.library.sessions.append({"new-session", "ruby-demo", QDateTime::currentDateTimeUtc(), {}, {}, PlaySessionOutcome::Running});
            f.shell.refreshLibrary();
        });
        QCOMPARE(f.shell.resumePoints().size(), 3);
        QTRY_COMPARE(f.shell.resumePoints().size(), 4);
        QCOMPARE(f.shell.resumePoints().first().toMap()["id"].toString(), "recent:ruby-demo");
        QCOMPARE(f.shell.resumePoints()[f.shell.focusIndex()].toMap()["id"].toString(), "crystal-1");
        QCOMPARE(f.shell.resumePoints()[f.shell.focusIndex()].toMap()["summary"].toString(), observation.summary);
        f.shell.dispatch(Action::Confirm); QCOMPARE(f.adapter.resumes, 0); QCOMPARE(f.adapter.launches, 0);
    }
    void foreignOrUnprovenSourceCannotResume() {
        MockLibraryRepository repo; MockAdventureAdapter adapter;
        const auto adventure = repo.adventures().first(); auto point = repo.resumePoints()[1];
        point.source.adapterId = "another";
        QCOMPARE(adapter.resumeAvailability(adventure, point), ResumeAvailability::Incompatible);
        QVERIFY(!adapter.resume(adventure, point).success);
        point = repo.resumePoints()[1]; point.source.revision.clear();
        QCOMPARE(adapter.resumeAvailability(adventure, point), ResumeAvailability::Stale);
        QVERIFY(!adapter.resume(adventure, point).success);
    }
    void worldsCannotSubstituteAReplacedState() {
        Fixture f; auto& worlds = *f.shell.worlds();
        worlds.activate(2); worlds.activate(0);
        QVERIFY(worlds.actions().first().toMap()["enabled"].toBool());
        f.library.points[1].source.revision = "replacement";
        worlds.activate(0);
        QCOMPARE(f.adapter.resumes, 0); QCOMPARE(f.adapter.launches, 0);
        QVERIFY(f.shell.notice().contains("changed"));
        f.library.points[1].availability = ResumeAvailability::Missing; worlds.refresh();
        QVERIFY(!worlds.actions().first().toMap()["enabled"].toBool());
        QCOMPARE(worlds.focusIndex(), 1);
    }
};
QTEST_GUILESS_MAIN(ResumeTests)
#include "ResumeTests.moc"
