#include "adapters/pokemon/PokemonExperience.h"
#include "PersistenceSmokeScenario.h"
#include "RenderSettlement.h"
#include <QCoreApplication>
#include <QQuickItem>
#include <QImage>
#include <QFile>
#include <QTimer>
#include <memory>

using namespace trainer;
void startPersistenceSmoke(QQuickWindow* window, ShellController& shell, SessionState& session,
        ControllerInput& input, SDL_Joystick* joystick, const QString& phase, const QString& screenshotDir,
        bool& completed, int& warnings, QStringList& diagnostics) {
    auto stage = std::make_shared<int>(0);
    auto failed = std::make_shared<bool>(false);
    auto layoutWait = std::make_shared<int>(0);
    auto timer = new QTimer(window); timer->setInterval(200);
    QObject::connect(timer, &QTimer::timeout, window, [=, &shell, &session, &input, &completed, &warnings, &diagnostics] {
        const auto check = [&](bool condition, const QString& message) {
            if (!condition) { *failed = true; diagnostics.append(QString("%1 stage %2: %3").arg(phase).arg(*stage).arg(message)); }
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
        const auto capture = [&](const QString& name) {
            const auto frame = window->grabWindow(); check(!frame.isNull(), "Empty frame");
            if (!screenshotDir.isEmpty()) check(frame.save(screenshotDir + "/" + name + ".png"), "Screenshot failed");
            auto* focus = window->activeFocusItem();
            check(focus && focus->isVisible(), "No visible actual focus");
            if (focus) {
                const auto rect = focus->mapRectToScene(QRectF(0, 0, focus->width(), focus->height()));
                check(QRectF(0, 0, window->width(), window->height()).contains(rect), "Focus is outside screen");
            }
        };
        const auto finish = [&] {
            check(warnings == 0, "QML warnings"); timer->stop(); completed = true;
            if (!screenshotDir.isEmpty()) {
                QFile report(screenshotDir + "/" + phase + "-verification.txt");
                if (report.open(QIODevice::WriteOnly)) report.write(((*failed ? QString("FAILED\n") : QString("PASSED\n")) + diagnostics.join('\n')).toUtf8());
            }
            if (*failed) { qCritical().noquote() << diagnostics.join('\n'); QCoreApplication::exit(1); return false; }
            return true;
        };
        constexpr auto a = SDL_CONTROLLER_BUTTON_B, b = SDL_CONTROLLER_BUTTON_A;
        constexpr auto down = SDL_CONTROLLER_BUTTON_DPAD_DOWN, up = SDL_CONTROLLER_BUTTON_DPAD_UP;
        constexpr auto right = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
        constexpr auto next = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, previous = SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
        if (phase == "error") {
            if (session.choices().isEmpty()) return;
            check(session.blocked() && focusIs("storage-action-0"), "Storage recovery must trap focus");
            press(next); check(shell.page() == 0, "Failed startup cannot enter unloaded pages");
            capture("storage-error");
            if (finish()) press(b); // Explicit exit from startup recovery.
            return;
        }
        if (session.blocked() || pokemonModule(shell).persona()->saving() || pokemonModule(shell).pokedex()->saving()) return;
        if(waitForViewport(window,*layoutWait))return;
        if (phase == "seed") {
            switch ((*stage)++) {
            case 0:
                check(!pokemonModule(shell).persona()->exists() && focusIs("home-launch"), "Fresh startup");
                press(next, 3); press(a); press(a); press(a); // Trainer name A.
                press(down, 3); press(right); press(a); // Apply.
                check(pokemonModule(shell).persona()->draftName() == "A", "Controller text entry");
                press(down, 2); press(a); // Favorite picker.
                check(pokemonModule(shell).persona()->picker()->entries().size() == 14, "Reference-backed favorite choices");
                press(down, 6); *stage = 3; break;
            case 3:
                check(focusIs("species-eevee"), "Favorite list retains actual controller focus");
                capture("trainer-favorite-picker"); press(a);
                check(pokemonModule(shell).persona()->draftFavorite() == "Eevee", "Choice updates only the profile draft");
                press(down); press(a); press(previous, 2); // Save, leave while pending to Worlds.
                *stage = 1;
                break;
            case 1:
                check(pokemonModule(shell).persona()->profile()["name"].toString() == "A", "Committed Trainer after leaving page");
                press(a); // Pokemon collection, with one shared installed-game browser.
                for(int i=0;i<shell.multiverse()->games().size() && shell.multiverse()->detail()["id"]!="emerald-demo";++i)press(down);
                check(shell.multiverse()->detail()["id"].toString() == "emerald-demo", "Collection selection");
                press(next); press(a); press(a); press(b); // Bulbasaur favorite, back during write.
                break;
            case 2:
                check(pokemonModule(shell).pokedex()->detail()["favorite"].toBool(), "Committed local favorite");
                press(up); press(a); press(a); press(down, 3); press(right); press(a); // Search A, Apply.
                check(pokemonModule(shell).pokedex()->query() == "A" && focusIs("dex-rail-0"), "Applied search and restored focus");
                capture("saved-search");
                press(SDL_CONTROLLER_BUTTON_START); press(down, 6); press(a); press(down); press(a);
                check(shell.modeConfirmation(), "Power routes development exit through confirmation");
                // Completion precedes asynchronous exit; the process harness reopens only after exit.
                if (finish()) press(a);
                break;
            }
        } else {
            switch ((*stage)++) {
            case 0:
                check(pokemonModule(shell).persona()->profile()["name"].toString() == "A", "Profile survived process restart");
                check(pokemonModule(shell).persona()->profile()["favorite"].toString() == "Eevee", "Chosen favorite survived process restart");
                check(shell.page() == 2 && pokemonModule(shell).pokedex()->query() == "A" && focusIs("dex-rail-0"), "Page/search/focus survived restart");
                check(!shell.menuOpen() && !shell.keyboard()->isOpen(), "No restored transient layers");
                press(down); press(a);
                check(pokemonModule(shell).pokedex()->detail()["favorite"].toBool(), "Favorite survived process restart");
                check(pokemonModule(shell).pokedex()->detail()["seen"].toString() == "Not recorded", "No sample progress contamination");
                capture("restored-favorite"); window->resize(1920, 1080); break;
            case 1:
                capture("restored-favorite-1080p");
                press(previous);*stage=5;break;
            case 5:
                check(shell.collectionsRoot() && focusIs("series-card-pokemon"), "Primary re-entry opens the remembered collection grid after restart");
                press(a);
                check(shell.multiverse()->detail()["id"].toString() == "emerald-demo", "Reopening the collection retains its game focus");
                window->resize(1024, 768);*stage=2;break;
            case 2:
                capture("restored-world-letterbox");
                press(next, 2); window->resize(960, 540); break;
            case 3:
                check(focusIs("trainer-open"), "Trainer keeps its fixed A action");
                capture("trainer-overview"); press(a); press(down, 2); press(a);
                press(SDL_CONTROLLER_BUTTON_Y); // Shared keyboard above picker.
                press(a); press(down, 3); press(right); press(a); // Search A.
                break;
            case 4:
                check(pokemonModule(shell).persona()->picker()->query() == "A", "Controller search returns to picker");
                capture("trainer-favorite-search"); press(b);
                check(pokemonModule(shell).persona()->editing() && !pokemonModule(shell).persona()->picker()->isOpen(), "Back returns to profile draft");
                press(a); press(SDL_CONTROLLER_BUTTON_X); // Clear only draft favorite.
                check(pokemonModule(shell).persona()->draftFavorite() == "Not chosen", "Y clears draft choice");
                press(previous); press(next); // Global page action discards the draft.
                check(pokemonModule(shell).persona()->profile()["favorite"].toString() == "Eevee" && !pokemonModule(shell).persona()->editing(), "Page change discards unsubmitted favorite");
                if (finish()) window->close(); // Native window closing also drains browsing state.
                break;
            }
        }
    });
    timer->start();
}
