#include "LaunchSmokeScenario.h"
#include "core/navigation/AdventureLaunchController.h"
#include <QCoreApplication>
#include <QQuickItem>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QTimer>
#include <memory>

using namespace trainer;
void startLaunchSmoke(QQuickWindow* window, ShellController& shell, SessionState& session, LocalStateStore& store,
        ControllerInput& input, ProbeAdventureAdapter& adapter, SDL_Joystick* joystick, const QString& screenshotDir,
        bool& completed, int& warnings, QStringList& diagnostics) {
    auto* process = new ProcessService(window);
    auto* launch = new AdventureLaunchController(*process, window);
    const auto program = QDir(QCoreApplication::applicationDirPath()).filePath(
#ifdef Q_OS_WIN
        "trainer_process_probe.exe"
#else
        "trainer_process_probe"
#endif
    );
    auto attempt = std::make_shared<int>(0), stage = std::make_shared<int>(0), returns = std::make_shared<int>(0);
    auto expected = std::make_shared<QJsonObject>(); auto failed = std::make_shared<bool>(false);
    adapter.request = [&, attempt, expected, program, launch] {
        *expected = shell.navigationState();
        const ProcessCommand command{*attempt == 1 ? program + ".missing" : program, *attempt == 2 ? QStringList{"crash"} : QStringList{}, {}};
        return launch->launch(command, *expected);
    };
    QObject::connect(launch, &AdventureLaunchController::checkpointRequested, &store, [&, launch](quint64 token, const QJsonObject& state) {
        store.saveNavigation(state, launch, [launch, token](const QString& error) { launch->checkpointCompleted(token, error); });
    });
    QObject::connect(launch, &AdventureLaunchController::suspendRequested, window, [&, window, joystick] {
        input.setEnabled(false); window->hide();
        SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 1); input.poll();
    });
    QObject::connect(launch, &AdventureLaunchController::restoreRequested, window, [&, returns, window](const QJsonObject& state) {
        ++*returns; shell.restoreNavigation(state); window->show(); window->requestActivate(); input.setEnabled(true);
    });
    auto timer = new QTimer(window); timer->setInterval(220);
    QObject::connect(timer, &QTimer::timeout, window, [=, &shell, &session, &input, &completed, &warnings, &diagnostics] {
        const auto check = [&](bool condition, const QString& text) { if (!condition) { *failed = true; diagnostics.append(QString("Launch stage %1: %2").arg(*stage).arg(text)); } };
        const auto press = [&](SDL_GameControllerButton button) {
            SDL_JoystickSetVirtualButton(joystick, button, 1); input.poll(); SDL_JoystickSetVirtualButton(joystick, button, 0); input.poll();
        };
        if (session.blocked() || launch->active()) return;
        if (*stage == 0) {
            if (shell.page() == 0) press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
            check(shell.worlds()->route() == "detail", "Restored Adventure detail before launch");
            press(SDL_CONTROLLER_BUTTON_DPAD_LEFT); ++*stage; return;
        }
        if (*stage % 2 == 1 && *stage < 7) { press(SDL_CONTROLLER_BUTTON_A); ++*stage; return; }
        if (*stage <= 6) {
            check(*returns == *attempt + 1, "Exactly one return signal per attempt");
            check(window->isVisible() && shell.navigationState() == *expected, "Window and navigation restored");
            check(window->activeFocusItem() && window->activeFocusItem()->objectName() == "world-action-launch", "Launch focus restored");
            check(launch->state() == (*attempt == 0 ? "returned" : "failed"), "Expected process result");
            // A shoulder button held across handoff must not switch the returned page.
            input.poll(); check(shell.page() == 1, "Held external input was suppressed");
            SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 0); input.poll();
            const auto frame = window->grabWindow(); check(!frame.isNull(), "Empty returned frame");
            if (!screenshotDir.isEmpty()) check(frame.save(screenshotDir + QString("/launch-return-%1.png").arg(*attempt)), "Cannot capture returned frame");
            ++*attempt; ++*stage; return;
        }
        check(warnings == 0, "QML warnings"); completed = true; timer->stop();
        if (!screenshotDir.isEmpty()) {
            QFile report(screenshotDir + "/launch-verification.txt");
            if (report.open(QIODevice::WriteOnly)) report.write(((*failed ? QString("FAILED\n") : QString("PASSED\n")) + diagnostics.join('\n')).toUtf8());
        }
        if (*failed) { qCritical().noquote() << diagnostics.join('\n'); QCoreApplication::exit(1); }
        else window->close();
    });
    timer->start();
}
