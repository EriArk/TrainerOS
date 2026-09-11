#include "HomeSmokeScenario.h"
#include "features/home/PlayHistoryController.h"
#include <QCoreApplication>
#include <QQuickItem>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QTimer>
#include <memory>

using namespace trainer;
void startHomeSmoke(QQuickWindow* window, ShellController& shell, SessionState& session, LocalStateStore& store,
        ControllerInput& input, ProbeAdventureAdapter& adapter, SDL_Joystick* joystick, bool reopen,
        const QString& screenshotDir, bool& completed, int& warnings, QStringList& diagnostics) {
    auto* process = new ProcessService(window);
    auto* launch = new AdventureLaunchController(*process, window);
    auto* history = new PlayHistoryController(*launch, store, window);
    auto stage = std::make_shared<int>(0), starts = std::make_shared<int>(0), returns = std::make_shared<int>(0);
    auto failed = std::make_shared<bool>(false);
    const auto program = QDir(QCoreApplication::applicationDirPath()).filePath(
#ifdef Q_OS_WIN
        "trainer_process_probe.exe"
#else
        "trainer_process_probe"
#endif
    );
    adapter.request = [&, launch, program] { return launch->launch({program, {}, {}}, shell.navigationState(), shell.home()["adventureId"].toString()); };
    QObject::connect(launch, &AdventureLaunchController::adventureStarted, window, [starts](const QString&) { ++*starts; });
    QObject::connect(launch, &AdventureLaunchController::checkpointRequested, &store, [&, launch](quint64 token, const QJsonObject& state) {
        store.saveNavigation(state, launch, [launch, token](const QString& error) { launch->checkpointCompleted(token, error); });
    });
    QObject::connect(launch, &AdventureLaunchController::changed, &session, [&, launch] { session.setAdventureActive(launch->active()); });
    QObject::connect(launch, &AdventureLaunchController::suspendRequested, window, [&, window] { input.setEnabled(false); window->hide(); });
    QObject::connect(launch, &AdventureLaunchController::restoreRequested, window, [&, window, returns](const QJsonObject& state) {
        ++*returns; shell.restoreNavigation(state); window->show(); window->requestActivate(); input.setEnabled(true);
    });
    auto* timer = new QTimer(window); timer->setInterval(250);
    QObject::connect(timer, &QTimer::timeout, window, [=, &shell, &session, &store, &input, &completed, &warnings, &diagnostics] {
        const auto check = [&](bool condition, const QString& text) { if (!condition) { *failed = true; diagnostics.append(QString("Home stage %1: %2").arg(*stage).arg(text)); } };
        const auto press = [&](SDL_GameControllerButton button) {
            SDL_JoystickSetVirtualButton(joystick, button, 1); input.poll(); SDL_JoystickSetVirtualButton(joystick, button, 0); input.poll();
        };
        const auto focusIs = [&](const QString& name) { return window->activeFocusItem() && window->activeFocusItem()->objectName() == name; };
        const auto capture = [&](const QString& name) { if (!screenshotDir.isEmpty()) check(window->grabWindow().save(screenshotDir + "/" + name + ".png"), "Capture failed"); };
        if (session.blocked() || launch->active() || store.pending()) return;
        switch ((*stage)++) {
        case 0:
            if (reopen) { *stage = 9; break; }
            for (int i = 0; i < 5; ++i) {
                AdventureRegistration record; record.adventure.id = "home-" + QString::number(i);
                record.adventure.title = "Original Adventure " + QString::number(i + 1);
                record.adventure.worldId = i % 2 ? "johto" : "hoenn"; record.adventure.adapterId = "test-process";
                record.contentPath = program;
                store.saveAdventureAsync(record, window, [&, window, i, failed](LibraryWriteResult result) {
                    if (!result.success) { *failed = true; return; }
                    PlaySession value{"seed-" + QString::number(i), "home-" + QString::number(i), QDateTime::fromString("2026-09-01T10:00:00.000Z", Qt::ISODateWithMs).addSecs(i * 1000), {}, {}, PlaySessionOutcome::Running};
                    store.saveSessionAsync(value, window, [&, window, value, failed](const QString& error) mutable {
                        if (!error.isEmpty()) { *failed = true; return; }
                        value.endedAt = value.startedAt.addSecs(720); value.elapsedSeconds = 720; value.outcome = PlaySessionOutcome::Returned;
                        store.saveSessionAsync(value, window, [failed](const QString& e) { if (!e.isEmpty()) *failed = true; });
                    });
                });
            }
            break;
        case 1:
            check(store.recentSessions().size() == 5 && shell.home()["adventureId"] == "home-4", "Latest launch is the default Home");
            check(focusIs("home-launch"), "Main button has initial focus");
            press(SDL_CONTROLLER_BUTTON_Y); break;
        case 2:
            for (int i = 0; i < 4; ++i) press(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
            break;
        case 3: {
            check(focusIs("resume-4"), "Controller reaches fifth card");
            auto* card = window->activeFocusItem();
            if (card) { const auto rect = card->mapRectToScene(card->boundingRect()); check(rect.left() >= 0 && rect.right() <= window->width(), "Scrolled card stays inside screen"); }
            capture("home-recent-scroll"); press(SDL_CONTROLLER_BUTTON_START); break;
        }
        case 4:
            check(shell.menuOpen(), "Start opens system menu above selector");
            press(SDL_CONTROLLER_BUTTON_B); check(focusIs("resume-4"), "Back restores selected card");
            press(SDL_CONTROLLER_BUTTON_A); break;
        case 5:
            check(*starts == 0 && *returns == 0 && shell.notice().isEmpty(), "Selecting a card must never launch");
            check(!shell.drawerOpen() && focusIs("home-launch") && shell.home()["adventureId"] == "home-0", "Selection rebuilds Home and focuses main button");
            check(shell.home()["recordedTime"] == "12 min" && shell.home()["badges"] == "—", "Real duration and unknown progress remain separate");
            window->resize(1920, 1080); break;
        case 6:
            capture("home-selected-1080p"); press(SDL_CONTROLLER_BUTTON_A); break;
        case 7:
            check(*starts == 1 && *returns == 1 && focusIs("home-launch"), "Main button alone launches and restores Home");
            check(store.recentSessions().first().adventureId == "home-0" && store.recentSessions().size() == 5, "History replaces the latest card without duplicates");
            check(history->error().isEmpty(), "History writes succeeded");
            press(SDL_CONTROLLER_BUTTON_DPAD_LEFT); check(focusIs("continue-toggle"), "D-pad reaches the selector button");
            press(SDL_CONTROLLER_BUTTON_A); break;
        case 8:
            check(shell.drawerOpen(), "A on the selector button opens it"); press(SDL_CONTROLLER_BUTTON_B);
            check(focusIs("continue-toggle"), "Cancel restores selector opener");
            press(SDL_CONTROLLER_BUTTON_DPAD_UP); press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER); press(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
            check(focusIs("home-launch") && shell.home()["adventureId"] == "home-0", "Shoulders retain the selected Home Adventure");
            break;
        case 9:
            check(shell.navigationState()["homeAdventure"] == "home-0" && shell.home()["adventureId"] == "home-0", "Explicit Home choice survives restart");
            check(store.recentSessions().size() == 5 && store.recordedSeconds("home-0").has_value(), "History survives restart");
            check(!reopen || *starts == 0, "Reopening the journal does not launch anything");
            capture(reopen ? "home-reopened" : "home-return"); break;
        default:
            check(warnings == 0, "QML warnings"); completed = true; timer->stop();
            if (*failed) { qCritical().noquote() << diagnostics.join('\n'); QCoreApplication::exit(1); }
            else window->close();
        }
    });
    timer->start();
}
