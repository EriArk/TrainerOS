#include "integrations/achievements/RetroAchievementsProvider.h"
#include "integrations/achievements/TrainerAchievementProvider.h"
#include "integrations/achievements/RetroArchAchievementSession.h"
#include "features/halloffame/AchievementAccountController.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonArray>
#include <QCryptographicHash>

using namespace trainer;
namespace {
const AchievementAccount account{"ExampleTrainer", "0123456789abcdef0123456789abcdef"};
void write(const QString& path, const QByteArray& bytes) { QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(bytes), bytes.size()); }
AdventureRegistration registration(const QString& path) {
    AdventureRegistration record; record.adventure.id = "original-test-adventure"; record.adventure.title = "Original test Adventure";
    record.adventure.worldId = "kanto"; record.adventure.adapterId = "retroarch"; record.adventure.platformId = "gba";
    record.contentPath = path; return record;
}
AchievementReply response(const AchievementParameters& q) {
    if (q.value("r") == "login2") return {{{"User", account.username}, {"Token", account.token}}, true};
    if (q.value("r") == "gameid") return {{{"GameID", 42}}, true};
    if (q.value("r") == "patch") {
        QJsonArray entries;
        for (int i = 0; i < 4; ++i) entries.append(QJsonObject{{"ID", 101 + i}, {"Title", "Original test goal " + QString::number(i)},
            {"Description", "Original content-free test description"}, {"Flags", i == 3 ? 5 : 3}});
        return {{{"PatchData", QJsonObject{{"ID", 42}, {"Title", "Original test set"}, {"Achievements", entries}}}}, true};
    }
    if (q.value("r") == "unlocks") return {{{"UserUnlocks", q.value("h") == "1" ? QJsonArray{102} : QJsonArray{101}}}, true};
    return {};
}
class RecentLibrary : public LibraryRepository {
public:
    AdventureRegistration value;
    QList<World> worlds() const override { return {{"kanto", "Kanto", {}}}; }
    QList<Adventure> adventures() const override { return {value.adventure}; }
    QList<ResumePoint> resumePoints() const override { return {}; }
    HomeSnapshot home() const override { return {}; }
    // A content-free local Adventure is sufficient to exercise association and caching.
    std::optional<AdventureRegistration> registration(const QString& id) const override { return id == value.adventure.id ? std::optional(value) : std::nullopt; }
    QList<PlaySession> recentSessions() const override { return {{"session", value.adventure.id, QDateTime::currentDateTimeUtc(), {}, {}, PlaySessionOutcome::Returned}}; }
};
}
class RetroAchievementsTests : public QObject {
    Q_OBJECT
private slots:
    void launchAccountIsTrainerScopedAndClearedOnSignOut() {
        QTemporaryDir first, second; RecentLibrary library;
        QVERIFY(writeAchievementAccount(first.filePath("integrations/retroachievements-account.json"), account));
        TrainerAchievementProvider a(library), b(library);
        QVERIFY(!a.launchAccount().valid());
        a.bind(first.path()); b.bind(second.path());
        QCOMPARE(a.launchAccount().username, account.username);
        QVERIFY(!b.launchAccount().valid());
        b.bind(first.path()); QVERIFY(!b.launchAccount().valid()); // Cannot rebind to another owner.
        QTRY_VERIFY(!a.accountBusy());
        a.disconnectAccount(); QVERIFY(!a.launchAccount().valid());
    }
    void launchOverlayPreservesSaveRouteAndIsRemovedAfterReturn() {
        QTemporaryDir dir;
        const auto base = dir.filePath("retroarch.cfg"), ordinary = dir.filePath("owned-save.cfg");
        const QByteArray global = "cheevos_username = \"OtherTrainer\"\ncheevos_token = \"inherited-token\"\n";
        const QByteArray saves = "savefile_directory = \"/private/owner/saves\"\n";
        write(base, global); write(ordinary, saves);
        for (const auto& selected : {account, AchievementAccount{"SecondTrainer", "fedcba9876543210fedcba9876543210"}, AchievementAccount{}}) {
            ProcessCommand command{dir.filePath("emulator"), {"--config", base, "--libretro", "core", "rom"}, {}};
            bool observedReturn = false;
            command.prepare = [&](auto& cmd, const auto&) -> QString {
                cmd.arguments.insert(cmd.arguments.size() - 1, "--appendconfig");
                cmd.arguments.insert(cmd.arguments.size() - 1, ordinary);
                cmd.settled = [&](const auto&) { observedReturn = true; };
                return {};
            };
            useRetroArchAchievementAccount(command, selected);
            const std::atomic_bool cancelled{false};
            QVERIFY(command.prepare(command, cancelled).isEmpty());
            QCOMPARE(command.arguments.last(), "rom");
            QCOMPARE(command.arguments.count("--appendconfig"), 1);
            const auto paths = command.arguments[command.arguments.indexOf("--appendconfig") + 1].split('|');
            QCOMPARE(paths.size(), 2); QCOMPARE(paths[0], ordinary);
            QFile file(paths[1]); QVERIFY(file.open(QIODevice::ReadOnly)); const auto bytes = file.readAll(); file.close();
            QVERIFY(bytes.contains("cheevos_enable = \"" + QByteArray(selected.valid() ? "true" : "false") + "\""));
            QVERIFY(bytes.contains("cheevos_token = \"" + selected.token.toUtf8() + "\""));
            QVERIFY(bytes.contains("cheevos_username = \"" + selected.username.toUtf8() + "\""));
            QVERIFY(bytes.contains("cheevos_password = \"\""));
            QVERIFY(bytes.contains("cheevos_custom_host = \"\""));
            QVERIFY(bytes.contains("cheevos_hardcore_mode_enable = \"false\""));
            QVERIFY(!command.arguments.join(' ').contains(account.token));
            QVERIFY(!command.arguments.join(' ').contains("fedcba9876543210"));
#ifdef Q_OS_LINUX
            QVERIFY(!(QFileInfo(paths[1]).permissions() & (QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ReadOther | QFileDevice::WriteOther)));
#endif
            command.settled({true, 0, false, false});
            QVERIFY(observedReturn); QVERIFY(!QFileInfo::exists(paths[1]));
            QFile unchanged(base); QVERIFY(unchanged.open(QIODevice::ReadOnly)); QCOMPARE(unchanged.readAll(), global);
            QFile saveConfig(ordinary); QVERIFY(saveConfig.open(QIODevice::ReadOnly)); QCOMPARE(saveConfig.readAll(), saves);
        }
    }
    void rejectedPreparationDoesNotLeaveCredentials() {
        QTemporaryDir dir;
        const auto base = dir.filePath("retroarch.cfg"), ordinary = dir.filePath("save.cfg");
        write(base, "original"); write(ordinary, "original");
        const auto invocation = [&] { return ProcessCommand{dir.filePath("absent-emulator"), {"--config", base, "--appendconfig", ordinary, "rom"}, {}}; };
        const std::atomic_bool cancelled{true}, running{false};
        auto command = invocation(); useRetroArchAchievementAccount(command, account);
        QVERIFY(!command.prepare(command, cancelled).isEmpty());
        command = invocation(); command.prepare = [](auto&, const auto&) -> QString { return "Save storage unavailable."; };
        useRetroArchAchievementAccount(command, account);
        QCOMPARE(command.prepare(command, running), "Save storage unavailable.");
        command = invocation(); useRetroArchAchievementAccount(command, {"Bad\nName", account.token});
        QVERIFY(!command.prepare(command, running).isEmpty());
        QCOMPARE(QDir(dir.path()).entryList({".traineros-ra-*"}, QDir::Files | QDir::Hidden).size(), 0);
        command = invocation(); useRetroArchAchievementAccount(command, account);
        ProcessService process; QSignalSpy finished(&process, &ProcessService::finished);
        QVERIFY(process.start(command)); QTRY_COMPARE(finished.size(), 1); QVERIFY(!process.active());
        QCOMPARE(QDir(dir.path()).entryList({".traineros-ra-*"}, QDir::Files | QDir::Hidden).size(), 0);
    }
    void returnedAdventureRefreshNotifiesOnlyNewConfirmedUnlocks() {
        QTemporaryDir dir;RecentLibrary library;const auto path=dir.filePath("original.gba");write(path,"Original content-free bytes");library.value=registration(path);
        QVERIFY(writeAchievementAccount(dir.filePath("integrations/retroachievements-account.json"),account));
        std::atomic_bool earned{false};
        const AchievementTransport transport=[&](const auto& q){
            if(q.value("r")=="unlocks" && q.value("h")=="0")return AchievementReply{{{"UserUnlocks",earned.load()?QJsonArray{101,103}:QJsonArray{101}}},true};
            return response(q);
        };
        {
            RetroAchievementsProvider provider(library,dir.path(),transport);QSignalSpy notices(&provider,&AchievementProvider::achievementsEarned);
            QTRY_VERIFY(!provider.accountBusy());provider.refreshAdventure(library.value.adventure.id,true);QTRY_VERIFY(!provider.accountBusy());
            QCOMPARE(notices.size(),0); // First observation establishes a baseline, never old rewards.
            earned.store(true);provider.refreshAdventure(library.value.adventure.id,true);QTRY_VERIFY(!provider.accountBusy());
            QCOMPARE(notices.size(),1);QCOMPARE(notices[0][1].toStringList(),QStringList{"Original test goal 2"});
            provider.refreshAdventure(library.value.adventure.id,true);QTRY_VERIFY(!provider.accountBusy());QCOMPARE(notices.size(),1);
        }
        {
            RetroAchievementsProvider provider(library,dir.path(),transport);QSignalSpy notices(&provider,&AchievementProvider::achievementsEarned);
            QTRY_VERIFY(!provider.accountBusy());provider.refreshAdventure(library.value.adventure.id,true);QTRY_VERIFY(!provider.accountBusy());QCOMPARE(notices.size(),0);
        }
    }
    void formEncodingPreservesPasswordCharacters() {
        QCOMPARE(achievementFormBody({{"p", "a+%2B &=#?é"}, {"u", "ExampleTrainer"}}), QByteArray("p=a%2B%252B%20%26%3D%23%3F%C3%A9&u=ExampleTrainer"));
    }
    void verifiedWholeFileHashAndUnsupportedLayouts() {
        QTemporaryDir dir; const auto path = dir.filePath("original.test.gba"); const QByteArray bytes("original content-free bytes"); write(path, bytes);
        auto record = registration(path); std::atomic_bool cancelled{false};
        const auto hash = achievementContentHash(record, cancelled);
        QCOMPARE(hash, QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Md5).toHex()));
        record.adventure.platformId = "nds"; QVERIFY(achievementContentHash(record, cancelled).isEmpty()); record.adventure.platformId = "gba";
        record.adventure.collectionOnly = true; QVERIFY(achievementContentHash(record, cancelled).isEmpty()); record.adventure.collectionOnly = false;
        write(dir.filePath("original.test.ips"), "original patch placeholder"); QVERIFY(achievementContentHash(record, cancelled).isEmpty());
        QVERIFY(QFile::remove(dir.filePath("original.test.ips"))); cancelled.store(true); QVERIFY(achievementContentHash(record, cancelled).isEmpty());
    }
    void verifiedCoreDefinitionsAndUnlockModes() {
        QStringList operations;
        const auto result = fetchAchievements(account, registration("/unused.gba"), "Kanto", QString(32, 'a'), [&](const auto& q) {
            operations.append(q.value("r"));
            if (q.value("r") != "gameid" && (q.value("u") != account.username || q.value("t") != account.token || q.value("g") != "42")) return AchievementReply{};
            return response(q);
        });
        QVERIFY(result); QCOMPARE(result->set.gameId, "42"); QCOMPARE(result->snapshot.state, AchievementState::Ready);
        QCOMPARE(result->snapshot.definitions.size(), 3); QCOMPARE(result->snapshot.unlocks.size(), 3);
        QCOMPARE(result->snapshot.unlocks[0].mode, std::optional(AchievementMode::Standard));
        QCOMPARE(result->snapshot.unlocks[1].mode, std::optional(AchievementMode::Hardcore));
        QCOMPARE(result->snapshot.unlocks[2].unlocked, std::optional(false)); QVERIFY(!result->snapshot.unlocks[2].mode);
        for (const auto& unlock : result->snapshot.unlocks) QVERIFY(!unlock.earnedAt.isValid());
        QCOMPARE(operations, QStringList({"gameid", "patch", "unlocks", "unlocks"}));
        auto cache = achievementCache(*result); const auto loaded = readAchievementCache(cache, result->snapshot.context);
        QVERIFY(loaded); QCOMPARE(loaded->snapshot.state, AchievementState::Offline); QCOMPARE(loaded->snapshot.unlocks.size(), 3);
        QVERIFY(!readAchievementCache(cache, {"retroAchievements", "AnotherTrainer"}));
        cache["entries"] = QJsonArray{QJsonObject{{"id", "101"}, {"title", "Original"}, {"description", "Original"}, {"earned", false}, {"hardcore", true}}};
        QVERIFY(!readAchievementCache(cache, result->snapshot.context));
    }
    void partialOrInvalidRepliesDoNotInventLockedTotals() {
        auto fetch = [&](const AchievementTransport& transport) { return fetchAchievements(account, registration("/unused.gba"), "Kanto", QString(32, 'a'), transport); };
        const auto partial = fetch([](const auto& q) {
            if (q.value("r") == "unlocks" && q.value("h") == "1") return AchievementReply{{}, false, true};
            return response(q);
        });
        QVERIFY(partial); QCOMPARE(partial->snapshot.state, AchievementState::Offline); QVERIFY(partial->snapshot.definitions.isEmpty()); QVERIFY(partial->snapshot.unlocks.isEmpty());
        const auto wrongGame = fetch([](const auto& q) {
            auto reply = response(q); if (q.value("r") == "patch") { auto p = reply.body["PatchData"].toObject(); p["ID"] = 41; reply.body["PatchData"] = p; } return reply;
        });
        QVERIFY(wrongGame); QCOMPARE(wrongGame->snapshot.state, AchievementState::Error); QVERIFY(wrongGame->snapshot.definitions.isEmpty());
        QVERIFY(!fetch([](const auto&) { return AchievementReply{{{"GameID", 0}}, true}; }));
    }
    void privateAccountAndProviderCacheSurviveOfflineRestart() {
        QTemporaryDir dir; RecentLibrary library; const auto path = dir.filePath("original.gba"); write(path, "Original test ROM bytes"); library.value = registration(path);
        const auto accountPath = dir.filePath("integrations/retroachievements-account.json");
        QVERIFY(writeAchievementAccount(accountPath, account)); QCOMPARE(readAchievementAccount(accountPath).username, account.username);
        {
            RetroAchievementsProvider provider(library, dir.path(), response);
            QTRY_VERIFY(!provider.accountBusy()); provider.refreshAll(); QTRY_VERIFY(!provider.accountBusy());
            QCOMPARE(provider.sets().size(), 1); QCOMPARE(provider.snapshot("42").state, AchievementState::Ready);
        }
        {
            RetroAchievementsProvider provider(library, dir.path(), [](const auto&) { return AchievementReply{{}, false, true}; });
            QTRY_VERIFY(!provider.accountBusy()); QCOMPARE(provider.sets().size(), 1); QCOMPARE(provider.snapshot("42").state, AchievementState::Offline);
            provider.refreshAll(); QTRY_VERIFY(!provider.accountBusy()); QCOMPARE(provider.snapshot("42").definitions.size(), 3);
            provider.disconnectAccount(); QVERIFY(provider.context().accountId.isEmpty()); QVERIFY(provider.sets().isEmpty()); QVERIFY(!QFileInfo::exists(accountPath));
        }
        QVERIFY(writeAchievementAccount(accountPath, {"AnotherTrainer", account.token}));
        RetroAchievementsProvider different(library, dir.path(), response); QTRY_VERIFY(!different.accountBusy()); QVERIFY(different.sets().isEmpty());
    }
    void controllerLoginMasksDraftAndClearsItOnClose() {
        QTemporaryDir dir; RecentLibrary library; RetroAchievementsProvider provider(library, dir.path(), response);
        AchievementAccountController controller(provider); QSignalSpy text(&controller, &AchievementAccountController::textRequested);
        controller.begin(); controller.activate(0); controller.applyText("ExampleTrainer");
        controller.dispatch(Action::Down); controller.dispatch(Action::Confirm); QVERIFY(text.last().at(3).toBool());
        controller.applyText("original test password+%2B"); const auto rows = controller.rows();
        QVERIFY(!rows[1].toMap()["detail"].toString().contains("original"));
        controller.close(); controller.begin(); QCOMPARE(controller.rows()[1].toMap()["detail"].toString(), "Enter password");
        controller.activate(0); controller.applyText("ExampleTrainer"); controller.activate(1); controller.applyText("original test password"); controller.activate(2);
        QTRY_VERIFY(!provider.accountBusy()); QCOMPARE(provider.context().accountId, account.username);
        QCOMPARE(controller.rows().size(), 3); controller.activate(1); QVERIFY(controller.status().contains("Sign out")); controller.dispatch(Action::Back);
        QCOMPARE(provider.context().accountId, account.username); controller.activate(1); controller.activate(1); QVERIFY(provider.context().accountId.isEmpty());
    }
};
QTEST_GUILESS_MAIN(RetroAchievementsTests)
#include "RetroAchievementsTests.moc"
