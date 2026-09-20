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
            if (item && shell.service()=="settings" && (item->objectName().startsWith("settings-control-") || item->objectName().startsWith("achievement-account-"))) {
                auto* status=window->findChild<QQuickItem*>("settings-status");
                check(status && item->mapToScene(QPointF(0,item->height()+3)).y() <= status->mapToScene(QPointF(0,0)).y(), "Focused setting cannot overlap status text");
            }
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
        case 0: press(start); press(a); for(int i=0;i<7;++i) press(down); press(a); press(a); break;
        case 1:
            check(shell.service() == "diagnostics" && focus("diagnostic-action-0"), "Controller service opened with deterministic focus");
            SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_Y, 1);
            SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTX, 23000);
            SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTY, -16000);
            SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, 18000); input.poll(); break;
        case 2:
            check(checks->buttons()[6].toMap()["held"].toBool(), "X live indicator");
            check(checks->axes()[2].toMap()["value"].toDouble() > 0.6, "Right stick moved");
            capture("live-controls");
            SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_Y, 0);
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
            press(start); press(a); for(int i=0;i<7;++i) press(down); press(a); press(a); break;
        case 6:
            check(checks->buttons()[9].toMap()["seen"].toBool(), "R1 history survives reopening");
            if (!fixedDisplay) window->resize(1920,1080); press(a);
            check(window->activeFocusItem() && window->activeFocusItem()->property("depressed").toBool(),
                  "SDL Confirm immediately depresses the active mounted cap without delaying its action");
            break;
        case 7:
            capture("display-1080p");
            input.setEnabled(false); SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_X, 1); input.poll();
            input.setEnabled(true); input.poll(); break;
        case 8:
            check(input.sample().awaitingNeutral, "Held input waits for neutral");
            check(shell.service() == "diagnostics", "Held Y does not activate another layer"); capture("neutral-gate");
            SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_X, 0); input.poll();
            press(right); press(a); break;
        case 9: {
            check(checks->status().contains("saved locally"), "Report saved through controller action");
            QFile report(reports.lastReportPath()); check(report.open(QIODevice::ReadOnly), "Report readable");
            const auto json = QJsonDocument::fromJson(report.readAll()).object();
            check(json["input"].toObject()["buttons"].toArray()[SDL_CONTROLLER_BUTTON_Y].toObject()["observed"].toBool(), "Report contains observed X");
            capture("report-saved"); press(right); press(a); if (!fixedDisplay) window->resize(1024,768); break;
        }
        case 10:
            check(!checks->buttons()[6].toMap()["seen"].toBool(), "Reset clears earlier checks");
            check(checks->runtimeLines().join(' ').contains(QString("%1 × %2").arg(window->width()).arg(window->height())), "Display follows window changes without a manual refresh");
            capture("letterboxed"); press(b); break;
        case 11:
            check(shell.service()=="settings" && focus("settings-control-0"), "B restores original system-menu entry");
            press(SDL_CONTROLLER_BUTTON_LEFTSHOULDER); check(shell.page() == 0, "L1 remains global");
            if (!fixedDisplay) window->resize(960,540); press(start);
            press(SDL_CONTROLLER_BUTTON_Y); // Direct shortcut to Volume.
            break;
        case 12:
            check(focus("menu-7"), "Start Volume has deterministic focus"); capture("start-quick-controls");
            press(SDL_CONTROLLER_BUTTON_Y); check(shell.focusIndex() == 0, "X restores previous service entry");
            press(SDL_CONTROLLER_BUTTON_Y); press(right); press(a); break;
        case 13:
            check(shell.device()->rows()[0].toMap()["value"].toString().startsWith("40%"), "Quick volume adjusted");
            check(shell.device()->rows()[0].toMap()["value"].toString().contains("Muted"), "Quick mute");
            press(down); press(left); break;
        case 14:
            check(focus("menu-8"), "Quick brightness focus");
            check(shell.device()->rows()[1].toMap()["value"].toString().startsWith("55%"), "Quick brightness adjusted");
            capture("start-brightness"); press(SDL_CONTROLLER_BUTTON_Y); for(int i=0;i<5;++i) press(down); press(a); break;
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
            press(start); for (int i=0;i<10;++i) press(SDL_CONTROLLER_BUTTON_DPAD_UP); for(int i=0;i<2;++i) press(down); press(a);
            for (int i=0;i<4;++i) press(down); press(a); break;
        case 19:
            check(shell.service()=="settings" && focus("settings-control-0"), "Trainer settings entry");
            capture("trainer-settings"); press(down); press(a); break;
        case 20:
            check(shell.hall()->account()->isOpen(), "Settings opens the shared RA controller");
            capture("trainer-account-service"); press(b); press(down); press(a); break;
        case 21:
            check(shell.service()=="trainer-setup" && focus("setup-action-0"), "Isolated setup preview");
            press(a); break;
        case 22:
            capture("trainer-welcome"); press(a); break;
        case 23:
            capture("trainer-identity"); press(a); break;
        case 24:
            check(shell.keyboard()->isOpen(), "Controller name entry"); capture("trainer-name-keyboard");
            press(a); press(down); press(down); press(down); press(right); press(a); break;
        case 25:
            check(!shell.keyboard()->isOpen() && shell.trainerSetup()->name()=="A", "Keyboard applies sample name");
            press(down); press(a); press(down); press(a); press(down); press(a); break;
        case 26:
            check(shell.trainerSetup()->keypad() && focus("setup-key-0"), "Numeric PIN keypad focus"); capture("trainer-pin");
            press(a); press(right); press(a); press(right); press(a); press(down); press(left); press(left); press(a);
            press(down); press(down); press(down); press(a); break;
        case 27:
            check(shell.trainerSetup()->stage()=="repeat" && shell.trainerSetup()->pinMask().isEmpty(), "Fresh repeated PIN entry");
            press(a); press(right); press(a); press(right); press(a); press(down); press(left); press(left); press(a);
            press(down); press(down); press(down); press(a); break;
        case 28:
            check(shell.trainerSetup()->stage()=="review", "Matching PIN reaches review"); capture("trainer-review");
            press(start); break;
        case 29:
            check(shell.menuOpen(), "Start overlays preview"); press(b); break;
        case 30:
            check(focus("setup-action-0"), "Start Back restores preview focus"); press(a); break;
        case 31:
            check(shell.trainerSetup()->stage()=="done", "Preview has no profile mutation"); capture("trainer-preview-finished");
            press(a); press(down); press(a); break;
        case 32:
            check(shell.trainerSetup()->stage()=="chooser", "Chooser reached with controller"); capture("trainer-chooser");
            press(down); press(a); break;
        case 33:
            check(shell.trainerSetup()->stage()=="unlock", "Sample locked Trainer keypad"); capture("trainer-unlock");
            press(a); press(b); break;
        case 34:
            check(shell.trainerSetup()->stage()=="chooser" && shell.trainerSetup()->pinMask().isEmpty(), "Back clears secret draft and restores card");
            press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER); break;
        case 35:
            check(!shell.serviceOpen() && shell.trainerSetup()->name().isEmpty(), "R1 cancels preview and clears draft");
            press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
            SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,32767); input.poll();
            SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768); input.poll(); break;
        case 36:
            check(shell.centerFace() && focus("party-slot-0"), "Center starts on sample Party grid"); capture("center-party");
            press(right); press(a); break;
        case 37:
            check(shell.party()->detailOpen() && shell.party()->detail()["hp"]=="0 / 38" && focus("party-detail-back"), "Known zero HP and detail focus"); capture("center-party-detail");
            press(b); press(SDL_CONTROLLER_BUTTON_Y); break;
        case 38:
            check(shell.party()->section()=="storage" && focus("party-slot-0"), "X opens bounded Storage grid"); capture("center-storage");
            press(right); press(right); press(right); press(right); break;
        case 39:
            check(shell.party()->box()==1, "Storage edge changes sample box without shoulders");
            press(down); press(right); press(a); break;
        case 40:
            check(shell.party()->detail()["kind"]=="unreadable", "Unreadable is distinct from empty"); capture("center-unreadable");
            press(b); press(SDL_CONTROLLER_BUTTON_BACK); break;
        case 41:
            check(shell.party()->section()=="saves" && focus("center-check"), "Select reaches ordinary save service");
            {
                auto* hint=window->findChild<QQuickItem*>("center-action-hint");
                check(hint && !hint->property("text").toString().contains("Restore selected"), "Empty shelf must not advertise restore");
            }
            capture("center-sample-saves");
            shell.center()->begin(); shell.center()->applySearch("No matching Adventure 987654321"); *stage=84; break;
        case 84: {
            auto* button=window->findChild<QQuickItem*>("center-check");
            check(focus("center-check") && button && button->property("label").toString()=="X Search Adventures", "Empty Adventure browser labels its actual search action");
            capture("center-search-empty"); press(SDL_CONTROLLER_BUTTON_Y); break;
        }
        case 85:
            check(shell.keyboard()->isOpen(), "Advertised X opens Center search"); press(b); break;
        case 86:
            check(focus("center-check"), "Cancelled empty search restores its opener");
            shell.center()->beginSelected(shell.home()["adventureId"].toString()); press(b); *stage=42; break;
        case 42:
            check(shell.party()->section()=="storage" && focus("party-slot-5"), "Back restores original box slot");
            press(SDL_CONTROLLER_BUTTON_LEFTSHOULDER); press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER); break;
        case 43:
            check(shell.centerFace() && focus("party-slot-5"), "Global pages preserve Center route");
            SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERLEFT,32767); input.poll();
            SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERLEFT,-32768); input.poll(); break;
        case 44:
            check(!shell.centerFace(), "Trigger returns to Pokedex");
            SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,32767); input.poll();
            SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768); input.poll();
            press(down); press(down); press(a); break;
        case 45:
            check(shell.party()->section()=="activities" && focus("activity-menu-0"), "Center activities has stable menu focus");
            capture("center-activities"); press(a); break;
        case 46:
            check(focus("playroom-actor-0"), "Playroom uses a fixed actor control");
            capture("playroom"); press(right); press(a); break;
        case 47:
            check(focus("playroom-actor-1") && shell.party()->activities()->reaction().contains("called"), "Calling preserves the selected actor control");
            press(SDL_CONTROLLER_BUTTON_Y); break;
        case 48:
            capture("playroom-greeting"); press(b); press(down); press(a); break;
        case 49:
            check(shell.party()->activities()->route()=="practice" && focus("activity-primary"), "Practice setup is controller accessible");
            capture("practice-setup"); press(a); break;
        case 50:
            check(shell.party()->activities()->stage()=="preview", "Practice preview does not run a battle");
            capture("practice-preview"); press(b); press(b); press(down); press(a); break;
        case 51:
            check(shell.party()->activities()->route()=="link", "Link Counter is a separate route");
            capture("link-peer"); press(a); break;
        case 52:
            check(shell.party()->activities()->stage()=="review", "Sample proposal review");
            capture("link-review"); press(a); break;
        case 53:
            check(shell.party()->activities()->stage()=="interrupted", "Interrupted rehearsal never reports transfer success");
            capture("link-interrupted"); press(b); press(b); press(b); press(SDL_CONTROLLER_BUTTON_DPAD_UP); break;
        case 54:
            check(shell.party()->section()=="storage" && focus("party-slot-9"), "Activities returns to the prior management slot");
            press(start); for (int i=0;i<10;++i) press(SDL_CONTROLLER_BUTTON_DPAD_UP); for(int i=0;i<2;++i) press(down); press(a); break;
        case 55:
            check(shell.service()=="settings" && focus("settings-category-0"), "Settings categories focus");
            capture("settings-root"); press(a); break;
        case 56:
            check(focus("settings-control-0"), "Appearance controls receive focus");
            capture("settings-appearance"); press(down); press(a); break;
        case 57:
            check(focus("settings-control-1"), "Motion switch retains focus"); capture("settings-motion");
            press(a); press(b); press(down); press(a); break;
        case 58:
            check(focus("settings-control-0"), "Sound slider receives focus");
            capture("settings-sound"); press(b); press(down); press(a); break;
        case 59:
            capture("settings-media"); press(b); press(down); press(a); break;
        case 60:
            capture("settings-feedback"); press(a); break;
        case 61:
            check(shell.settings()->category()==3 && shell.settings()->controlsFocused(), "Unavailable feedback cannot open a fake action");
            press(b); break;
        case 62:
            check(focus("settings-category-3"), "Back restores category");
            press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER); break;
        case 63:
            check(!shell.serviceOpen(), "Global shoulder leaves settings");
            press(start); press(SDL_CONTROLLER_BUTTON_Y); press(down); press(down); break;
        case 64:
            check(focus("menu-0") && shell.settings()->theme()=="turquoise", "Start has only two quick controls");
            capture("start-two-controls"); break;
        case 65:
            check(shell.settings()->theme()=="turquoise", "Reverse theme adjustment"); press(b);
            press(start); for(int i=0;i<10;++i) press(SDL_CONTROLLER_BUTTON_DPAD_UP);
            press(down); press(down); press(a); for(int i=0;i<4;++i) press(down); press(a); break;
        case 66:
            check(focus("settings-control-0") && shell.settings()->category()==4, "Inline Trainer category");
            capture("settings-trainer"); press(a); break;
        case 67:
            check(shell.trainer()->editing() && shell.service()=="settings", "Profile edits inside Settings");
            capture("settings-profile-edit"); for(int i=0;i<4;++i) press(down); break;
        case 68:
            check(shell.focusIndex()==4 && focus("settings-control-4"), "Vertical Cancel focus");
            capture("settings-profile-cancel"); press(a); break;
        case 69:
            check(!shell.trainer()->editing() && focus("settings-control-0"), "Cancel restores profile row");
            press(b); press(down); press(a); break;
        case 70:
            check(shell.settings()->category()==5 && focus("settings-control-0"), "Inline System category");
            capture("settings-system"); press(down); press(down); press(a); break;
        case 71:
            check(!shell.modeConfirmation() && shell.notice().contains("ArmadaOS"), "Isolated preview cannot perform real power actions"); press(b); break;
        case 72:
            check(focus("settings-control-2"), "Cancelled power restores its row");
            press(b); press(down); press(down); press(a); break;
        case 73:
            check(shell.settings()->category()==7 && focus("settings-control-0"), "Controller category");
            capture("settings-controller"); press(a); break;
        case 74:
            check(shell.service()=="diagnostics", "Controller test from Settings"); press(b); break;
        case 75:
            check(shell.service()=="settings" && focus("settings-control-0"), "Controller test returns to its row");
            press(b); for(int i=0;i<3;++i) press(SDL_CONTROLLER_BUTTON_DPAD_UP); press(a); press(a);
            for(int i=0;i<3;++i) press(down); press(a); break;
        case 76: {
            check(shell.trainer()->editing() && !shell.trainer()->error().isEmpty() && focus("settings-control-0"), "Invalid inline profile restores Name");
            auto* status=window->findChild<QQuickItem*>("settings-status");
            check(status && status->property("text").toString().contains("name"), "Profile validation is visible in Settings");
            capture("settings-profile-validation"); press(a); break;
        }
        case 77:
            check(shell.keyboard()->isOpen(), "Inline profile opens controller keyboard");
            press(a); press(start); break;
        case 78:
            check(shell.menuOpen() && shell.keyboard()->isOpen(), "Start keeps keyboard draft alive");
            press(b); check(shell.keyboard()->isOpen() && !shell.keyboard()->text().isEmpty(), "Back restores keyboard draft");
            capture("settings-profile-keyboard"); press(b); break;
        case 79:
            check(!shell.keyboard()->isOpen() && focus("settings-control-0") && shell.trainer()->draftName().isEmpty(), "Keyboard cancel restores Name without applying draft");
            press(b); press(b); for(int i=0;i<4;++i) press(SDL_CONTROLLER_BUTTON_DPAD_UP); press(a); break;
        case 80: {
            auto* status=window->findChild<QQuickItem*>("settings-status");
            check(status && !status->property("text").toString().contains("name before saving"), "Appearance does not inherit profile error");
            press(b); for(int i=0;i<4;++i) press(down); press(a); press(down); press(a); press(down); press(a); break;
        }
        case 81:
            check(shell.hall()->account()->isOpen() && shell.hall()->account()->status().contains("B cancels"), "Account sign-out asks before changing identity");
            capture("settings-account-confirmation"); press(b); break;
        case 82:
            check(shell.hall()->account()->isOpen() && !shell.hall()->account()->status().contains("B cancels"), "Back cancels sign-out inside the account pane");
            press(b); break;
        case 83:
            check(focus("settings-control-1"), "Account Back restores exact Trainer row");
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
