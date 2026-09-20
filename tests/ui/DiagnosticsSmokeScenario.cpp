#include "DiagnosticsSmokeScenario.h"
#include <QQuickItem>
#include <QCoreApplication>
#include <QTimer>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <memory>

using namespace trainer;
void startDiagnosticsSmoke(QQuickWindow* window, ShellController& shell, SessionState& session, ControllerInput& input,
        DiagnosticsService& reports, SDL_Joystick* joystick, const QString& output, bool& completed, int& warnings, QStringList& diagnostics) {
    const bool fixedDisplay = qEnvironmentVariableIsSet("GAMESCOPE_WAYLAND_DISPLAY");
    auto stage = std::make_shared<int>(0); auto failed = std::make_shared<bool>(false);
    auto timer = new QTimer(window); timer->setInterval(200);
    QObject::connect(timer, &QTimer::timeout, window, [=, &shell, &session, &input, &reports, &completed, &warnings, &diagnostics] {
        const auto check = [&](bool result, const QString& message) {
            if (!result) { *failed = true; diagnostics.append(QString("Diagnostics stage %1: %2").arg(*stage).arg(message)); }
        };
        const auto press = [&](SDL_GameControllerButton button) {
            SDL_JoystickSetVirtualButton(joystick, button, 1); input.poll(); SDL_JoystickSetVirtualButton(joystick, button, 0); input.poll();
        };
        const auto focus = [&](const QString& name) { return window->activeFocusItem() && window->activeFocusItem()->objectName() == name; };
        const auto capture = [&](const QString& name) {
            const auto frame = window->grabWindow(); check(!frame.isNull(), "No rendered frame");
            if (!output.isEmpty()) check(frame.save(output + "/" + name + ".png"), "Cannot save frame");
            auto* item = window->activeFocusItem(); check(item && item->isVisible(), "No visible active focus");
            if (item) {
                const auto outline = item->mapRectToScene(QRectF(-4, -4, item->width() + 8, item->height() + 8));
                check(QRectF(0, 0, window->width(), window->height()).contains(outline), "Focus outside viewport");
                for (auto* parent = item->parentItem(); parent; parent = parent->parentItem()) if (parent->clip())
                    check(parent->mapRectToScene(QRectF(0, 0, parent->width(), parent->height())).adjusted(-0.1,-0.1,0.1,0.1).contains(outline), "Clipped focus");
            }
        };
        constexpr auto a = SDL_CONTROLLER_BUTTON_B, b = SDL_CONTROLLER_BUTTON_A, start = SDL_CONTROLLER_BUTTON_START;
        constexpr auto down = SDL_CONTROLLER_BUTTON_DPAD_DOWN, left = SDL_CONTROLLER_BUTTON_DPAD_LEFT, right = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
        auto* checks = shell.diagnostics();
        if (session.blocked() || reports.saving() || shell.device()->busy()) return;
        switch ((*stage)++) {
        case 0: press(start); press(down); press(a); break;
        case 1:
            check(shell.service() == "diagnostics" && focus("diagnostic-action-0"), "Controller service opened with deterministic focus");
            SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_X, 1);
            SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTX, 23000);
            SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTY, -16000);
            SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, 18000); input.poll(); break;
        case 2:
            check(checks->buttons()[6].toMap()["held"].toBool(), "X live indicator");
            check(checks->axes()[2].toMap()["value"].toDouble() > 0.6, "Right stick moved");
            capture("live-controls");
            SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_X, 0);
            SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTX, 0);
            SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTY, 0);
            SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, -32768); input.poll();
            press(start); break;
        case 3:
            check(shell.menuOpen(), "Start retains global priority"); press(b); break;
        case 4:
            check(focus("diagnostic-action-0"), "Back from menu restores service focus");
            check(checks->buttons()[5].toMap()["seen"].toBool(), "B was recorded even while closing menu");
            press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER); break;
        case 5:
            check(shell.page() == 1 && !shell.serviceOpen(), "R1 switches primary section");
            press(start); press(a); break; // Remembered Controller menu entry.
        case 6:
            check(checks->buttons()[9].toMap()["seen"].toBool(), "R1 history survives reopening");
            if (!fixedDisplay) window->resize(1920,1080); press(a);
            check(window->activeFocusItem() && window->activeFocusItem()->property("depressed").toBool(),
                  "SDL Confirm immediately depresses the active mounted cap without delaying its action");
            break;
        case 7:
            capture("display-1080p");
            input.setEnabled(false); SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_Y, 1); input.poll();
            input.setEnabled(true); input.poll(); break;
        case 8:
            check(input.sample().awaitingNeutral, "Held input waits for neutral");
            check(shell.service() == "diagnostics", "Held Y does not activate another layer"); capture("neutral-gate");
            SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_Y, 0); input.poll();
            press(right); press(a); break;
        case 9: {
            check(checks->status().contains("saved locally"), "Report saved through controller action");
            QFile report(reports.lastReportPath()); check(report.open(QIODevice::ReadOnly), "Report readable");
            const auto json = QJsonDocument::fromJson(report.readAll()).object();
            check(json["input"].toObject()["buttons"].toArray()[SDL_CONTROLLER_BUTTON_X].toObject()["observed"].toBool(), "Report contains observed X");
            capture("report-saved"); press(right); press(a); if (!fixedDisplay) window->resize(1024,768); break;
        }
        case 10:
            check(!checks->buttons()[6].toMap()["seen"].toBool(), "Reset clears earlier checks");
            check(checks->runtimeLines().join(' ').contains(QString("%1 × %2").arg(window->width()).arg(window->height())), "Display follows window changes without a manual refresh");
            capture("letterboxed"); press(b); break;
        case 11:
            check(shell.menuOpen() && focus("menu-1"), "B restores original system-menu entry");
            press(SDL_CONTROLLER_BUTTON_LEFTSHOULDER); check(shell.page() == 0, "L1 remains global");
            if (!fixedDisplay) window->resize(960,540); press(start);
            press(SDL_CONTROLLER_BUTTON_X); // Direct shortcut to Volume.
            break;
        case 12:
            check(focus("menu-7"), "Start Volume has deterministic focus"); capture("start-quick-controls");
            press(SDL_CONTROLLER_BUTTON_X); check(shell.focusIndex() == 1, "X restores previous service entry");
            press(SDL_CONTROLLER_BUTTON_X); press(right); press(a); break;
        case 13:
            check(shell.device()->rows()[0].toMap()["value"].toString().startsWith("40%"), "Quick volume adjusted");
            check(shell.device()->rows()[0].toMap()["value"].toString().contains("Muted"), "Quick mute");
            press(down); press(left); break;
        case 14:
            check(focus("menu-8"), "Quick brightness focus");
            check(shell.device()->rows()[1].toMap()["value"].toString().startsWith("55%"), "Quick brightness adjusted");
            capture("start-brightness"); press(SDL_CONTROLLER_BUTTON_DPAD_UP); press(SDL_CONTROLLER_BUTTON_DPAD_UP); press(a); break;
        case 15:
            check(shell.powerMenu() && focus("menu-3"), "Power opens on safe Cancel");
            capture("power-menu"); press(a); break;
        case 16:
            check(!shell.powerMenu() && focus("menu-6"), "Cancel restores Power entry");
            press(a); press(SDL_CONTROLLER_BUTTON_DPAD_UP); press(a); break;
        case 17:
            check(!shell.modeConfirmation() && shell.notice().contains("Switch Player"), "Unavailable switching is honest");
            press(b); press(b); press(b); break;
        case 18:
            check(!shell.menuOpen() && !shell.powerMenu(), "Back unwinds Power and Start");
            check(warnings == 0, "QML warnings"); completed = true; timer->stop();
            if (!output.isEmpty()) {
                QFile report(output + "/verification.txt");
                if (report.open(QIODevice::WriteOnly)) report.write(((*failed ? QString("FAILED\n") : QString("PASSED\n")) + diagnostics.join('\n')).toUtf8());
            }
            if (*failed) { qCritical().noquote() << diagnostics.join('\n'); QCoreApplication::exit(1); }
            else window->close();
            break;
        }
    });
    timer->start();
}
