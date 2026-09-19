#include "ExitSmokeScenario.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QQuickItem>
#include <QTemporaryDir>
#include <algorithm>
#include <cmath>
#include <memory>

using namespace trainer;
namespace {
struct Scenario {
    QTemporaryDir files;
    int stage = 0, returned = 0, completed = 0, closes = 0;
    quint64 capture = 0;
    QByteArray pid;
    QJsonObject context;
    bool failed = false;
};
}
void startExitSmoke(QQuickWindow* shellWindow, ShellController& shell, AdventureLaunchController& launch,
        AdventureExitPresentation& view, ControllerInput& input, SDL_Joystick* joystick,
        const QString& output, bool& completed, int& warnings, QStringList& diagnostics) {
    auto state = std::make_shared<Scenario>();
    auto* window = shellWindow->findChild<QQuickWindow*>("adventure-exit-window");
    if (!window) { diagnostics.append("Missing exit window"); QCoreApplication::exit(1); return; }
    auto& exit = launch.exitController();
    auto* timer = new QTimer(shellWindow); timer->setInterval(200);
    // This is an isolated test provider. Production does not treat a muted SDL
    // virtual device as evidence that the physical controller has been released.
    QObject::connect(&input, &ControllerInput::sampled, timer, [&input, &view] {
        const auto& sample = input.sample();
        bool neutral = std::none_of(sample.buttons.begin(), sample.buttons.end(), [](bool b) { return b; });
        for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; ++i)
            neutral &= i < SDL_CONTROLLER_AXIS_TRIGGERLEFT ? std::abs(sample.axes[i]) < 11468 : sample.axes[i] < 8000;
        view.updateInput(view.inputGeneration(), {sample.connected, neutral,
            sample.buttons[SDL_CONTROLLER_BUTTON_B], sample.buttons[SDL_CONTROLLER_BUTTON_A]});
    });
    QObject::connect(&launch, &AdventureLaunchController::checkpointRequested, timer,
        [&launch](quint64 token, const QJsonObject&) { launch.checkpointCompleted(token, {}); });
    QObject::connect(&launch, &AdventureLaunchController::suspendRequested, timer, [shellWindow] { shellWindow->hide(); });
    QObject::connect(&launch, &AdventureLaunchController::restoreRequested, timer,
        [state, shellWindow, &shell](const QJsonObject& context) {
            ++state->returned; state->context = context; shell.restoreNavigation(context);
            shellWindow->show(); shellWindow->requestActivate();
        });
    QObject::connect(&exit, &AdventureExitController::captureRequested, timer, [state](quint64 token) { state->capture = token; });
    QObject::connect(&exit, &AdventureExitController::gracefulExitRequested, timer, [state] { ++state->closes; });
    QObject::connect(&exit, &AdventureExitController::completed, timer, [state] { ++state->completed; });
    const auto initial = shell.navigationState();
    QObject::connect(timer, &QTimer::timeout, timer,
        [=, &shell, &launch, &exit, &view, &input, &completed, &warnings, &diagnostics] {
        const auto check = [&](bool pass, const QString& message) {
            if (!pass) { state->failed = true; diagnostics.append(QString("Exit stage %1: %2").arg(state->stage).arg(message)); }
        };
        const auto button = [&](SDL_GameControllerButton b, bool held) { SDL_JoystickSetVirtualButton(joystick, b, held); input.poll(); };
        const auto press = [&](SDL_GameControllerButton b) { button(b, true); button(b, false); };
        const auto readPid = [&] {
            QFile f(state->files.filePath("pid")); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
        };
        const auto capture = [&](const QString& name) {
            const auto image = window->grabWindow(); check(!image.isNull(), "Exit window rendered");
            if (!output.isEmpty()) check(image.save(output + "/" + name + ".png"), "Screenshot saved");
            for (const auto* name : {"exit-back", "exit-confirm"}) {
                auto* item = window->findChild<QQuickItem*>(name);
                check(item && QRectF(0,0,window->width(),window->height()).contains(
                    item->mapRectToScene(QRectF(0,0,item->width(),item->height()))), "Mounted action fits viewport");
            }
        };
        switch (state->stage) {
        case 0: {
            input.setEnabled(false);
            const auto probe = QDir(QCoreApplication::applicationDirPath()).filePath(
#ifdef Q_OS_WIN
                "trainer_process_probe.exe"
#else
                "trainer_process_probe"
#endif
            );
            check(launch.launch({probe, {"controlled", state->files.filePath("command"), state->files.filePath("pid")}, {}}, initial), "Child launch accepted");
            break;
        }
        case 1:
            if (launch.state() == "starting") return;
            check(launch.state() == "running", "Original process is running");
            state->pid = readPid(); check(!state->pid.isEmpty(), "Original PID recorded");
            exit.setAvailable(true); check(exit.requestExit(), "Exit request accepted");
            check(!window->isVisible(), "No overlay before capture completes");
            button(SDL_CONTROLLER_BUTTON_B, true);
            exit.captureCompleted(state->capture, {}, "Fixture capture failure");
            view.setInputIsolated(true);
            break;
        case 2:
            check(window->isVisible() && window->isActive(), "Separate exit window has focus while shell is hidden");
            check(!shellWindow->isVisible(), "Hidden shell stays hidden");
            check(!view.ready() && state->closes == 0, "Held opening A cannot confirm");
            capture("release-controls"); button(SDL_CONTROLLER_BUTTON_B, false);
            break;
        case 3:
            check(view.ready(), "Full neutral snapshot arms actions"); capture("capture-failed");
            press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER); press(SDL_CONTROLLER_BUTTON_START);
            check(shell.navigationState() == initial && !shell.menuOpen(), "Modal suppresses page and Start navigation");
            press(SDL_CONTROLLER_BUTTON_A);
            check(!view.visible() && !window->isVisible(), "B removes the exit window");
            check(launch.state() == "running" && readPid() == state->pid && state->returned == 0,
                  "B retains same child and does not restore shell navigation");
            check(state->completed == 0 && state->closes == 0, "Cancellation publishes no completion or close");
            shellWindow->resize(1920,1080);
            check(exit.requestExit(), "Retry accepted");
            { QImage frame(64,36,QImage::Format_RGB32); frame.fill(Qt::green); exit.captureCompleted(state->capture, frame); }
            view.setInputIsolated(true);
            break;
        case 4:
            check(view.ready() && !view.captureFailed(), "Retry has fresh input and capture state");
            capture("confirmation-1080p"); press(SDL_CONTROLLER_BUTTON_B);
            check(state->closes == 1 && !view.confirming() && view.visible(), "A requests close once and presents waiting");
            check(launch.state() == "running" && state->completed == 0, "A is not proof of process exit");
            press(SDL_CONTROLLER_BUTTON_A); press(SDL_CONTROLLER_BUTTON_B);
            check(state->closes == 1 && view.visible(), "No cancel or duplicate close after dispatch");
            break;
        case 5: {
            capture("closing");
            QFile command(state->files.filePath("command"));
            check(command.open(QIODevice::WriteOnly) && command.write("exit") == 4, "Ask fixture to end normally");
            break;
        }
        case 6:
            if (launch.active()) return;
            check(!view.visible() && !window->isVisible(), "Actual process exit removes waiting window");
            check(state->returned == 1 && state->completed == 1 && state->context == initial,
                  "Actual exit completes once and restores original route");
            input.setEnabled(true); input.poll();
            break;
        case 7:
            press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER); check(shell.page() == 1, "R1 works after return");
            press(SDL_CONTROLLER_BUTTON_LEFTSHOULDER); check(shell.page() == 0, "L1 works after return");
            input.setEnabled(false);
            check(QFile::remove(state->files.filePath("command")) && QFile::remove(state->files.filePath("pid")), "Remove finished fixture controls");
            check(launch.launch({QDir(QCoreApplication::applicationDirPath()).filePath(
#ifdef Q_OS_WIN
                "trainer_process_probe.exe"
#else
                "trainer_process_probe"
#endif
            ), {"controlled", state->files.filePath("command"), state->files.filePath("pid")}, {}}, initial, {}, AdventureSavePolicy::VerifiedAutosave), "Autosave fixture launch");
            break;
        case 8:
            if (launch.state() == "starting") return;
            check(launch.state() == "running", "Autosave process running"); state->pid = readPid();
            exit.setAvailable(true); check(exit.requestExit(), "Autosave exit request");
            { QImage frame(64,36,QImage::Format_RGB32); frame.fill(Qt::blue); exit.captureCompleted(state->capture, frame); }
            view.setInputIsolated(true); break;
        case 9:
            check(view.ready() && view.autosave() && view.confirming() && state->closes == 1, "Autosave does not bypass confirmation");
            check(window->findChild<QObject*>("exit-heading")->property("text").toString() == "Close this game?", "Autosave wording asks exit, not manual save");
            capture("autosave-confirmation"); press(SDL_CONTROLLER_BUTTON_A);
            check(launch.state() == "running" && readPid() == state->pid && state->returned == 1, "Autosave B returns to same process");
            check(exit.requestExit(), "Autosave retry"); exit.captureCompleted(state->capture, {}, "Fixture failure");
            view.setInputIsolated(true); break;
        case 10: {
            check(view.ready(), "Fresh input after autosave cancellation"); press(SDL_CONTROLLER_BUTTON_B);
            check(state->closes == 2, "Autosave requires explicit A");
            QFile command(state->files.filePath("command"));
            check(command.open(QIODevice::WriteOnly) && command.write("exit") == 4, "End autosave fixture normally"); break;
        }
        case 11:
            if (launch.active()) return;
            check(state->returned == 2 && state->completed == 2 && !view.visible(), "Autosave exit completes once after real process end");
            check(warnings == 0, "No QML warnings");
            completed = true; timer->stop();
            if (!output.isEmpty()) {
                QFile report(output + "/verification.txt");
                if (report.open(QIODevice::WriteOnly)) report.write(((state->failed ? QString("FAILED\n") : QString("PASSED\n")) + diagnostics.join('\n')).toUtf8());
            }
            if (state->failed) qCritical().noquote() << diagnostics.join('\n');
            QCoreApplication::exit(state->failed ? 1 : 0);
            break;
        }
        ++state->stage;
    });
    timer->start();
}
