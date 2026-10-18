#include "integrations/achievements/RetroAchievementsProvider.h"
#include "integrations/achievements/TrainerAchievementProvider.h"
#include "integrations/achievements/RetroArchAchievementSession.h"
#include "features/halloffame/AchievementAccountController.h"
#include "features/halloffame/AchievementCelebration.h"
#include "core/storage/SqliteAchievementMoments.h"
#include "core/storage/SqliteScreens.h"
#include <QSqlQuery>
#include <QBuffer>
#include <QSettings>
#include <QScopeGuard>
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
    void accountHistoryIsPagedPrivateAndRejectsMalformedProgress() {
        QJsonObject row{{"GameID",42},{"Title","Another console game"},{"ConsoleName","SNES"},{"MaxPossible",20},{"NumAwarded",7},{"NumAwardedHardcore",3}};
        QJsonObject page{{"Count",1},{"Total",2},{"Results",QJsonArray{row}}};
        QVERIFY(readAchievementHistoryPage(page));
        auto invalid=page;invalid["Count"]=2;QVERIFY(!readAchievementHistoryPage(invalid));
        row["NumAwarded"]=-1;invalid["Results"]=QJsonArray{row};invalid["Count"]=1;QVERIFY(!readAchievementHistoryPage(invalid));
        QTemporaryDir dir;RecentLibrary library;
        QVERIFY(writeAchievementAccount(dir.filePath("integrations/retroachievements-account.json"),account));
        std::atomic_int calls=0;std::atomic_bool offline=false;
        RetroAchievementsProvider provider(library,dir.path(),[&](const auto& q){
            if(q.value("r")!="account-history")return AchievementReply{};
            ++calls;if(offline)return AchievementReply{{},false,true};
            if(q.value("y")!=account.token || q.contains("t") || q.value("u")!=account.username)return AchievementReply{};
            auto result=page;
            if(q.value("o")=="1") {auto second=result["Results"].toArray()[0].toObject();second["GameID"]=43;result["Results"]=QJsonArray{second};}
            return AchievementReply{result,true,false};
        });
        QTRY_VERIFY(!provider.accountBusy());
        provider.fetchHistory();QCOMPARE(calls.load(),0);
        provider.setHistoryKey(account.token);QVERIFY(provider.hasHistoryKey());
        provider.fetchHistory();QTRY_VERIFY(!provider.historyBusy());QCOMPARE(provider.accountHistory().size(),1);QVERIFY(provider.historyMore());
        provider.fetchHistory(true);QTRY_VERIFY(!provider.historyBusy());QCOMPARE(provider.accountHistory().size(),2);QVERIFY(!provider.historyMore());
        offline=true;provider.fetchHistory();QTRY_VERIFY(!provider.historyBusy());QCOMPARE(provider.accountHistory().size(),2);QVERIFY(provider.historyStatus().contains("Offline"));offline=false;
        provider.fetchHistory();provider.disconnectAccount();
        QTest::qWait(100);QVERIFY(provider.accountHistory().isEmpty());QVERIFY(!provider.hasHistoryKey());
        QVERIFY(!QFileInfo::exists(dir.filePath("integrations/retroachievements-web-account.json")));
    }
    void serverWarningsAreNotAwardsOrProgressEvenInOldCache() {
        const auto value=fetchAchievements(account,registration("unused"),"",QString(32,'a'),[](const auto& q){
            auto reply=response(q);
            if(q.value("r")=="patch") {
                auto p=reply.body["PatchData"].toObject();auto list=p["Achievements"].toArray();
                list.append(QJsonObject{{"ID",101000001},{"Flags",3},{"Title","Warning"},{"Description","Server diagnostic"}});
                p["Achievements"]=list;reply.body["PatchData"]=p;
            }
            return reply;
        });
        QVERIFY(value);QCOMPARE(value->snapshot.definitions.size(),3);
        auto cache=achievementCache(*value);auto entries=cache["entries"].toArray();
        entries.append(QJsonObject{{"id","101000001"},{"title","Warning"},{"description","Server diagnostic"},{"earned",true},{"hardcore",false}});
        cache["entries"]=entries;const auto loaded=readAchievementCache(cache,value->snapshot.context);
        QVERIFY(loaded);QCOMPARE(loaded->snapshot.definitions.size(),3);QCOMPARE(loaded->snapshot.unlocks.size(),3);
    }
    void nativeEventsAreBoundedAndNeverReadHistoricalOrUnscopedText() {
        RetroArchAchievementEvents parser;
        QVERIFY(parser.feed("Awarding achievement 101: fake\n[INFO] ordinary core: Awarding achievement 101: fake\n").isEmpty());
        const QByteArray lines="[INFO] [RCHEEVOS] Identified game: 42 \"Title\" (aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa)\n"
            "[INFO] [RCHEEVOS] Game 42 loaded, hardcore disabled\n"
            "[INFO] [RCHEEVOS] Captured screenshot for achievement 101\n"
            "[INFO] [RCHEEVOS] Awarding achievement 101: First goal\n";
        QList<RuntimeAchievementEvent> events;
        for(char c:lines)events+=parser.feed(QByteArray(1,c));
        QCOMPARE(events.size(),4);QCOMPARE(events[0].game,"42");QCOMPARE(events[0].hash,QString(32,'a'));
        QCOMPARE(events[1].mode,"Standard");QCOMPARE(events[2].kind,RuntimeAchievementEvent::Captured);
        QCOMPARE(events[3].title,"First goal");
        QCOMPARE(parser.feed("[INFO] [RCHEEVOS] Hardcore disabled\n").first().kind,RuntimeAchievementEvent::ModeChanged);
        QVERIFY(parser.feed("[INFO] [RCHEEVOS] Game 42 completed\n").isEmpty());
        QVERIFY(parser.feed(QByteArray(20000,'x')+"[INFO] [RCHEEVOS] Awarding achievement 101: fake\n").isEmpty());
        QCOMPARE(parser.feed("[INFO] [RCHEEVOS] Unloading game 42\n").first().kind,RuntimeAchievementEvent::Invalidated);
        parser.feed("[INFO] [RCHEEVOS] Awarding achievement 10");parser.clear();QVERIFY(parser.feed("1: old\n").isEmpty());
    }
    void momentsMigratePhotosDeduplicateAndRetainAwardsAfterPruning() {
        const auto connection=QUuid::createUuid().toString();
        {
            auto db=QSqlDatabase::addDatabase("QSQLITE",connection);db.setDatabaseName(":memory:");QVERIFY(db.open());
            QSqlQuery q(db);QVERIFY(q.exec("PRAGMA foreign_keys=ON"));
            QVERIFY(q.exec("CREATE TABLE trainer_profile(id TEXT PRIMARY KEY)"));QVERIFY(q.exec("INSERT INTO trainer_profile VALUES('one'),('two')"));
            QVERIFY(q.exec("CREATE TABLE adventures(id TEXT PRIMARY KEY,title TEXT,revision INTEGER)"));QVERIFY(q.exec("INSERT INTO adventures VALUES('game','A game',1)"));
            QVERIFY(migrateScreens(db).isEmpty());
            QImage frame(160,144,QImage::Format_RGB32);frame.fill(Qt::darkGreen);
            QByteArray jpeg;QBuffer buffer(&jpeg);buffer.open(QIODevice::WriteOnly);QVERIFY(frame.save(&buffer,"JPEG"));
            const auto now=QDateTime::currentDateTimeUtc();
            QVERIFY(appendScreen(db,{"original","one","multiverse","game",1,"original",now,jpeg},"A game",true).isEmpty());
            const auto before=readScreens(db,"one");QVERIFY(migrateAchievementMoments(db).isEmpty());QCOMPARE(readScreens(db,"one"),before);
            AchievementMoment m{"one","Example","42","101","Standard","game","session",QString(32,'a'),"Award","123",now,now,frame};
            bool inserted=false;QVERIFY(appendAchievementMoment(db,"one",m,inserted).isEmpty());QVERIFY(inserted);
            const auto photo=readAchievementMoments(db,"one").first().toMap()["capture"].toString();QVERIFY(!photo.isEmpty());
            m.session="another-session";m.account="EXAMPLE";QVERIFY(appendAchievementMoment(db,"one",m,inserted).isEmpty());QVERIFY(!inserted);
            QCOMPARE(readScreens(db,"one").size(),2);
            QVERIFY(!appendAchievementMoment(db,"two",m,inserted).isEmpty());QVERIFY(readAchievementMoments(db,"two").isEmpty());
            m.mode="Hardcore";QVERIFY(appendAchievementMoment(db,"one",m,inserted).isEmpty());QVERIFY(inserted);
            for(int i=0;i<101;++i)QVERIFY(appendScreen(db,{"later-"+QString::number(i),"one","multiverse","game",1,"build",now.addSecs(i+1),jpeg},"A game").isEmpty());
            QVERIFY(pruneScreens(db,"one").isEmpty());QCOMPARE(readScreens(db,"one").size(),101);
            const auto moments=readAchievementMoments(db,"one");QCOMPARE(moments.size(),2);
            for(const auto& v:moments){QVERIFY(v.toMap()["expired"].toBool());QVERIFY(v.toMap()["source"].toString().isEmpty());}
            QVERIFY(appendAchievementMoment(db,"one",m,inserted).isEmpty());QVERIFY(!inserted); // No replacement photo on replay.
            QVERIFY(q.exec("SELECT jpeg FROM screens WHERE id='original'"));QVERIFY(q.next());QCOMPARE(q.value(0).toByteArray(),jpeg);q.finish();
            QVERIFY(q.exec("PRAGMA foreign_key_check"));QVERIFY(!q.next());q.finish();
            QVERIFY(q.exec("DELETE FROM trainer_profile WHERE id='one'"));QVERIFY(readAchievementMoments(db,"one").isEmpty());
        }
        QSqlDatabase::removeDatabase(connection);
    }
    void runtimeHashEnablesOtherPlatformsWithoutFilenameOrRawHashGuessing() {
        QTemporaryDir dir;RecentLibrary library;const auto path=dir.filePath("original.nes");write(path,"Content-free fixture");
        library.value=registration(path);library.value.adventure.platformId="nes";
        QVERIFY(writeAchievementAccount(dir.filePath("integrations/retroachievements-account.json"),account));
        RetroAchievementsProvider provider(library,dir.path(),response);QTRY_VERIFY(!provider.accountBusy());QVERIFY(provider.sets().isEmpty());
        provider.observeRuntimeGame(library.value.adventure.id,QString(32,'a'),"43");QTRY_VERIFY(!provider.accountBusy());QVERIFY(provider.sets().isEmpty());
        provider.observeRuntimeGame(library.value.adventure.id,QString(32,'a'),"42");QTRY_VERIFY(!provider.accountBusy());QCOMPARE(provider.sets().size(),1);
        QCOMPARE(provider.sets().first().adventureId,library.value.adventure.id);
    }
    void liveCaptureIsBoundToSessionAndHistoricalRefreshDoesNotCapture() {
        QSettings settings;const auto previousVolume=settings.value("achievements/volume");
        settings.setValue("achievements/volume",0);
        const auto restoreVolume=qScopeGuard([&]{if(previousVolume.isValid())settings.setValue("achievements/volume",previousVolume);else settings.remove("achievements/volume");});
        QTemporaryDir dir;LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());
        bool done=false;store.saveAsync({"owner","Fixture","compass",{},QDateTime::currentDateTimeUtc()},this,[&](auto r){QVERIFY(r.success);done=true;});QTRY_VERIFY(done);
        const auto path=dir.filePath("original.gba");write(path,"Content-free fixture");auto record=registration(path);
        record.adventure.worldId.clear();record.adventure.domain="multiverse";done=false;
        store.saveAdventureAsync(record,this,[&](auto r){QVERIFY2(r.success,qPrintable(r.error));done=true;});QTRY_VERIFY(done);
        QVERIFY(writeAchievementAccount(dir.filePath("integrations/retroachievements-account.json"),account));
        RetroAchievementsProvider provider(store,dir.path(),response);QTRY_VERIFY(!provider.accountBusy());
        AchievementCelebration live(&store,provider);
        QSignalSpy notices(&live,&AchievementCelebration::notice);
        const QVariantMap controls{{"directory",dir.path()},{"account",account.username},{"session","fresh"}};
        const QByteArray identity="[INFO] [RCHEEVOS] Hardcore disabled\n[INFO] [RCHEEVOS] Identified game: 42 (aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa)\n[INFO] [RCHEEVOS] Game 42 loaded, hardcore disabled\n";
        const QByteArray award="[INFO] [RCHEEVOS] Captured screenshot for achievement 103\n[INFO] [RCHEEVOS] Awarding achievement 103: Goal\n";
        QImage frame(160,144,QImage::Format_RGB32);frame.fill(Qt::darkBlue);QVERIFY(frame.save(dir.filePath("original-cheevo-103.png")));
        live.begin(record.adventure.id,123,controls);live.feed(identity);QTRY_VERIFY(!provider.accountBusy());
        QVERIFY(store.achievementMoments().isEmpty()); // Definitions and old unlocks never capture.
        live.feed(award);QTRY_VERIFY(!store.achievementMoments().isEmpty() || !notices.isEmpty());
        QVERIFY2(notices.isEmpty(),notices.isEmpty()?"":qPrintable(notices.first().first().toString()));
        QCOMPARE(store.achievementMoments().size(),1);
        QCOMPARE(store.screenMedia()->size(),1);QVERIFY(!store.achievementMoments().first().toMap()["source"].toString().isEmpty());
        QVERIFY(live.current().isEmpty());live.setPresentable(true);QCOMPARE(live.current()["id"].toString(),"103");
        QVERIFY(live.current()["played"].toBool());live.setPresentable(false);QVERIFY(live.current().isEmpty());
        live.setPresentable(true);QCOMPARE(live.current()["id"].toString(),"103");
        live.feed(award);QTest::qWait(100);QCOMPARE(store.achievementMoments().size(),1);
        live.end();live.begin(record.adventure.id,124,controls);live.feed(identity+award);QTest::qWait(150);QCOMPARE(store.achievementMoments().size(),1);
        live.end();live.begin(record.adventure.id,125,controls);live.feed(identity);
        live.feed("[INFO] [RCHEEVOS] Awarding achievement 102: Late\n[INFO] [RCHEEVOS] Captured screenshot for achievement 102\n");
        live.end();QTest::qWait(1400);QCOMPARE(store.achievementMoments().size(),1); // Late callback cannot attach to next session.
        live.begin(record.adventure.id,126,controls);live.feed(identity);live.feed("[INFO] [RCHEEVOS] Game 42 loaded, hardcore disabled, spectating\n");live.feed(award);
        QTest::qWait(50);QCOMPARE(store.achievementMoments().size(),1);
        live.begin(record.adventure.id,127,controls);live.feed(identity);
        live.feed("[INFO] [RCHEEVOS] Awarding achievement 102: No photo\n[INFO] [RCHEEVOS] Failed to capture screenshot for achievement 102\n");
        QTRY_COMPARE(store.achievementMoments().size(),2);QCOMPARE(store.screenMedia()->size(),1);
        bool missing=false;for(const auto& v:store.achievementMoments())if(v.toMap()["achievement"]=="102")missing=v.toMap()["source"].toString().isEmpty()&&!v.toMap()["expired"].toBool();
        QVERIFY(missing); // Honest absence, never an exit screenshot replacement.
        const auto pcm=AchievementCelebration::jinglePcm();QCOMPARE(pcm.size(),37440);QVERIFY(pcm.count(char(0))<pcm.size()/2);
    }
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
    void optionalBadgeIdentitySurvivesCacheWithoutChangingUnlocks() {
        for (const QString name : {QString("012345"), QString("../secret"), QString("https://other.invalid/icon"), QString()}) {
            auto value = fetchAchievements(account, registration("/unused.gba"), "Kanto", QString(32, 'a'), [&](const auto& q) {
                auto reply = response(q);
                if (q.value("r") == "patch") {
                    auto patch = reply.body["PatchData"].toObject(); auto entries = patch["Achievements"].toArray();
                    auto first = entries[0].toObject(); first["BadgeName"] = name; entries[0] = first;
                    patch["Achievements"] = entries; reply.body["PatchData"] = patch;
                }
                return reply;
            });
            QVERIFY(value);
            const auto expected = name == "012345" ? name : QString();
            QCOMPARE(value->snapshot.definitions.first().badgeName, expected);
            auto cache = achievementCache(*value); auto loaded = readAchievementCache(cache, value->snapshot.context);
            QVERIFY(loaded); QCOMPARE(loaded->snapshot.definitions.first().badgeName, expected);
            QCOMPARE(loaded->snapshot.unlocks.first().unlocked, std::optional(true));
            auto entries = cache["entries"].toArray(); auto first = entries[0].toObject(); first.remove("badge"); entries[0] = first; cache["entries"] = entries;
            loaded = readAchievementCache(cache, value->snapshot.context);
            QVERIFY(loaded); QVERIFY(loaded->snapshot.definitions.first().badgeName.isEmpty());
        }
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
        QCOMPARE(controller.rows().size(), 4); controller.activate(1); QVERIFY(controller.status().contains("Sign out")); controller.dispatch(Action::Back);
        QCOMPARE(provider.context().accountId, account.username); controller.activate(1); controller.activate(1); QVERIFY(provider.context().accountId.isEmpty());
    }
};
QTEST_GUILESS_MAIN(RetroAchievementsTests)
#include "RetroAchievementsTests.moc"
