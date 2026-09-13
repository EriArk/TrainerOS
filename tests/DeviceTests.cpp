#include "features/device/DeviceController.h"
#include "core/navigation/ShellController.h"
#include "core/storage/SessionState.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QSemaphore>

using namespace trainer;
namespace {
void write(const QString& path, const QByteArray& value) {
    QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(value), value.size());
}
}
class DeviceTests : public QObject {
    Q_OBJECT
private slots:
    void volumeReadingsAreHonest() {
        QCOMPARE(parseVolume("Volume: 0.11\n").volume, 11);
        const auto muted = parseVolume("Volume: 0.35 [MUTED]\n");
        QCOMPARE(muted.volume, 35); QVERIFY(muted.muted);
        QCOMPARE(parseVolume("Volume: 1.25").volume, 125);
        for (const auto& value : {"", "Volume: nan", "Volume: -1", "Volume: 0,11", "Volume: 11", "error\nVolume: 0.50"})
            QCOMPARE(parseVolume(value).volume, -1);
    }
    void backlightWritesAreBoundedAndUnambiguous() {
        QTemporaryDir root; QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir("screen"));
        const auto screen = root.filePath("screen");
        write(screen + "/max_brightness", "255\n"); write(screen + "/brightness", "2\n");
        QCOMPARE(readBacklight(root.path()).current, 2);
        QVERIFY(!writeBacklight(root.path(), 0).isEmpty());
        QVERIFY(!writeBacklight(root.path(), 101).isEmpty());
        QCOMPARE(readBacklight(root.path()).current, 2);
        QVERIFY(writeBacklight(root.path(), 50).isEmpty());
        QCOMPARE(readBacklight(root.path()).current, 128);
        QVERIFY(writeBacklight(root.path(), 5).isEmpty());
        QCOMPARE(readBacklight(root.path()).current, 13);
        QVERIFY(QDir(root.path()).mkdir("second"));
        write(root.filePath("second/max_brightness"), "100\n"); write(root.filePath("second/brightness"), "20\n");
        QVERIFY(readBacklight(root.path()).directory.isEmpty());
        QVERIFY(!writeBacklight(root.path(), 60).isEmpty());
        QFile original(screen + "/brightness"); QVERIFY(original.open(QIODevice::ReadOnly)); QCOMPARE(original.readAll(), "13\n");
        write(root.filePath("second/max_brightness"), "broken");
        QCOMPARE(readBacklight(root.path()).directory, screen);
        write(screen + "/brightness", "256\n");
        QVERIFY(readBacklight(root.path()).directory.isEmpty());
    }
    void workerFailuresAndControllerRecovery() {
        DeviceSnapshot value; value.volume = 35; value.brightness = 50;
        bool fail = false; QThread* thread = nullptr;
        DeviceService service({[&] { thread = QThread::currentThread(); return value; },
            [&](const QString& control, int setting) -> QString {
                if (fail) return "Audio device unavailable";
                if (control == "volume") value.volume = setting;
                if (control == "brightness") value.brightness = setting;
                if (control == "mute") value.muted = setting != 0;
                return {};
            }});
        DeviceController device; device.configure(&service, true); device.begin();
        QTRY_VERIFY(!service.busy()); QVERIFY(thread != QThread::currentThread());
        device.dispatch(Action::Right); QTRY_VERIFY(!service.busy()); QCOMPARE(service.snapshot().volume, 40);
        device.dispatch(Action::Confirm); QTRY_VERIFY(!service.busy()); QVERIFY(service.snapshot().muted);
        fail = true;
        device.dispatch(Action::Left); QTRY_VERIFY(!service.busy());
        QCOMPARE(service.snapshot().volume, 40); QVERIFY(!device.error().isEmpty());
        fail = false; device.dispatch(Action::Left); QTRY_VERIFY(!service.busy());
        QCOMPARE(service.snapshot().volume, 35); QVERIFY(device.error().isEmpty());
        device.dispatch(Action::Down); device.dispatch(Action::Left); QTRY_VERIFY(!service.busy());
        QCOMPARE(service.snapshot().brightness, 45);
        QSignalSpy closed(&device, &DeviceController::closeRequested);
        device.dispatch(Action::Back); QCOMPARE(closed.size(), 1);
    }
    void powerRequiresConfirmationAndWaitsForWork() {
        MockLibraryRepository library; MockTrainerRepository profiles; MockAdventureAdapter adapter;
        DevelopmentPlatformService platform; MockPokedexRepository dex; MockHallOfFameRepository archive; MockAchievementProvider achievements;
        ShellController shell(library, profiles, adapter, platform, dex, dex, archive, achievements);
        QSemaphore release;
        DeviceService service({[&] { release.tryAcquire(1, 3000); return DeviceSnapshot{}; }, [](const QString&, int) { return QString(); }});
        shell.device()->configure(&service, true);
        SessionState session(shell, nullptr);
        connect(&service, &DeviceService::changed, &session, [&] { session.setServiceActive(service.busy()); });
        QSignalSpy requested(&shell, &ShellController::modeRequested), ready(&session, &SessionState::exitReady);
        connect(&shell, &ShellController::modeRequested, &session, [&](const QString&) { session.requestExit(); });
        shell.dispatch(Action::SystemMenu); shell.activate(0); shell.activate(2);
        QCOMPARE(shell.service(), "device"); QVERIFY(service.busy());
        shell.activate(4); QVERIFY(shell.modeConfirmation()); shell.dispatch(Action::Back);
        QVERIFY(!shell.modeConfirmation()); QVERIFY(requested.isEmpty()); QCOMPARE(shell.focusIndex(), 4);
        shell.activate(3); shell.dispatch(Action::Confirm);
        QCOMPARE(requested.first().first().toString(), "reboot"); QVERIFY(ready.isEmpty());
        release.release(); QTRY_VERIFY(!ready.isEmpty());
        session.cancelPendingExit(); shell.dispatch(Action::Back);
        QCOMPARE(shell.service(), "settings"); QCOMPARE(shell.focusIndex(), 2);
        shell.dispatch(Action::Confirm); shell.dispatch(Action::NextPage);
        QVERIFY(!shell.serviceOpen()); QCOMPARE(shell.page(), 1);
        release.release();
    }
};
QTEST_GUILESS_MAIN(DeviceTests)
#include "DeviceTests.moc"
