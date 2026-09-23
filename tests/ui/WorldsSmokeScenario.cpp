#include "WorldsSmokeScenario.h"
#include <QGuiApplication>
#include <QQuickItem>
#include <QFile>
#include <QImage>
#include <QTimer>
#include <memory>

using namespace trainer;
void startWorldsSmoke(QQuickWindow* window, ShellController& shell, ControllerInput& input,
                     MockAdventureAdapter& adapter, SDL_Joystick* joystick, const QString& screenshotDir,
                     bool& completed, int& qmlWarnings, QStringList& diagnostics) {
    auto stage = std::make_shared<int>(0);
    auto failed = std::make_shared<bool>(false);
    auto drawerWaits = std::make_shared<int>(0);
    auto timer = new QTimer(window);
    timer->setInterval(450); // The two-phase Choose drawer takes 400 ms to settle.
    QObject::connect(timer, &QTimer::timeout, window,
                     [window, &shell, &input, &adapter, joystick, screenshotDir, &completed, &qmlWarnings, &diagnostics, stage, failed, drawerWaits, timer] {
        const auto check = [&](bool condition, const QString& message) {
            if (!condition) { *failed = true; diagnostics.append(QString("Stage %1: %2").arg(*stage - 1).arg(message)); }
        };
        const auto press = [&](SDL_GameControllerButton button, int count = 1) {
            for (int i = 0; i < count; ++i) {
                SDL_JoystickSetVirtualButton(joystick, button, 1); input.poll();
                SDL_JoystickSetVirtualButton(joystick, button, 0); input.poll();
            }
        };
        const auto focusIs = [&](const QString& name) {
            return window->activeFocusItem() && window->activeFocusItem()->objectName() == name;
        };
        const auto flipFace = [&] {
            SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 32767); input.poll();
            SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, -32768); input.poll();
        };
        const auto capture = [&](const QString& name) {
            const auto frame = window->grabWindow();
            check(!frame.isNull(), "Empty rendered frame");
            if (!screenshotDir.isEmpty()) check(frame.save(screenshotDir + "/" + name + ".png"), "Cannot write screenshot");
        };
        const auto visibleFocus = [&] {
            auto* item = window->activeFocusItem();
            if (!item) { check(false, "No active focus item"); return; }
            check(item->isVisible() && item->isEnabled(), "Focus must be visible and enabled: " + item->objectName());
            const QRectF focusBounds(-4, -4, item->width() + 8, item->height() + 8);
            for (auto* ancestor = item->parentItem(); ancestor; ancestor = ancestor->parentItem()) {
                if (ancestor->property("itemPrefix").isValid()
                    && item->objectName().startsWith(ancestor->property("itemPrefix").toString())) {
                    const auto center = ancestor->mapFromItem(item, QPointF(item->width()/2, item->height()/2));
                    check(qAbs(center.y() - (36 + (ancestor->height()-78)/2)) < 3,
                        "Selected wheel edition must align with its highlighted slot: " + item->objectName());
                }
                if (!ancestor->clip()) continue;
                check(QRectF(-1, -1, ancestor->width() + 2, ancestor->height() + 2)
                    .contains(ancestor->mapRectFromItem(item, focusBounds)), "Focused control clipped: " + item->objectName());
            }
        };
        constexpr auto a = SDL_CONTROLLER_BUTTON_B, b = SDL_CONTROLLER_BUTTON_A;
        constexpr auto left = SDL_CONTROLLER_BUTTON_DPAD_LEFT, right = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
        constexpr auto up = SDL_CONTROLLER_BUTTON_DPAD_UP, down = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
        constexpr auto l1 = SDL_CONTROLLER_BUTTON_LEFTSHOULDER, r1 = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
        constexpr auto start = SDL_CONTROLLER_BUTTON_START;
        if (*stage == 44) {
            auto* drawer = window->findChild<QQuickItem*>("continue-drawer");
            // Software rendering on ARM can stretch wall-clock animation time.
            // Inspect the actual settled geometry, with a bounded failure path.
            if (drawer && drawer->height() < 228.99 && ++*drawerWaits < 20) return;
            check(drawer && drawer->height() >= 228.99, "Choose drawer did not finish opening");
        }
        visibleFocus(); // Check settled focus, including outlines inside scrolling lists.
        switch ((*stage)++) {
        case 0:
            check(input.connected(), "Virtual controller unavailable");
            press(r1); press(right, 2); press(a); break;
        case 1: {
            check(shell.worlds()->route() == "adventures" && focusIs("adventure-emerald-demo"), "Opening Hoenn focuses Emerald");
            auto* list = window->findChild<QQuickItem*>("adventure-list");
            const auto rows = shell.worlds()->adventures();
            check(list && rows.size() >= 4, "Four-row fixture is available");
            check(list && list->property("selectionIndex").toInt()==0, "Wheel opens at the remembered edition");
            check(shell.worlds()->detail()["id"]==rows[0].toMap()["id"], "Wheel details follow the focused edition");
            capture("hoenn-list"); press(down, 4); break;
        }
        case 2: {
            check(focusIs("adventure-emerald-trails-demo"), "Final ROM hack row is focused");
            auto* list = window->findChild<QQuickItem*>("adventure-list");
            check(list && list->property("selectionIndex").toInt() == 4, "Controller navigation must advance the wheel to the final edition");
            capture("hoenn-list-scrolled"); press(a); break;
        }
        case 3:
            check(shell.worlds()->route() == "detail" && focusIs("world-action-launch"), "ROM hack detail focuses Start");
            capture("rom-hack-detail"); press(start); break;
        case 4:
            check(focusIs("menu-0"), "System menu focus above detail");
            press(b); check(focusIs("world-action-launch"), "Menu restores detail action");
            press(r1); break;
        case 5:
            check(shell.page() == 2 && focusIs("dex-entry-bulbasaur"), "R1 remains global");
            press(l1); break;
        case 6:
            check(shell.worlds()->detail()["id"] == "emerald-trails-demo" && focusIs("world-action-launch"), "Page return preserves detail identity and focus");
            press(b); break;
        case 7:
            check(focusIs("adventure-emerald-trails-demo"), "Back restores scrolled list row");
            press(up, 3); press(a); break;
        case 8:
            check(shell.worlds()->actions().size() == 2 && focusIs("world-action-launch"), "Launch-only Adventure has no direct-resume action");
            capture("launch-only-detail"); adapter.failNextLaunch(); press(a); break;
        case 9:
            check(shell.notice().contains("couldn't open") && focusIs("notice-close"), "Launch failure is recoverable");
            capture("launch-error"); press(b); break;
        case 10:
            check(focusIs("world-action-launch"), "Retry restores launch focus");
            press(a); break;
        case 11:
            check(shell.notice().contains("Demo launch ready") && shell.notice().contains("No Adventure was launched"), "Mock launch result");
            capture("launch-result"); press(b); press(b); break;
        case 12:
            check(focusIs("adventure-ruby-demo"), "Back restores Ruby");
            press(down); press(a); break;
        case 13:
            check(focusIs("world-action-back"), "Unavailable Start is skipped");
            press(left); check(focusIs("world-action-back"), "Disabled action cannot take focus");
            capture("needs-setup"); press(a); break;
        case 14:
            check(focusIs("adventure-sapphire-demo"), "Explicit Back restores Sapphire");
            press(up, 2); press(a); break;
        case 15:
            check(focusIs("world-action-resume"), "Emerald detail defaults to available resume");
            capture("emerald-detail"); press(a); break;
        case 16:
            check(shell.notice().contains("Route 119") && focusIs("notice-close"), "Correct resume point sent to adapter");
            capture("worlds-resume-result"); press(b); press(b); break;
        case 17:
            check(focusIs("adventure-emerald-demo"), "Resume leaves list selection intact");
            press(b); press(left, 2); press(down); press(a); break;
        case 18:
            check(shell.worlds()->adventures().isEmpty() && focusIs("world-list-back"), "Empty World has reachable Back");
            capture("empty-world"); press(start); break;
        case 19:
            press(b); check(focusIs("world-list-back"), "Menu restores empty-state action");
            press(a); break;
        case 20:
            check(focusIs("world-3"), "Empty World returns to its region");
            press(up); press(a); break;
        case 21:
            check(focusIs("adventure-firered-demo"), "Other region has its own Adventure list");
            press(a); break;
        case 22:
            check(shell.worlds()->detail()["badges"] == "0", "Known zero badge count is retained");
            capture("known-zero-progress");
            press(b, 2); press(right, 2); press(a); press(down, 4); press(a); break;
        case 23:
            check(shell.worlds()->detail()["id"] == "emerald-trails-demo", "Return to Hoenn and traverse again");
            window->resize(1920, 1080); break;
        case 24:
            capture("adventure-1080p"); window->resize(1024, 768); break;
        case 25:
            capture("adventure-letterbox"); window->resize(960, 540); press(b); break;
        case 26:
            check(focusIs("adventure-emerald-trails-demo"), "Restored last row after resizing");
            press(down); check(focusIs("adventure-emerald-trails-demo"), "Down clamps to final game; B owns return");
            press(a); press(r1); break;
        case 27:
            press(l1); press(start); press(r1); break;
        case 28:
            check(shell.page() == 2 && !shell.menuOpen(), "R1 from system menu changes primary page");
            press(l1); break;
        case 29:
            check(focusIs("world-action-launch") && shell.worlds()->detail()["id"] == "emerald-trails-demo", "Detail survives global menu/page transitions");
            press(b); press(SDL_CONTROLLER_BUTTON_Y); break;
        case 30:
            check(shell.keyboard()->isOpen(), "X opens the shared controller keyboard");
            press(right, 7); press(down); press(a); // R
            press(left, 7); press(down); press(a); // U
            press(up, 2); press(right); press(a); // B
            press(down, 2); press(right, 3); press(a); // Y
            break;
        case 31:
            check(shell.keyboard()->text() == "RUBY", "Search entered entirely with SDL gamepad events");
            capture("worlds-search-keyboard"); press(down); press(right); press(a); break;
        case 32:
            check(!shell.keyboard()->isOpen() && shell.worlds()->adventures().size() == 1 && focusIs("adventure-ruby-demo"), "Applying search focuses its result");
            capture("worlds-search-results");
            press(SDL_CONTROLLER_BUTTON_X, 2); break;
        case 33:
            check(shell.worlds()->filterLabel() == "Missing" && shell.worlds()->adventures().isEmpty() && focusIs("world-list-back"), "Empty search/filter combination has visible recovery");
            capture("worlds-filter-empty"); press(start); press(SDL_CONTROLLER_BUTTON_Y);
            check(!shell.keyboard()->isOpen(), "Search does not bypass the system menu");
            press(b); press(SDL_CONTROLLER_BUTTON_X); break;
        case 34:
            check(focusIs("adventure-ruby-demo"), "Filter restores a matching row");
            press(SDL_CONTROLLER_BUTTON_Y); press(a); press(b);
            check(shell.worlds()->query() == "RUBY" && focusIs("adventure-ruby-demo"), "Cancelled draft restores the original search and focus");
            press(SDL_CONTROLLER_BUTTON_Y); press(a); press(r1); break;
        case 35:
            check(shell.page() == 2 && !shell.keyboard()->isOpen(), "R1 cancels search draft and switches section");
            press(l1); check(shell.worlds()->query() == "RUBY", "Committed search survives page changes");
            press(SDL_CONTROLLER_BUTTON_Y); press(down, 2); press(right, 7); press(a); // Clear
            press(down); press(a); break; // Apply
        case 36:
            check(shell.worlds()->query().isEmpty() && shell.worlds()->adventures().size() == 5, "Controller Clear restores the complete list");
            press(right); break;
        case 37:
            check(focusIs("adventure-emerald-trails-demo"), "Fast jump clamps at the final row and reveals it");
            capture("worlds-jump"); press(a); break;
        case 38:
            check(focusIs("world-action-launch") && shell.worlds()->detail()["id"] == "emerald-trails-demo", "Detail survives global menu/page transitions");
            flipFace(); break;
        case 39:
            check(shell.multiverseFace() && focusIs("multiverse-system-0"), "Trigger opens Multiverse systems");
            capture("multiverse-systems"); press(a); break;
        case 40:
            check(focusIs("multiverse-game-0"), "First sample game focus"); capture("multiverse-list");
            press(down); press(a); break;
        case 41:
            check(focusIs("multiverse-game-1") && shell.page()==1, "Missing entry stays in the wheel"); capture("multiverse-missing");
            press(up); break;
        case 42:
            check(focusIs("multiverse-game-0"), "Linked sample and details share the wheel"); capture("multiverse-detail");
            press(a); break;
        case 43:
            check(shell.page() == 0 && shell.multiverseHome() && focusIs("multiverse-launch"), "Selection opens independent Multiverse Home");
            capture("multiverse-home"); press(SDL_CONTROLLER_BUTTON_X); break;
        case 44:
            check(shell.drawerOpen() && shell.resumePoints().size() == 3, "Multiverse-only drawer"); capture("multiverse-choose");
            press(right); press(a); press(a); break;
        case 45:
            check(shell.notice().contains("No game was launched"), "Sample Home does not launch"); press(b);
            press(SDL_CONTROLLER_BUTTON_Y); break;
        case 46:
            check(!shell.multiverseHome() && focusIs("home-launch"), "X returns to Pokemon Home");
            capture("pokemon-home-context"); press(r1); break;
        case 47:
            check(shell.multiverseFace() && focusIs("multiverse-game-0"), "Page return keeps wheel position");
            flipFace(); break;
        case 48:
            check(focusIs("world-action-launch") && shell.worlds()->detail()["id"] == "emerald-trails-demo", "Pokemon detail survives paired browser");
            flipFace(); press(SDL_CONTROLLER_BUTTON_Y); break;
        case 49:
            check(shell.keyboard()->isOpen(), "Multiverse uses controller keyboard"); capture("multiverse-search");
            flipFace(); check(shell.multiverseFace(), "Keyboard traps face switching"); press(b); press(b); press(down); press(a); break;
        case 50:
            check(focusIs("multiverse-empty"), "Empty system has visible recovery"); capture("multiverse-empty");
            press(a); break;
        case 51:
            check(focusIs("multiverse-system-3"), "Empty recovery restores system position");
            window->resize(1920,1080); break;
        case 52:
            capture("multiverse-1080p"); window->resize(960,540); flipFace(); break;
        case 53: {
            check(focusIs("world-action-launch"), "Pair return restores Pokemon detail action");
            auto* header=window->findChild<QQuickItem*>("world-detail-header");
            check(header, "Adventure heading available");
            // Stress the production heading with a long edition name without
            // changing library identities or the normal sample catalogue.
            if (header) header->setProperty("title", "Pokemon: A Very Long Adventure Through Distant Regions - Expanded Anniversary Edition");
            break;
        }
        case 54: {
            auto* header=window->findChild<QQuickItem*>("world-detail-header");
            auto* heading=header ? header->findChild<QQuickItem*>("page-heading") : nullptr;
            check(heading && heading->property("lineCount").toInt()==2 && !heading->property("truncated").toBool(), "Long edition heading fits two lines");
            capture("worlds-long-title"); press(b);
            shell.worlds()->applySearch("No matching edition 987654321"); break;
        }
        case 55:
            check(shell.worlds()->adventures().isEmpty() && focusIs("world-list-back"), "Empty long search retains recovery focus");
            capture("worlds-long-search-empty"); flipFace(); break;
        case 56:
            check(shell.multiverseFace() && focusIs("multiverse-system-3"), "Empty Pokemon search leaves paired route intact");
            press(up); press(a); shell.multiverse()->applySearch("No matching title"); break;
        case 57:
            check(focusIs("multiverse-empty"), "Multiverse empty filter has focused reset");
            capture("multiverse-filter-empty"); flipFace(); break;
        case 58:
            check(!shell.multiverseFace() && shell.worlds()->query()=="No matching edition 987654321" && focusIs("world-list-back"), "Pair return retains empty search and Back focus");
            flipFace(); press(start); press(b); break;
        case 59:
            check(focusIs("multiverse-empty") && shell.multiverse()->query()=="No matching title", "Start restores empty Multiverse filter");
            press(a); break;
        case 60:
            check(shell.multiverse()->query().isEmpty() && focusIs("multiverse-game-0"), "Empty reset restores first real sample row");
            capture("multiverse-filter-reset"); break;
        default:
            check(qmlWarnings == 0, "QML warnings emitted");
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
