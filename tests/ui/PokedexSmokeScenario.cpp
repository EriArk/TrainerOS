#include "PokedexSmokeScenario.h"
#include <QGuiApplication>
#include <QQuickItem>
#include <QFile>
#include <QImage>
#include <QTimer>
#include <memory>

using namespace trainer;
void startPokedexSmoke(QQuickWindow* window, ShellController& shell, ControllerInput& input,
                      MockPokedexRepository& repository, SDL_Joystick* joystick, const QString& screenshotDir,
                      bool& completed, int& qmlWarnings, QStringList& diagnostics) {
    auto stage = std::make_shared<int>(0);
    auto failed = std::make_shared<bool>(false);
    auto timer = new QTimer(window);
    timer->setInterval(350);
    QObject::connect(timer, &QTimer::timeout, window,
        [window, &shell, &input, &repository, joystick, screenshotDir, &completed, &qmlWarnings, &diagnostics, stage, failed, timer] {
        const auto check = [&](bool condition, const QString& message) {
            if (!condition) { *failed = true; diagnostics.append(QString("Stage %1: %2").arg(*stage).arg(message)); }
        };
        const auto press = [&](SDL_GameControllerButton button, int count = 1) {
            for (int i = 0; i < count; ++i) {
                SDL_JoystickSetVirtualButton(joystick, button, 1); input.poll();
                SDL_JoystickSetVirtualButton(joystick, button, 0); input.poll();
            }
        };
        const auto focusIs = [&](const QString& id) {
            return window->activeFocusItem() && window->activeFocusItem()->objectName() == id;
        };
        const auto capture = [&](const QString& name) {
            const auto frame = window->grabWindow();
            check(!frame.isNull(), "Empty rendered frame");
            if (!screenshotDir.isEmpty()) check(frame.save(screenshotDir + "/" + name + ".png"), "Cannot save screenshot");
        };
        auto* focused = window->activeFocusItem();
        check(focused && focused->isVisible() && focused->isEnabled(), "Focus must be visible and enabled");
        if (focused) for (auto* ancestor = focused->parentItem(); ancestor; ancestor = ancestor->parentItem()) {
            if (ancestor->clip()) check(QRectF(-1, -1, ancestor->width() + 2, ancestor->height() + 2).contains(
                ancestor->mapRectFromItem(focused, QRectF(-4, -4, focused->width() + 8, focused->height() + 8))),
                "Focus outline clipped: " + focused->objectName());
        }
        constexpr auto a = SDL_CONTROLLER_BUTTON_A, b = SDL_CONTROLLER_BUTTON_B;
        constexpr auto left = SDL_CONTROLLER_BUTTON_DPAD_LEFT, right = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
        constexpr auto up = SDL_CONTROLLER_BUTTON_DPAD_UP, down = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
        constexpr auto l1 = SDL_CONTROLLER_BUTTON_LEFTSHOULDER, r1 = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
        constexpr auto start = SDL_CONTROLLER_BUTTON_START;
        auto* dex = shell.pokedex();
        switch ((*stage)++) {
        case 0: check(input.connected(), "Virtual controller unavailable"); press(r1, 2); break;
        case 1:
            check(focusIs("dex-recovery"), "Failed initial reference load has retry focus");
            capture("load-error"); press(a); break;
        case 2:
            check(focusIs("dex-entry-bulbasaur"), "Retry opens reference list");
            capture("list"); press(down, 12); break;
        case 3: {
            check(focusIs("dex-entry-mudkip"), "Long list reaches Mudkip");
            auto* list = window->findChild<QQuickItem*>("dex-list");
            check(list && list->property("contentY").toDouble() > 0, "Controller scrolls only the entry list");
            capture("scrolled-list"); press(a); break;
        }
        case 4: check(focusIs("dex-favorite"), "Detail focus"); capture("detail"); press(start); break;
        case 5: check(focusIs("menu-0"), "Menu overlays entry"); press(b); press(r1); break;
        case 6: check(shell.page() == 3, "R1 remains global"); press(l1); break;
        case 7: check(focusIs("dex-favorite") && dex->detail()["id"] == "mudkip", "Detail survives section return"); press(b); break;
        case 8:
            check(focusIs("dex-entry-mudkip"), "Back restores scrolled entry");
            press(up, 13); press(right); press(a); break;
        case 9:
            check(focusIs("dex-choice-0"), "World chooser traps focus");
            capture("world-picker"); press(right, 3); press(a); break;
        case 10:
            check(focusIs("dex-rail-1") && dex->entries().size() == 6, "World filter applied");
            press(right); press(a); break;
        case 11:
            press(down, 3); press(right, 3);
            check(focusIs("dex-choice-18"), "Water is reachable on final type row");
            capture("type-picker"); press(a); break;
        case 12:
            check(dex->entries().size() == 1 && dex->detail()["id"] == "mudkip", "World and type combine");
            press(right); press(a); break;
        case 13: press(right, 2); press(a); break;
        case 14:
            check(dex->entries().size() == 1 && focusIs("dex-rail-3"), "Caught filter combines with World/type");
            capture("combined-filters"); press(left, 3); press(a); break;
        case 15:
            check(focusIs("key-A"), "Search reuses controller keyboard");
            capture("search-keyboard");
            press(right, 11); press(a); press(down); press(a); // 25, numeric block.
            press(down, 2); press(left); press(a); break;
        case 16:
            check(dex->query() == "25" && dex->entries().isEmpty() && focusIs("dex-rail-0"), "Applied search preserves combined filters");
            press(down); break;
        case 17: check(focusIs("dex-recovery"), "No matches has recovery action"); capture("no-matches"); press(a); press(up); press(a); break;
        case 18:
            check(shell.keyboard()->isOpen() && shell.keyboard()->text().isEmpty(), "Reset clears query and filters");
            press(down); press(right, 2); press(a); // M
            press(down); press(left, 2); press(a); // U
            press(up, 2); press(right, 3); press(a); // D
            press(down, 3); press(right); press(a); break;
        case 19:
            check(dex->query() == "MUD" && dex->entries().size() == 1, "Letter search is case-insensitive");
            capture("name-search"); press(a); press(a); press(start); break;
        case 20:
            press(b); check(focusIs("key-A") && shell.keyboard()->text() == "MUDA", "Menu preserves search draft/key");
            press(b); break;
        case 21:
            check(focusIs("dex-rail-0") && dex->query() == "MUD", "Cancel preserves applied query");
            press(a); press(a); press(r1); break;
        case 22:
            check(!shell.keyboard()->isOpen() && dex->query() == "MUD", "Global page change discards only unsubmitted search");
            press(l1); break;
        case 23: check(focusIs("dex-rail-0"), "Section return restores search opener"); press(down); press(a); break;
        case 24: repository.failNextWrite(); press(a); break;
        case 25:
            check(focusIs("notice-close") && dex->detail()["favorite"].toBool(), "Failed favorite write keeps saved mark");
            capture("favorite-error"); press(b); break;
        case 26: check(focusIs("dex-favorite"), "Favorite retry focus"); press(a); break;
        case 27:
            check(!dex->detail()["favorite"].toBool(), "Favorite retry succeeds");
            press(b); press(up); press(right, 5); press(a); break;
        case 28: press(up, 13); press(left, 2); press(a); break;
        case 29: press(right, 3); press(a); break;
        case 30:
            check(dex->entries().size() == 2, "Favorites filter reflects persisted-in-memory marks");
            capture("favorites"); press(down); press(a); break;
        case 31: press(a); break; // Remove Pikachu; fallback to Eevee list row.
        case 32: check(focusIs("dex-entry-eevee"), "Removing a filtered favorite restores remaining row"); press(a); break;
        case 33: press(a); break;
        case 34:
            check(focusIs("dex-recovery") && dex->entries().isEmpty(), "Removing final favorite keeps recovery focus");
            capture("empty-favorites"); press(a); break;
        case 35: press(up); press(right); press(a); break; // Remembered rail index 3 -> Order.
        case 36: press(right); press(a); break;
        case 37:
            check(dex->rail()[4].toMap()["value"] == "Name A–Z", "Sort applied");
            capture("sorted"); press(down, 5); press(a); break;
        case 38: check(dex->detail()["id"] == "eevee", "Sorted browse opens correct identity"); window->resize(1920, 1080); break;
        case 39: capture("detail-1080p"); window->resize(1024, 768); break;
        case 40: capture("detail-letterbox"); window->resize(960, 540); repository.failNextLoad(); dex->refresh(); break;
        case 41:
            check(focusIs("notice-close") && dex->entries().size() == 14, "Failed refresh retains usable reference snapshot");
            capture("refresh-error"); press(b); break;
        case 42:
            check(focusIs("dex-favorite") && dex->detail()["id"] == "eevee", "Refresh notice restores detail");
            press(SDL_CONTROLLER_BUTTON_Y);break;
        case 43:
            check(focusIs("journal-field-0") && dex->journal()->isOpen(), "Y opens journal with controller focus");
            capture("journal");press(right);press(a,2);press(down);press(a);break;
        case 44:
            check(focusIs("key-A"), "Journal note uses shared controller keyboard");press(a);press(down,3);press(right);press(a);break;
        case 45:
            check(focusIs("journal-field-2"), "Keyboard returns to note field");capture("journal-filled");press(SDL_CONTROLLER_BUTTON_Y);break;
        case 46:
            check(!dex->journal()->isOpen() && dex->detail()["notes"]=="A", "Journal Save commits note");
            check(dex->detail()["caught"]=="Yes" && dex->detail()["seen"]=="Yes", "Caught includes Seen");
            capture("journal-recorded");press(SDL_CONTROLLER_BUTTON_Y);press(down);press(a);press(a);press(b);press(b);break;
        case 47:
            check(!dex->journal()->isOpen() && dex->detail()["notes"]=="A", "Cancelled journal leaves committed note");
            press(SDL_CONTROLLER_BUTTON_Y);press(r1);break;
        case 48:
            check(shell.page()==3 && !dex->journal()->isOpen(), "Global page change discards journal draft");press(l1);break;
        default:
            check(focusIs("dex-favorite"), "Journal page return restores detail focus");
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
