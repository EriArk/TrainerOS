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
namespace {
class HomeProgressFixture final : public GameProgressProvider {
public:
    using GameProgressProvider::GameProgressProvider;
    QString selected = "home-0";
    GameProgress value;
    QString adventureId() const override { return selected; }
    GameProgress snapshot() const override { return value; }
    void publish() { emit changed(); }
};
}
void startHomeSmoke(QQuickWindow* window, ShellController& shell, SessionState& session, LocalStateStore& store,
        ControllerInput& input, ProbeAdventureAdapter& adapter, SDL_Joystick* joystick, bool reopen,
        const QString& screenshotDir, bool& completed, int& warnings, QStringList& diagnostics) {
    auto* process = new ProcessService(window);
    auto* launch = new AdventureLaunchController(*process, window);
    auto* history = new PlayHistoryController(*launch, store, window);
    auto* progress = new HomeProgressFixture(window);
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
            press(SDL_CONTROLLER_BUTTON_A); check(focusIs("resume-4"), "Back restores selected card");
            press(SDL_CONTROLLER_BUTTON_B); break;
        case 5:
            check(*starts == 0 && *returns == 0 && shell.notice().isEmpty(), "Selecting a card must never launch");
            check(!shell.drawerOpen() && focusIs("home-launch") && shell.home()["adventureId"] == "home-0", "Selection rebuilds Home and focuses main button");
            check(shell.home()["recordedTime"] == "12 min" && shell.home()["badges"] == "—", "Real duration and unknown progress remain separate");
            progress->value.availability = ProgressAvailability::Available;
            progress->value.badgeMask = 0xa5; progress->value.caught = 241;
            progress->value.badgeSet = "kanto";
            progress->value.message = "Last in-game save · National Pokédex";
            shell.configureProgress(progress);
            window->resize(1920, 1080); break;
        case 6:
            check(shell.home()["badges"] == "4" && shell.home()["caught"] == "241"
                && shell.home()["badgeSlots"].toList().size() == 8 && shell.home()["badgeSet"] == "kanto", "Verified save fields reach Home independently of recorded time");
            capture("home-selected-1080p"); press(SDL_CONTROLLER_BUTTON_B); break;
        case 7:
            check(*starts == 1 && *returns == 1 && focusIs("home-launch"), "Main button alone launches and restores Home");
            check(store.recentSessions().first().adventureId == "home-0" && store.recentSessions().size() == 5, "History replaces the latest card without duplicates");
            check(history->error().isEmpty(), "History writes succeeded");
            for (auto button : {SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_LEFT}) press(button);
            check(focusIs("home-launch") && !shell.drawerOpen(), "D-pad cannot redirect Home's fixed A action");
            press(SDL_CONTROLLER_BUTTON_B); break;
        case 8:
            check(*starts == 2 && *returns == 2 && !shell.drawerOpen(), "A launches immediately after arbitrary D-pad input");
            press(SDL_CONTROLLER_BUTTON_Y); check(shell.drawerOpen(), "Y opens the selector independently"); press(SDL_CONTROLLER_BUTTON_A);
            check(focusIs("home-launch"), "Cancel restores the fixed page action");
            press(SDL_CONTROLLER_BUTTON_START); press(SDL_CONTROLLER_BUTTON_DPAD_DOWN); press(SDL_CONTROLLER_BUTTON_DPAD_DOWN); press(SDL_CONTROLLER_BUTTON_B);
            check(!shell.notice().isEmpty() && *starts == 2, "A in the system menu cannot launch Home's Adventure");
            press(SDL_CONTROLLER_BUTTON_A); press(SDL_CONTROLLER_BUTTON_A);
            press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER); press(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
            check(focusIs("home-launch") && shell.home()["adventureId"] == "home-0", "Shoulders retain the selected Home Adventure");
            progress->selected = "another-adventure"; progress->publish();
            check(shell.home()["badges"] == "—" && shell.home()["caught"] == "—", "An unrelated Adventure cannot populate Home");
            progress->selected = "home-0"; progress->value = {};
            progress->value.message = "Progress is not supported for this edition yet."; progress->publish();
            check(shell.home()["badges"] == "—" && shell.home()["badgeSlots"].toList().isEmpty(), "Unsupported progress stays unknown");
            break;
        case 9:
            check(shell.navigationState()["homeAdventure"] == "home-0" && shell.home()["adventureId"] == "home-0", "Explicit Home choice survives restart");
            check(store.recentSessions().size() == 5 && store.recordedSeconds("home-0").has_value(), "History survives restart");
            check(!reopen || *starts == 0, "Reopening the journal does not launch anything");
            capture(reopen ? "home-reopened" : "home-return");
            if (reopen) { *stage = 13; break; }
            progress->value.availability = ProgressAvailability::Available;
            progress->value.badgeMask = 255; progress->value.caught = 386; progress->value.badgeSet = "kanto";
            progress->value.message = "Last in-game save · National Pokédex"; progress->publish();
            window->resize(960, 540); break;
        case 10:
            for (int i = 0; i < 8; ++i) {
                QQuickItem* badge = nullptr;
                QList<QQuickItem*> pending{window->contentItem()};
                while (!pending.isEmpty()) {
                    auto* item = pending.takeLast();
                    if (item->objectName() == "home-badge-" + QString::number(i)) { badge = item; break; }
                    pending.append(item->childItems());
                }
                check(badge && badge->width() >= 26 && badge->height() >= 40, "Large badge remains readable at handheld size");
                if (badge) {
                    const auto bounds = badge->mapRectToScene(badge->boundingRect());
                    check(bounds.left() >= 0 && bounds.right() <= window->width() && bounds.bottom() <= window->height(), "Badge stays inside the viewport");
                }
            }
            check(focusIs("home-launch"), "Decorative badges add no directional focus stops");
            capture("home-badges-kanto-960");
            progress->value.badgeSet = "hoenn"; progress->publish(); break;
        case 11:
            capture("home-badges-hoenn-960");
            progress->value.badgeMask = 0; progress->publish(); break;
        case 12:
            check(shell.home()["badges"] == "0", "An empty badge tray represents verified zero, not unknown");
            capture("home-badges-empty-960"); break;
        default:
            check(warnings == 0, "QML warnings"); completed = true; timer->stop();
            if (*failed) { qCritical().noquote() << diagnostics.join('\n'); QCoreApplication::exit(1); }
            else window->close();
        }
    });
    timer->start();
}
