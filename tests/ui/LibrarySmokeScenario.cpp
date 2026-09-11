#include "LibrarySmokeScenario.h"
#include <QQuickItem>
#include <QCoreApplication>
#include <QTimer>
#include <QImage>
#include <QFile>
#include <memory>

using namespace trainer;
void startLibrarySmoke(QQuickWindow* window, ShellController& shell, SessionState& session, LocalStateStore& store,
        ControllerInput& input, SDL_Joystick* joystick, const QString& phase, const QString& contentFolder,
        const QString& screenshotDir, bool& completed, int& warnings, QStringList& diagnostics) {
    shell.libraryManager()->setInitialFolder(contentFolder);
    auto stage = std::make_shared<int>(0); auto failed = std::make_shared<bool>(false);
    auto identity = std::make_shared<QString>();
    auto timer = new QTimer(window); timer->setInterval(220);
    QObject::connect(timer, &QTimer::timeout, window, [=, &shell, &session, &store, &input, &completed, &warnings, &diagnostics] {
        const auto check = [&](bool condition, const QString& message) {
            if (!condition) { *failed = true; diagnostics.append(QString("%1 / %2: %3").arg(phase).arg(*stage).arg(message)); }
        };
        const auto press = [&](SDL_GameControllerButton button, int count = 1) {
            for (int i = 0; i < count; ++i) {
                SDL_JoystickSetVirtualButton(joystick, button, 1); input.poll(); SDL_JoystickSetVirtualButton(joystick, button, 0); input.poll();
            }
        };
        const auto focusIs = [&](const QString& name) { return window->activeFocusItem() && window->activeFocusItem()->objectName() == name; };
        const auto capture = [&](const QString& name) {
            const auto frame = window->grabWindow(); check(!frame.isNull(), "Empty rendered frame");
            if (!screenshotDir.isEmpty()) check(frame.save(screenshotDir + "/" + name + ".png"), "Screenshot failed");
            auto* focus = window->activeFocusItem(); check(focus && focus->isVisible(), "No visible actual focus");
            if (focus) {
                const auto outline = focus->mapRectToScene(QRectF(-4, -4, focus->width() + 8, focus->height() + 8));
                check(QRectF(0, 0, window->width(), window->height()).contains(outline), "Focused outline outside window");
                for (auto* ancestor = focus->parentItem(); ancestor; ancestor = ancestor->parentItem()) if (ancestor->clip())
                    check(ancestor->mapRectToScene(QRectF(0, 0, ancestor->width(), ancestor->height())).adjusted(-0.1, -0.1, 0.1, 0.1).contains(outline), "Focus clipped by " + ancestor->objectName());
            }
        };
        const auto finish = [&] {
            check(warnings == 0, "QML warnings"); completed = true; timer->stop();
            if (!screenshotDir.isEmpty()) {
                QFile f(screenshotDir + "/" + phase + "-verification.txt");
                if (f.open(QIODevice::WriteOnly)) f.write(((*failed ? QString("FAILED\n") : QString("PASSED\n")) + diagnostics.join('\n')).toUtf8());
            }
            if (*failed) { qCritical().noquote() << diagnostics.join('\n'); QCoreApplication::exit(1); return false; }
            return true;
        };
        if (session.blocked() || shell.libraryManager()->saving() || shell.libraryManager()->files()->busy() || shell.settings()->saving()) return;
        auto* manager = shell.libraryManager();
        constexpr auto a = SDL_CONTROLLER_BUTTON_A, b = SDL_CONTROLLER_BUTTON_B;
        constexpr auto up = SDL_CONTROLLER_BUTTON_DPAD_UP, down = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
        constexpr auto left = SDL_CONTROLLER_BUTTON_DPAD_LEFT, right = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
        constexpr auto start = SDL_CONTROLLER_BUTTON_START, next = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, previous = SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
        if (phase == "collection") {
            switch ((*stage)++) {
            case 0:
                check(store.adventures().isEmpty(), "Reference cards do not create personal records");
                press(next); press(right, 2); press(a); break;
            case 1: {
                capture("collection-missing");
                int ruby = -1; const auto rows = shell.worlds()->adventures();
                for (int i = 0; i < rows.size(); ++i) if (rows[i].toMap()["id"] == "catalogue:ruby-gba") ruby = i;
                check(ruby >= 0, "Ruby catalogue edition is listed"); press(down, std::max(0, ruby)); press(a); break;
            }
            case 2:
                check(focusIs("world-action-setup") && shell.worlds()->detail()["platform"] == "Game Boy Advance", "Missing edition opens a focusable Link a file action");
                capture("collection-detail"); press(a); break;
            case 3:
                check(focusIs("adventure-field-4") && manager->fields()[0].toMap()["value"] == "Pokémon Ruby", "Catalogue details are prefilled with file field focused");
                press(a); break;
            case 4:
                check(manager->files()->rows().size() == 1 && manager->files()->zone() == "list", "Controller file picker contains the test file"); press(a); break;
            case 5:
                check(focusIs("adventure-field-4"), "File selection restores its field"); press(down); press(a); break;
            case 6:
                check(store.adventures().size() == 1 && !shell.serviceOpen() && !shell.menuOpen(), "Save returns directly to Worlds, without a system menu");
                check(store.registration("catalogue:ruby-gba").has_value(), "Stable reference identity linked");
                check(focusIs("world-action-setup"), "Unconfigured attachment remains honestly unavailable for launch"); press(b); break;
            case 7:
                check(shell.worlds()->adventureIndex() == 0 && focusIs("adventure-catalogue:ruby-gba"), "Owned card moves first and retains focus by identity");
                capture("collection-linked"); press(next); press(previous); window->resize(1920, 1080); break;
            case 8:
                check(focusIs("adventure-catalogue:ruby-gba"), "Shoulders preserve collection selection"); capture("collection-linked-1080p");
                press(a); press(a); press(b); break; // Edit then discard: return to the exact card action.
            default:
                check(!shell.serviceOpen() && !shell.menuOpen() && focusIs("world-action-setup"), "Cancel returns to the Worlds action");
                if (finish()) window->close(); break;
            }
        } else if (phase == "library-seed") {
            switch ((*stage)++) {
            case 0:
                check(store.adventures().isEmpty() && !shell.sampleLibrary(), "Personal library must start empty");
                capture("empty-home"); press(next); press(start); press(down, 3); press(a); break;
            case 1:
                check(shell.service() == "library" && focusIs("manage-action-0"), "Library entry focus"); capture("empty-library");
                press(a); press(down, 2); press(a); break;
            case 2:
                check(manager->route() == "files" && manager->files()->rows().size() == 2, "File picker opened on explicit test folder");
                capture("file-picker"); press(a); break; // Empty subfolder.
            case 3:
                check(manager->files()->rows().isEmpty() && focusIs("file-action-0"), "Empty folder recovery"); capture("empty-folder"); press(b); break;
            case 4:
                check(manager->files()->rows().size() == 2, "B restores parent list");
                press(down, 2);
                check(manager->files()->zone() == "actions", "Down from last file reaches the lower action rail");
                press(up);
                check(manager->files()->rowIndex() == 1 && manager->files()->zone() == "list", "Up returns to the remembered file");
                press(a); break;
            case 5:
                check(manager->route() == "edit" && focusIs("adventure-field-4"), "File choice restores field");
                press(up, 2); press(a); press(a); press(down, 3); press(right); press(a); // Append A to suggested title.
                press(right); press(a); press(down, 2); press(a); // Primary Hoenn.
                press(down); press(left); press(a); press(a); press(down, 8); press(a); // Additional Kanto, Apply.
                press(right); press(a); press(a, 2); press(b); break; // ROM hack, back from edition chooser.
            case 6:
                check(manager->fields()[0].toMap()["value"] == "journeyA" && manager->fields()[1].toMap()["value"] == "Hoenn"
                      && manager->fields()[2].toMap()["value"] == "Kanto", "Controller title and World relationships");
                capture("adventure-form"); press(down, 2); press(left); press(a); press(next); break;
            case 7:
                check(store.adventures().size() == 1 && !shell.serviceOpen(), "Save completes after global page switch");
                press(previous); press(a); press(a); break; // Kanto secondary listing / detail.
            case 8:
                check(shell.worlds()->detail()["title"] == "journeyA" && focusIs("world-action-setup"), "Personal Adventure offers setup without claiming it can launch");
                capture("personal-adventure"); press(start); press(up, 3); press(a); press(a); break;
            case 9:
                check(shell.settings()->theme() == "red" && focusIs("settings-0"), "Persisted red theme"); capture("theme-red"); press(a); break;
            case 10: check(shell.settings()->theme() == "green", "Green theme"); capture("theme-green"); press(a); break;
            case 11: check(shell.settings()->theme() == "blue", "Blue theme"); capture("theme-blue"); press(a); break;
            case 12: check(shell.settings()->theme() == "orange", "Orange theme"); capture("theme-orange"); press(down); press(a); break;
            case 13:
                check(shell.settings()->reducedMotion(), "Reduced motion saved"); capture("motion-settings");
                press(b); press(down, 6); if (finish()) press(a); break;
            }
        } else if (phase == "library-verify") {
            switch ((*stage)++) {
            case 0:
                check(store.adventures().size() == 1 && shell.page() == 1 && shell.worlds()->route() == "detail", "Library / route survived restart");
                if (!store.adventures().isEmpty()) *identity = store.adventures().first().id;
                check(shell.settings()->theme() == "orange" && shell.settings()->reducedMotion(), "Settings survived restart");
                window->resize(1920, 1080); break;
            case 1:
                capture("personal-adventure-1080p"); press(start); press(down, 3); press(a); press(a); break;
            case 2:
                check(manager->fields()[0].toMap()["value"] == "journeyA", "Edit reads committed metadata");
                press(a); press(a); press(b); press(b); // Discard keyboard then form.
                press(a); press(down, 3); press(a); break; // Save stale file reference: harness moved it.
            case 3:
                check(!manager->error().isEmpty() && manager->route() == "edit", "Missing file keeps retryable draft");
                check(store.registration(*identity)->adventure.title == "journeyA", "Cancel did not alter title");
                capture("missing-file"); press(up); press(a); break;
            case 4:
                press(down); press(a); press(down); press(a); break; // Choose moved file and save.
            case 5:
                check(store.registration(*identity)->contentPath.endsWith("journey-moved.bin"), "Relocated content keeps identity");
                press(a); press(right); press(a); press(down, 9); press(a); // Create World.
                press(a); press(down, 3); press(right); press(a); // A, Apply.
                break;
            case 6:
                check(manager->fields()[1].toMap()["value"] == "A", "Custom World name entered through controller");
                capture("custom-world-form"); press(down, 3); press(left); press(a); break;
            case 7:
                check(store.worlds().size() == 10 && store.adventures().size() == 1 && store.adventures().first().id == *identity, "Custom World persisted without duplicating Adventure");
                press(next); press(previous); press(b, 2); press(down, 3); break;
            case 8:
                check(focusIs("world-9"), "Custom World is reachable in a bounded region grid"); capture("custom-world-grid");
                press(a); press(a); window->resize(1024, 768); break;
            case 9:
                check(shell.worlds()->region()["name"] == "A" && focusIs("world-action-setup"), "Custom World detail focus"); capture("custom-world-letterbox");
                if (finish()) window->close(); break;
            }
        } else {
            switch ((*stage)++) {
            case 0:
                check(store.worlds().size() == 10 && shell.worlds()->region()["name"] == "A", "Custom World survived second restart");
                check(shell.worlds()->detail()["title"] == "journeyA", "Adventure survived second restart");
                press(previous); press(SDL_CONTROLLER_BUTTON_Y); break;
            case 1:
                check(shell.page() == 0 && shell.drawerOpen() && focusIs("resume-empty"), "Empty Continue focus");
                if (auto* drawer = window->findChild<QQuickItem*>("continue-drawer")) check(drawer->height() == 229, "Reduced motion completes drawer geometry");
                capture("personal-continue"); press(b); if (finish()) window->close(); break;
            }
        }
    });
    timer->start();
}
