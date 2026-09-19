#include "HallSmokeScenario.h"
#include <QGuiApplication>
#include <QQuickItem>
#include <QFile>
#include <QImage>
#include <QTimer>
#include <memory>

using namespace trainer;
void startHallSmoke(QQuickWindow* window, ShellController& shell, ControllerInput& input,
                    MockHallOfFameRepository& archive, MockAchievementProvider& provider, SDL_Joystick* joystick,
                    const QString& screenshotDir, bool& completed, int& warnings, QStringList& diagnostics) {
    auto stage = std::make_shared<int>(0);
    auto failed = std::make_shared<bool>(false);
    auto timer = new QTimer(window);
    timer->setInterval(350);
    QObject::connect(timer, &QTimer::timeout, window,
        [window, &shell, &input, &archive, &provider, joystick, screenshotDir, &completed, &warnings, &diagnostics, stage, failed, timer] {
        const auto check = [&](bool condition, const QString& reason) {
            if (!condition) { *failed = true; diagnostics.append(QString("Stage %1: %2").arg(*stage).arg(reason)); }
        };
        const auto press = [&](SDL_GameControllerButton button, int count = 1) {
            for (int i = 0; i < count; ++i) {
                SDL_JoystickSetVirtualButton(joystick, button, 1); input.poll();
                SDL_JoystickSetVirtualButton(joystick, button, 0); input.poll();
            }
        };
        const auto flip = [&](SDL_GameControllerAxis axis = SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
            SDL_JoystickSetVirtualAxis(joystick, axis, 32767); input.poll();
            SDL_JoystickSetVirtualAxis(joystick, axis, -32768); input.poll();
        };
        const auto focusIs = [&](const QString& id) { return window->activeFocusItem() && window->activeFocusItem()->objectName() == id; };
        const auto capture = [&](const QString& name) {
            const auto frame = window->grabWindow();
            check(!frame.isNull(), "Empty rendered frame");
            if (!screenshotDir.isEmpty()) check(frame.save(screenshotDir + "/" + name + ".png"), "Cannot save screenshot");
        };
        const auto drawer = window->findChild<QQuickItem*>("continue-drawer");
        if (drawer && ((shell.drawerOpen() && drawer->height() < 228)
            || (!shell.drawerOpen() && drawer->height() > 53))) return;
        auto* focused = window->activeFocusItem();
        check(focused && focused->isVisible() && focused->isEnabled(), "Focus must be visible and enabled");
        if (focused) for (auto* ancestor = focused->parentItem(); ancestor; ancestor = ancestor->parentItem())
            if (ancestor->clip()) check(QRectF(-1, -1, ancestor->width() + 2, ancestor->height() + 2).contains(
                ancestor->mapRectFromItem(focused, QRectF(-4, -4, focused->width() + 8, focused->height() + 8))), "Clipped focus: " + focused->objectName());
        constexpr auto a = SDL_CONTROLLER_BUTTON_B, b = SDL_CONTROLLER_BUTTON_A;
        constexpr auto up = SDL_CONTROLLER_BUTTON_DPAD_UP, down = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
        constexpr auto left = SDL_CONTROLLER_BUTTON_DPAD_LEFT, right = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
        constexpr auto l1 = SDL_CONTROLLER_BUTTON_LEFTSHOULDER, r1 = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
        constexpr auto start = SDL_CONTROLLER_BUTTON_START;
        constexpr auto x = SDL_CONTROLLER_BUTTON_X, y = SDL_CONTROLLER_BUTTON_Y;
        auto* hall = shell.hall();
        switch ((*stage)++) {
        case 0: check(input.connected(), "Controller unavailable"); press(r1, 4); break;
        case 1: check(focusIs("hall-row-crystal-champion"), "Initial archive focus"); capture("archive"); press(down, 3); break;
        case 2: {
            check(focusIs("hall-row-crystal-undated"), "Last archive record focus");
            auto* list = window->findChild<QQuickItem*>("hall-list");
            check(list && list->property("contentY").toDouble() > 0, "Controller scrolls archive viewport");
            capture("archive-scrolled"); press(a); break;
        }
        case 3: check(focusIs("hall-action-0"), "Unknown record has Back"); capture("unknown-record"); press(b); press(up, 3); press(a); break;
        case 4: capture("team-detail"); press(start); break;
        case 5: check(focusIs("menu-0"), "System menu above archive detail"); press(b); press(l1); break;
        case 6: check(shell.page() == 3, "L1 remains global"); press(r1); break;
        case 7:
            check(hall->route() == "archive-detail" && focusIs("hall-action-0"), "Archive detail preserved across sections");
            flip(); break;
        case 8: check(focusIs("hall-row-emerald-sample"), "RA opens Adventure sets"); capture("achievement-sets"); press(a); break;
        case 9: check(focusIs("hall-row-first-trail"), "Achievement list focus"); capture("achievements"); press(down, 4); break;
        case 10: check(focusIs("hall-row-lasting-memory"), "Achievements scroll to last goal"); capture("achievements-scrolled"); press(a); break;
        case 11: capture("unknown-unlock-date"); press(b); press(up, 3); press(a); break;
        case 12:
            check(hall->detail()["summary"] == "Unlocked · Hardcore", "Unlock mode is preserved");
            capture("achievement-detail"); provider.holdRequests(true); provider.setNextResult(AchievementState::Offline);
            press(right); press(a); break;
        case 13:
            check(hall->status().contains("Refreshing") && focusIs("hall-action-0"), "Loading skips disabled Refresh");
            capture("loading-cached"); press(start); break;
        case 14: press(b); check(focusIs("hall-action-0"), "Menu restores loading action"); press(l1); break;
        case 15: provider.finishRefresh("emerald-sample"); press(r1); break;
        case 16:
            check(hall->status().contains("Offline") && hall->rowIndex() == 1, "Offline result preserves detail identity");
            capture("offline-cached"); provider.setNextResult(AchievementState::Error); press(right); press(a); break;
        case 17: provider.finishRefresh("emerald-sample"); break;
        case 18: check(hall->status().contains("Refresh failed"), "Error retains cache"); capture("error-cached"); press(right); press(a); break;
        case 19: provider.finishRefresh("emerald-sample"); break;
        case 20:
            check(hall->status().contains("sample records"), "Retry succeeds");
            press(b, 2); press(down, 2); press(a); break;
        case 21:
            check(hall->rows().isEmpty() && focusIs("hall-action-0"), "Unsupported set keeps Back");
            capture("unsupported"); press(right); check(focusIs("hall-action-0"), "Unsupported Refresh skipped"); press(a); break;
        case 22:
            check(focusIs("hall-row-hack-sample"), "Back restores unsupported Adventure");
            press(up, 2); provider.setAccount({}); press(a); break;
        case 23:
            check(hall->rows().isEmpty() && hall->detail()["summary"] == "Unlocks unavailable", "Disconnected is not zero earned");
            capture("disconnected"); press(a); break;
        case 24: provider.setAccount("other-sample"); provider.refresh("emerald-sample"); press(a); break;
        case 25:
            check(hall->rows().isEmpty() && hall->status().contains("Loading"), "Loading without cache");
            capture("loading-empty"); provider.finishRefresh("emerald-sample"); press(up); break;
        case 26:
            check(focusIs("hall-row-first-trail"), "Loaded rows accessible with Up");
            for (const auto& row : hall->rows()) check(row.toMap()["subtitle"] == "Not recorded", "Previous account unlock leaked");
            capture("other-account"); press(a); break;
        case 27:
            capture("unknown-account-record"); press(b, 2); provider.setAccount("sample-trainer");
            provider.setNextResult(AchievementState::Error); provider.refresh("emerald-sample"); press(a); break;
        case 28: provider.finishRefresh("emerald-sample"); break;
        case 29:
            check(hall->rows().isEmpty() && focusIs("hall-action-0"), "Error without cache keeps recovery focus");
            capture("error-empty"); press(right); press(a); break;
        case 30: provider.finishRefresh("emerald-sample"); break;
        case 31: press(up); press(a); break;
        case 32:
            check(hall->detail()["summary"] == "Unlocked · Standard", "Correct account records restored after fresh load");
            flip(SDL_CONTROLLER_AXIS_TRIGGERLEFT); break;
        case 33: check(hall->route() == "archive-detail" && focusIs("hall-action-0"), "Paired archive detail survives provider changes"); window->resize(1920, 1080); break;
        case 34: capture("team-1080p"); window->resize(1024, 768); break;
        case 35: capture("team-letterbox"); window->resize(960, 540); press(b); break;
        case 36: archive.setEmpty(true); hall->refreshArchive(); break;
        case 37:
            check(focusIs("hall-action-0"), "Empty local archive has recovery action");
            capture("archive-empty"); archive.setEmpty(false); press(a); break;
        case 38:
            check(focusIs("hall-row-crystal-champion"), "Archive retry restores list focus");
            archive.failNextLoad(); press(down, 4); press(a); break;
        case 39: check(focusIs("notice-close"), "Failed archive refresh reports error"); capture("archive-error"); press(b); break;
        case 40:
            check(hall->rows().size() == 4 && focusIs("hall-row-crystal-undated"), "Failed archive refresh keeps data and selection");
            press(SDL_CONTROLLER_BUTTON_BACK); break;
        case 41:
            check(focusIs("memory-field-0") && hall->editor()->isOpen(), "Select opens a new memory with deterministic focus");
            flip(); check(hall->isArchive() && hall->editor()->isOpen(), "Pair cannot bypass archive editor");
            capture("new-memory");press(a);break;
        case 42:
            check(hall->editor()->route()=="adventures", "Adventure picker opens");capture("memory-adventures");press(a);press(down,2);press(right);press(a);break;
        case 43:
            check(hall->editor()->route()=="team" && focusIs("memory-field-0"), "Team is a controller grid");capture("memory-team");
            press(a);press(a);press(down,3);press(right);press(a);break;
        case 44:
            check(hall->editor()->fields().first().toMap()["value"]=="A", "Team name entered using controller keyboard");
            press(x);capture("memory-level-keyboard");press(right,10);press(a);press(left);press(down,3);press(a);break;
        case 45:
            check(hall->editor()->fields().first().toMap()["subtitle"]=="Lv 1", "Numeric block entered team level");
            capture("memory-team-entered");press(b);press(left);press(a);press(a);press(down,3);press(right);press(a);press(y);break;
        case 46:
            check(!hall->editor()->isOpen() && hall->rows().size()==5, "Y commits memory and closes editor");
            check(hall->detail()["description"]=="A" && hall->team().first().toMap()["name"]=="A", "Saved note and team displayed");
            capture("manual-memory");press(x);break;
        case 47:
            check(hall->editor()->isOpen(), "X edits selected memory");capture("edit-memory");press(a);press(x);break;
        case 48:
            check(shell.keyboard()->isOpen(), "Picker search uses shared keyboard");press(a);press(down,3);press(right);press(a);break;
        case 49:
            check(hall->editor()->query()=="A", "Search applied through controller");capture("memory-search");press(b);press(l1);break;
        case 50:
            check(shell.page()==3 && !hall->editor()->isOpen(), "L1 closes transient editor and remains global");press(r1);break;
        case 51:
            check(hall->rows().size()==5 && focusIs("hall-action-0"), "Archive detail restores focus after leaving editor");
            press(x);press(down,2);press(a);press(a);press(b);press(b);break;
        case 52:
            check(!hall->editor()->isOpen() && hall->detail()["description"]=="A", "Cancelling keyboard and draft preserves committed note");
            provider.setAccount({}); flip(); press(x); break;
        case 53:
            check(hall->account()->isOpen() && focusIs("achievement-account-0"), "X opens controller account form");
            flip(); check(!hall->isArchive() && hall->account()->isOpen(), "Pair cannot bypass account form");
            capture("account-form"); press(down); press(a); break;
        case 54:
            check(shell.keyboard()->isOpen(), "Password opens shared controller keyboard");
            press(a); check(shell.keyboard()->displayText() == QString(QChar(0x2022)), "Password draft is masked");
            capture("account-password"); press(down,3); press(right); press(a); break;
        case 55:
            check(!shell.keyboard()->isOpen() && hall->account()->rows()[1].toMap()["detail"] == QString(QChar(0x2022)), "Applied password stays masked");
            press(start); break;
        case 56:
            check(focusIs("menu-0"), "Start overlays account form"); press(b); window->resize(1920,1080); break;
        case 57:
            check(focusIs("achievement-account-1"), "Account focus returns after Start"); capture("account-1080p"); press(l1); break;
        case 58:
            check(shell.page()==3 && !hall->account()->isOpen(), "Global page navigation clears account draft"); press(r1); press(x); break;
        case 59:
            check(hall->account()->rows()[1].toMap()["detail"] == "A · Enter password", "Password not retained after leaving page");
            press(b); window->resize(960,540); break;
        case 60:
            check(!hall->isArchive(), "RA remains active after closing account form");
            press(y); break;
        case 61: {
            check(drawer, "Shared selector is present in the rendered chassis");
            check(shell.drawerOpen(), "Y opens shared Adventure selector from RA");
            flip(); check(!hall->isArchive(), "Pair cannot bypass shared selector");
            capture("achievement-adventure-selector"); press(b); break;
        }
        case 62:
            check(!shell.drawerOpen() && !hall->isArchive(), "B returns to the same RA face");
            flip(); break;
        case 63:
            check(hall->isArchive() && hall->route() == "archive-detail" && focusIs("hall-action-0"), "Archive detail restored after account and drawer");
            capture("paired-archive-return"); flip(SDL_CONTROLLER_AXIS_TRIGGERLEFT); break;
        case 64:
            check(!hall->isArchive() && focused->objectName().startsWith("hall-"), "RA restores visible deterministic focus");
            capture("paired-achievement-return"); break;
        default:
            check(!hall->account()->isOpen(), "Back returns to achievement browser");
            check(warnings == 0, "QML warnings emitted");
            completed = true; timer->stop();
            if (!screenshotDir.isEmpty()) {
                QFile report(screenshotDir + "/verification.txt");
                if (report.open(QIODevice::WriteOnly | QIODevice::Truncate))
                    report.write(((*failed ? QString("FAILED\n") : QString("PASSED\n")) + diagnostics.join('\n')).toUtf8());
            }
            QGuiApplication::exit(*failed ? 1 : 0);
        }
    });
    timer->start();
}
