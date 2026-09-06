#include "core/storage/SessionState.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QFile>
#include <QUuid>

using namespace trainer;
namespace {
struct Fixture {
    MockLibraryRepository library;
    MockAdventureAdapter adapter;
    DevelopmentPlatformService platform;
    MockPokedexRepository reference;
    MockHallOfFameRepository archive;
    MockAchievementProvider achievements;
    LocalStateStore store;
    ShellController shell;
    SessionState session;
    explicit Fixture(const QString& path) : store(path),
        shell(library, store, adapter, platform, reference, store, archive, achievements), session(shell, &store) {}
};
class ExternalConnection {
public:
    QSqlDatabase db;
    explicit ExternalConnection(const QString& path) {
        db = QSqlDatabase::addDatabase("QSQLITE", QUuid::createUuid().toString());
        db.setDatabaseName(path + "/traineros.sqlite3"); db.open();
    }
    ~ExternalConnection() {
        const auto name = db.connectionName(); db.close(); db = {}; QSqlDatabase::removeDatabase(name);
    }
};
QByteArray bytes(const QString& path) { QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {}; return file.readAll(); }
}
class StorageTests final : public QObject {
    Q_OBJECT
private slots:
    void durableProfileFavoritesAndRoutes() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        QString identity; QDateTime created; QJsonObject navigation;
        {
            Fixture f(dir.path()); f.session.start();
            QTRY_VERIFY(f.store.ready()); QVERIFY(!f.session.blocked());
            QVERIFY(!f.shell.trainer()->exists());
            QVERIFY(!f.store.progress("pikachu").favorite); // No fixture favorite leakage.
            QVERIFY(!f.store.progress("bulbasaur").seen.has_value());
            auto* trainer = f.shell.trainer();
            trainer->beginEdit(); trainer->setDraftName("ERI 2"); trainer->activate(3);
            QVERIFY(trainer->saving()); QVERIFY(!trainer->exists()); // No optimistic success.
            f.shell.goToPage(4); // Global page navigation remains usable during Save.
            QTRY_VERIFY(!trainer->saving()); QVERIFY(trainer->exists());
            identity = f.store.load()->id; created = f.store.load()->createdAt;
            trainer->beginEdit(); trainer->setDraftName("Not saved"); trainer->cancel();
            QCOMPARE(f.store.load()->name, "ERI 2");
            bool favoriteDone = false;
            f.store.setFavoriteAsync("pikachu", true, this, [&](const QString& error) { QVERIFY(error.isEmpty()); favoriteDone = true; });
            QTRY_VERIFY(favoriteDone);
            f.shell.worlds()->activate(2); f.shell.worlds()->activate(0);
            f.shell.pokedex()->applySearch("pika");
            f.shell.pokedex()->dispatch(Action::Down); f.shell.pokedex()->dispatch(Action::Confirm);
            f.shell.goToPage(2);
            navigation = f.shell.navigationState();
            f.shell.dispatch(Action::SystemMenu); // Transient menu must not be restored.
            QSignalSpy exited(&f.session, &SessionState::exitReady);
            f.session.requestExit();
            QTRY_VERIFY(!exited.isEmpty()); QCOMPARE(f.store.pending(), 0);
        }
        {
            Fixture f(dir.path()); f.session.start(); QTRY_VERIFY(f.store.ready());
            QCOMPARE(f.store.load()->id, identity); QCOMPARE(f.store.load()->createdAt, created);
            QCOMPARE(f.store.load()->name, "ERI 2"); QVERIFY(f.store.progress("pikachu").favorite);
            QVERIFY(!f.store.progress("pikachu").caught.has_value());
            QCOMPARE(f.shell.navigationState(), navigation);
            QVERIFY(!f.shell.menuOpen()); QVERIFY(!f.shell.keyboard()->isOpen()); QVERIFY(!f.shell.trainer()->editing());
            auto* trainer = f.shell.trainer(); trainer->beginEdit(); trainer->setDraftName("ERI 3"); trainer->activate(3);
            QTRY_VERIFY(!trainer->saving());
            QCOMPARE(f.store.load()->id, identity); QCOMPARE(f.store.load()->createdAt, created);
            auto other = *f.store.load(); other.id = "another-trainer";
            bool rejected = false;
            f.store.saveAsync(other, this, [&](const auto& result) { rejected = !result.success; });
            QTRY_VERIFY(rejected); QCOMPARE(f.store.load()->id, identity);
        }
        LocalStateStore reopened(dir.path()); reopened.open(); QTRY_VERIFY(reopened.ready());
        QCOMPARE(reopened.load()->name, "ERI 3");
    }
    void lockedWriteKeepsDraftAndUiResponsive() {
        QTemporaryDir dir; Fixture f(dir.path()); f.session.start(); QTRY_VERIFY(f.store.ready());
        ExternalConnection external(dir.path()); QSqlQuery query(external.db);
        QVERIFY(query.exec("BEGIN IMMEDIATE"));
        int ticks = 0; QTimer tick; tick.setInterval(10);
        connect(&tick, &QTimer::timeout, this, [&] { if (f.store.pending()) ++ticks; }); tick.start();
        // An autocommit write waits for busy_timeout. A profile transaction's
        // read-to-write upgrade is allowed to fail immediately to avoid deadlock.
        f.store.setFavoriteAsync("eevee", true, this, [](const QString& error) { QVERIFY(!error.isEmpty()); });
        auto* trainer = f.shell.trainer(); trainer->beginEdit(); trainer->setDraftName("Retry me"); trainer->activate(3);
        QVERIFY(trainer->saving());
        QSignalSpy exited(&f.session, &SessionState::exitReady);
        f.session.requestExit();
        QTRY_VERIFY(!trainer->saving());
        QVERIFY(ticks > 0); QVERIFY(trainer->editing()); QVERIFY(!trainer->error().isEmpty());
        QVERIFY(!trainer->exists()); QVERIFY(!f.store.load()); QVERIFY(exited.isEmpty());
        QVERIFY(query.exec("ROLLBACK"));
        // A concurrent navigation flush may also fail; return to the still-retryable form.
        if (f.session.blocked()) f.session.activate(1);
        trainer->activate(3); QTRY_VERIFY(!trainer->saving()); QVERIFY(trainer->exists());
        f.session.requestExit(); QTRY_VERIFY(!exited.isEmpty());
    }
    void failedFavoriteKeepsCommittedValue() {
        QTemporaryDir dir; Fixture f(dir.path()); f.session.start(); QTRY_VERIFY(f.store.ready());
        ExternalConnection external(dir.path()); QSqlQuery query(external.db); QVERIFY(query.exec("BEGIN IMMEDIATE"));
        f.shell.pokedex()->activate(0); f.shell.pokedex()->activate(0);
        QTRY_VERIFY(!f.shell.pokedex()->saving());
        QVERIFY(!f.store.progress("bulbasaur").favorite); QVERIFY(!f.shell.notice().isEmpty());
        QVERIFY(query.exec("ROLLBACK")); f.shell.dispatch(Action::Back);
        f.shell.pokedex()->activate(0); QTRY_VERIFY(!f.shell.pokedex()->saving());
        QVERIFY(f.store.progress("bulbasaur").favorite);
        f.store.setFavoriteAsync("bulbasaur", false, this, [](const QString& error) { QVERIFY(error.isEmpty()); });
        QTRY_COMPARE(f.store.pending(), 0); QVERIFY(!f.store.progress("bulbasaur").favorite);
    }
    void secondInstanceAndRetry() {
        QTemporaryDir dir;
        auto first = std::make_unique<LocalStateStore>(dir.path()); first->open(); QTRY_VERIFY(first->ready());
        LocalStateStore second(dir.path()); second.open(); QTRY_VERIFY(!second.opening());
        QVERIFY(!second.ready()); QVERIFY(second.error().contains("locked"));
        first.reset(); second.open(); QTRY_VERIFY(second.ready());
    }
    void corruptForeignAndFutureStoresArePreserved() {
        for (int kind = 0; kind < 3; ++kind) {
            QTemporaryDir dir; const auto path = dir.path() + "/traineros.sqlite3";
            if (kind == 0) { QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("not a database\n"); }
            else {
                ExternalConnection external(dir.path()); QSqlQuery query(external.db);
                QVERIFY(query.exec("CREATE TABLE unrelated(value TEXT)"));
                if (kind == 2) QVERIFY(query.exec("PRAGMA user_version=99"));
            }
            const auto original = bytes(path); QVERIFY(!original.isEmpty());
            LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(!store.opening());
            QVERIFY(!store.ready()); QVERIFY(!store.error().isEmpty()); QCOMPARE(bytes(path), original);
        }
    }
    void unavailableDirectory() {
        QTemporaryDir dir; const auto path = dir.path() + "/file";
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("keep"); file.close();
        LocalStateStore store(path + "/nested"); store.open(); QTRY_VERIFY(!store.opening());
        QVERIFY(!store.ready()); QCOMPARE(bytes(path), QByteArray("keep"));
    }
    void staleNavigationAndDraftExclusion() {
        QTemporaryDir dir; Fixture f(dir.path()); f.session.start(); QTRY_VERIFY(f.store.ready());
        const QJsonObject stale{{"version", 1}, {"page", "removed"},
            {"worlds", QJsonObject{{"world", "missing"}, {"route", "detail"}}},
            {"pokedex", QJsonObject{{"entry", "missing"}, {"zone", "detail"}, {"type", "invalid"}, {"rail", -100}, {"sort", "invalid"}}},
            {"hall", QJsonObject{{"archive", "missing"}, {"route", "archive-detail"}, {"action", 999}}}};
        f.shell.restoreNavigation(stale);
        QCOMPARE(f.shell.page(), 0); QCOMPARE(f.shell.worlds()->route(), "regions");
        QCOMPARE(f.shell.pokedex()->zone(), "list"); QCOMPARE(f.shell.pokedex()->detail()["id"].toString(), "bulbasaur");
        QCOMPARE(f.shell.hall()->route(), "archive-list");
        f.shell.goToPage(3); const auto before = f.shell.navigationState();
        f.shell.dispatch(Action::Confirm); f.shell.dispatch(Action::Confirm);
        f.shell.keyboard()->dispatch(Action::Confirm);
        QCOMPARE(f.shell.navigationState(), before); QVERIFY(!f.store.load());
        f.shell.restoreNavigation(before); QVERIFY(!f.shell.keyboard()->isOpen()); QVERIFY(!f.shell.trainer()->editing());
        f.shell.restoreNavigation(QJsonObject{{"version", 99}, {"page", "worlds"}});
        QCOMPARE(f.shell.page(), 3);
    }
    void navigationFailureRetryAndExplicitSkip() {
        QTemporaryDir dir; Fixture f(dir.path()); f.session.start(); QTRY_VERIFY(f.store.ready());
        ExternalConnection external(dir.path()); QSqlQuery query(external.db); QVERIFY(query.exec("BEGIN IMMEDIATE"));
        f.shell.goToPage(1); QSignalSpy exited(&f.session, &SessionState::exitReady);
        f.session.requestExit(); QTRY_VERIFY(!f.session.choices().isEmpty());
        QVERIFY(exited.isEmpty()); QCOMPARE(f.store.navigation(), QJsonObject());
        QVERIFY(query.exec("ROLLBACK")); f.session.activate(0); QTRY_VERIFY(!exited.isEmpty());
        QCOMPARE(f.store.navigation()["page"].toString(), "worlds");
        // A second change can explicitly skip only browsing state after a failed flush.
        f.session.dispatch(Action::Back); f.shell.goToPage(2);
        QVERIFY(query.exec("BEGIN IMMEDIATE")); f.session.requestExit();
        QTRY_COMPARE(f.session.choices().size(), 3);
        const auto count = exited.size(); f.session.activate(2); QTRY_VERIFY(exited.size() > count);
        QCOMPARE(f.store.navigation()["page"].toString(), "worlds"); QVERIFY(query.exec("ROLLBACK"));
    }
    void completionContextMayDisappear() {
        QTemporaryDir dir; LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready());
        bool called = false; auto context = std::make_unique<QObject>();
        store.setFavoriteAsync("eevee", true, context.get(), [&](const QString&) { called = true; }); context.reset();
        QTRY_COMPARE(store.pending(), 0); QVERIFY(!called); QVERIFY(store.progress("eevee").favorite);
    }
};
QTEST_GUILESS_MAIN(StorageTests)
#include "StorageTests.moc"
