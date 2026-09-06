#include "core/input/ControllerInput.h"
#include "core/navigation/ShellController.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include <QtTest>

using namespace trainer;

class SparseLibrary final : public LibraryRepository {
public:
    QList<ResumePoint> points;
    QList<World> worlds() const override { return {}; }
    QList<Adventure> adventures() const override { return {}; }
    QList<ResumePoint> resumePoints() const override { return points; }
    HomeSnapshot home() const override { return {}; }
};

class CoreTests : public QObject {
    Q_OBJECT
private slots:
    void navigationAndBack() {
        MockLibraryRepository repo;
        MockTrainerRepository profiles;
        MockAdventureAdapter adapter;
        DevelopmentPlatformService platform;
        MockPokedexRepository dex;
        MockHallOfFameRepository shellArchive;
        MockAchievementProvider shellAchievements;
        ShellController shell(repo, profiles, adapter, platform, dex, dex, shellArchive, shellAchievements);
        QSignalSpy exit(&shell, &ShellController::exitRequested);
        shell.dispatch(Action::PreviousPage);
        QCOMPARE(shell.page(), 0);
        shell.dispatch(Action::ToggleContinue);
        shell.dispatch(Action::Right);
        QCOMPARE(shell.focusIndex(), 1);
        shell.dispatch(Action::SystemMenu);
        shell.dispatch(Action::ToggleContinue);
        QVERIFY(shell.drawerOpen());
        shell.dispatch(Action::Down); shell.dispatch(Action::Down); // Center remains an informational notice.
        shell.dispatch(Action::Confirm);
        QVERIFY(!shell.notice().isEmpty());
        shell.dispatch(Action::Back);
        QVERIFY(shell.menuOpen());
        shell.dispatch(Action::Back);
        QVERIFY(shell.drawerOpen());
        QCOMPARE(shell.focusIndex(), 1);
        shell.dispatch(Action::Back);
        QVERIFY(!shell.drawerOpen());
        shell.dispatch(Action::Back);
        QCOMPARE(exit.size(), 0);
        shell.dispatch(Action::NextPage);
        shell.dispatch(Action::Down);
        shell.dispatch(Action::Right);
        QCOMPARE(shell.focusIndex(), 4);
        shell.dispatch(Action::NextPage);
        shell.dispatch(Action::PreviousPage);
        QCOMPARE(shell.focusIndex(), 4);
        shell.dispatch(Action::SystemMenu);
        shell.dispatch(Action::NextPage);
        QVERIFY(!shell.menuOpen());
        QCOMPARE(shell.page(), 2);
        shell.goToPage(4);
        shell.dispatch(Action::NextPage);
        QCOMPARE(shell.page(), 4);
    }
    void repositoryAndAdapter() {
        MockLibraryRepository repo;
        MockTrainerRepository profiles;
        MockAdventureAdapter adapter;
        DevelopmentPlatformService platform;
        MockPokedexRepository dex;
        MockHallOfFameRepository shellArchive;
        MockAchievementProvider shellAchievements;
        ShellController shell(repo, profiles, adapter, platform, dex, dex, shellArchive, shellAchievements);
        QCOMPARE(shell.resumePoints().first().toMap()["id"].toString(), "emerald-1");
        QVERIFY(!adapter.capabilities(repo.adventures().first()).screenshots);
        QVERIFY(!platform.canSwitchSession());
        auto adventure = repo.adventures().first();
        auto point = repo.resumePoints().at(1);
        QVERIFY(adapter.resume(adventure, point).success);
        point.adventureId = "unrelated";
        QVERIFY(!adapter.resume(adventure, point).success);
        shell.dispatch(Action::ToggleContinue);
        shell.dispatch(Action::Confirm);
        QVERIFY(shell.notice().contains("No Adventure was launched"));
    }
    void virtualController() {
        SDL_SetMainReady();
        QVERIFY(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0);
        const int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                                                   SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
        QVERIFY2(index >= 0, SDL_GetError());
        SDL_Joystick* joystick = SDL_JoystickOpen(index);
        QVERIFY(joystick);
        {
            ControllerInput input(nullptr, SDL_JoystickInstanceID(joystick));
            QSignalSpy events(&input, &ControllerInput::action);
            input.poll();
            QVERIFY(input.initialized());
            QVERIFY(input.connected());
            const auto button = [&](SDL_GameControllerButton code, bool pressed) {
                QCOMPARE(SDL_JoystickSetVirtualButton(joystick, code, pressed), 0);
                input.poll();
            };
            const std::pair<SDL_GameControllerButton, Action> bindings[] = {
                {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, Action::PreviousPage},
                {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, Action::NextPage},
                {SDL_CONTROLLER_BUTTON_A, Action::Confirm}, {SDL_CONTROLLER_BUTTON_B, Action::Back},
                {SDL_CONTROLLER_BUTTON_Y, Action::ToggleContinue}, {SDL_CONTROLLER_BUTTON_START, Action::SystemMenu},
                {SDL_CONTROLLER_BUTTON_DPAD_UP, Action::Up}, {SDL_CONTROLLER_BUTTON_DPAD_DOWN, Action::Down},
                {SDL_CONTROLLER_BUTTON_DPAD_LEFT, Action::Left}, {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, Action::Right}
            };
            for (const auto& [code, action] : bindings) {
                events.clear(); button(code, true);
                QCOMPARE(events.size(), 1);
                QCOMPARE(qvariant_cast<Action>(events.first().first()), action);
                button(code, false);
            }
            events.clear();
            SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_LEFTX, 9000); input.poll();
            QCOMPARE(events.size(), 0);
            SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_LEFTX, 26000); input.poll();
            QCOMPARE(events.size(), 1);
            QCOMPARE(qvariant_cast<Action>(events.first().first()), Action::Right);
            QTest::qWait(180);
            QCOMPARE(events.size(), 1); // Initial repeat delay.
            QTRY_VERIFY_WITH_TIMEOUT(events.size() >= 2, 500);
            SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_LEFTX, 0); input.poll();
            events.clear();
            button(SDL_CONTROLLER_BUTTON_A, true);
            QTest::qWait(470);
            QCOMPARE(events.size(), 1); // Confirm never repeats while held.
            input.setEnabled(false);
            button(SDL_CONTROLLER_BUTTON_Y, true);
            input.setEnabled(true); input.poll();
            QCOMPARE(events.size(), 1); // Returning while held cannot trigger a stale action.
            button(SDL_CONTROLLER_BUTTON_A, false);
            button(SDL_CONTROLLER_BUTTON_Y, false);
            button(SDL_CONTROLLER_BUTTON_Y, true);
            QCOMPARE(events.size(), 2);
            button(SDL_CONTROLLER_BUTTON_Y, false);
            SDL_JoystickClose(joystick);
            joystick = nullptr;
            QCOMPARE(SDL_JoystickDetachVirtual(index), 0);
            input.poll();
            QVERIFY(!input.connected());
        }
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    }
    void emptyAndMissingData() {
        SparseLibrary repo;
        MockTrainerRepository profiles;
        MockAdventureAdapter adapter;
        DevelopmentPlatformService platform;
        MockPokedexRepository dex;
        MockHallOfFameRepository emptyArchive;
        MockAchievementProvider emptyAchievements;
        ShellController empty(repo, profiles, adapter, platform, dex, dex, emptyArchive, emptyAchievements);
        QCOMPARE(empty.home()["caught"].toString(), QString::fromUtf8("—"));
        empty.dispatch(Action::ToggleContinue);
        empty.dispatch(Action::Right);
        QCOMPARE(empty.focusIndex(), 0);
        empty.dispatch(Action::Confirm);
        QVERIFY(!empty.drawerOpen());
        empty.goToPage(1);
        empty.activate(0);
        QCOMPARE(empty.page(), 0); // Empty library offers an explicit Return Home action.
        repo.points = {{"orphan", "missing-adventure", QDateTime::currentDateTimeUtc(), "", ""}};
        MockHallOfFameRepository missingArchive;
        MockAchievementProvider missingAchievements;
        ShellController missing(repo, profiles, adapter, platform, dex, dex, missingArchive, missingAchievements);
        QCOMPARE(missing.resumePoints().size(), 1);
        QCOMPARE(missing.resumePoints().first().toMap()["title"].toString(), "Unavailable Adventure");
        missing.dispatch(Action::ToggleContinue);
        missing.dispatch(Action::Confirm);
        QVERIFY(missing.notice().contains("has been kept"));
        missing.dispatch(Action::Back);
        QVERIFY(missing.drawerOpen());
    }
};
QTEST_GUILESS_MAIN(CoreTests)
#include "CoreTests.moc"
