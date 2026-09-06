#include "features/diagnostics/DiagnosticsController.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QKeyEvent>

using namespace trainer;
class VirtualPad {
public:
    int index = -1;
    SDL_Joystick* joystick = nullptr;
    VirtualPad() {
        SDL_SetMainReady(); SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
        index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
        if (index >= 0) joystick = SDL_JoystickOpen(index);
    }
    void detach() {
        if (joystick) { SDL_JoystickClose(joystick); joystick = nullptr; }
        if (index >= 0) { SDL_JoystickDetachVirtual(index); index = -1; }
    }
    ~VirtualPad() { detach(); SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER); }
};
class DiagnosticsTests final : public QObject {
    Q_OBJECT
private slots:
    void controllerSignalsRemainDistinctFromKeyboardAndForeground() {
        VirtualPad pad; QVERIFY(pad.joystick);
        ControllerInput input(nullptr, SDL_JoystickInstanceID(pad.joystick));
        DiagnosticsController checks; checks.configure(&input, nullptr); input.poll(); checks.begin(); checks.reset();
        QKeyEvent key(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier); input.eventFilter(nullptr, &key);
        QVERIFY(checks.observations()["recentControllerActions"].toArray().isEmpty());
        QVERIFY(!checks.observations()["buttons"].toArray()[SDL_CONTROLLER_BUTTON_A].toObject()["observed"].toBool());
        QSignalSpy actions(&input, &ControllerInput::action);
        input.setEnabled(false);
        SDL_JoystickSetVirtualButton(pad.joystick, SDL_CONTROLLER_BUTTON_A, 1); input.poll();
        QVERIFY(input.sample().buttons[SDL_CONTROLLER_BUTTON_A]); QVERIFY(actions.isEmpty());
        QCOMPARE(checks.gate(), QString("Shell input paused while inactive"));
        input.setEnabled(true); input.poll(); QVERIFY(actions.isEmpty()); QVERIFY(input.sample().awaitingNeutral);
        SDL_JoystickSetVirtualButton(pad.joystick, SDL_CONTROLLER_BUTTON_A, 0); input.poll();
        SDL_JoystickSetVirtualButton(pad.joystick, SDL_CONTROLLER_BUTTON_A, 1); input.poll();
        QCOMPARE(actions.size(), 1); QCOMPARE(checks.lastAction(), QString("Confirm (A)"));
        SDL_JoystickSetVirtualButton(pad.joystick, SDL_CONTROLLER_BUTTON_A, 0); input.poll();
        SDL_JoystickSetVirtualAxis(pad.joystick, SDL_CONTROLLER_AXIS_RIGHTX, -30000); input.poll();
        SDL_JoystickSetVirtualAxis(pad.joystick, SDL_CONTROLLER_AXIS_RIGHTX, 26000); input.poll();
        const auto axis = checks.observations()["axes"].toArray()[SDL_CONTROLLER_AXIS_RIGHTX].toObject();
        QCOMPARE(axis["minimum"].toInt(), -30000); QCOMPARE(axis["maximum"].toInt(), 26000);
        const auto guid = checks.observations()["guid"].toString(); QVERIFY(!guid.isEmpty());
        pad.detach(); input.poll(); QVERIFY(!input.connected());
        QCOMPARE(checks.observations()["guid"].toString(), guid);
        QCOMPARE(checks.observations()["disconnectionsObserved"].toInt(), 1);
        QVERIFY(checks.observations()["axes"].toArray()[0].toObject()["rawValue"].isNull());
        QVERIFY(checks.observations()["buttons"].toArray()[SDL_CONTROLLER_BUTTON_A].toObject()["observed"].toBool());
    }
    void unmappedDeviceHasHonestRecoveryState() {
        SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
        const auto index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 2, 2, 0);
        QVERIFY(index >= 0);
        {
            ControllerInput input(nullptr, SDL_JoystickGetDeviceInstanceID(index));
            DiagnosticsController checks; checks.configure(&input, nullptr); input.poll(); checks.begin();
            QVERIFY(!input.connected()); QVERIFY(!input.sample().unmappedDevices.isEmpty());
            QVERIFY(checks.connection().contains("without a controller mapping"));
            QSignalSpy back(&checks, &DiagnosticsController::closeRequested);
            checks.dispatch(Action::Back); QCOMPARE(back.size(), 1);
        }
        SDL_JoystickDetachVirtual(index); SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    }
    void reportsAreAtomicBoundedSnapshotsAndRetryable() {
        QTemporaryDir dir; QWindow window; window.resize(960, 540);
        DiagnosticsService reports(dir.path() + "/reports"); reports.setWindow(&window);
        DiagnosticsController checks; checks.configure(nullptr, &reports); checks.begin();
        QSignalSpy completed(&reports, &DiagnosticsService::completed);
        checks.activate(1); QVERIFY(reports.saving()); checks.activate(1);
        QTRY_COMPARE(completed.size(), 1); QVERIFY(completed.first()[0].toString().isEmpty());
        QFile file(reports.lastReportPath()); QVERIFY(file.open(QIODevice::ReadOnly));
        const auto bytes = file.readAll(); file.close();
        const auto report = QJsonDocument::fromJson(bytes).object();
        QCOMPARE(report["schemaVersion"].toInt(), 1);
        QCOMPARE(report["runtime"].toObject()["window"].toObject()["width"].toInt(), 960);
        window.resize(1024, 768);
        QTRY_VERIFY(checks.runtimeLines().join(' ').contains("1024 × 768"));
        QVERIFY(!report["runtime"].toObject().contains("hostname"));
        QVERIFY(!report.contains("library")); QVERIFY(!report.contains("environment"));
        QVERIFY(report["input"].toObject()["axes"].toArray()[0].toObject()["minimum"].isNull());
        checks.activate(1); QTRY_COMPARE(completed.size(), 2);
        QVERIFY(reports.lastReportPath() != file.fileName());
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), bytes); file.close();
        const auto blocked = dir.path() + "/blocked";
        QFile blocker(blocked); QVERIFY(blocker.open(QIODevice::WriteOnly)); blocker.write("original"); blocker.close();
        DiagnosticsService retry(blocked + "/reports"); checks.configure(nullptr, &retry);
        QSignalSpy retried(&retry, &DiagnosticsService::completed);
        checks.activate(1); QTRY_COMPARE(retried.size(), 1); QVERIFY(!retried.first()[0].toString().isEmpty());
        QVERIFY(blocker.open(QIODevice::ReadOnly)); QCOMPARE(blocker.readAll(), QByteArray("original")); blocker.close();
        QVERIFY(blocker.remove()); // Only the test-owned regular file.
        checks.activate(1); QTRY_COMPARE(retried.size(), 2); QVERIFY(retried.last()[0].toString().isEmpty());
    }
    void requestedReportFinishesDuringNormalTeardown() {
        QTemporaryDir dir;
        { DiagnosticsService service(dir.path()); service.save({{"connected", false}}); }
        const auto files = QDir(dir.path()).entryList({"device-check-*.json"}, QDir::Files);
        QCOMPARE(files.size(), 1);
        QFile report(QDir(dir.path()).filePath(files.first())); QVERIFY(report.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(report.readAll()).object()["schemaVersion"].toInt(), 1);
    }
};
QTEST_MAIN(DiagnosticsTests)
#include "DiagnosticsTests.moc"
