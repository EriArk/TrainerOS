#include "platform/device/VolumeKeys.h"
#include "features/device/DeviceController.h"
#include "core/navigation/ShellController.h"
#include "core/storage/SessionState.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QSemaphore>
#include <QJsonArray>

using namespace trainer;
namespace {
class NetworkProbe : public NetworkService {
public:
    QJsonObject requestValue, reply;
    int requests=0, cancellations=0;
    void request(const QJsonObject& value) override {requestValue=value;++requests;}
    void respond(const QJsonObject& value) override {reply=value;}
    void cancel() override {++cancellations;}
    void list(const QJsonArray& rows) {emit event({{"event","snapshot"},{"rows",rows}});emit finished({});}
};
void write(const QString& path, const QByteArray& value) {
    QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(value), value.size());
}
}
class DeviceTests : public QObject {
    Q_OBJECT
private slots:
    void networkPasswordsCancelAndStableSelection() {
        NetworkProbe service; NetworkController network;network.configure(&service);
        QSignalSpy text(&network,&NetworkController::textRequested);
        const QJsonObject first{{"id","ap:one"},{"title","First"},{"security","wpa-psk"},{"saved",false}};
        const QJsonObject second{{"id","saved:two"},{"title","Second"},{"security","wpa-psk"},{"saved",true}};
        network.setActive(true);QCOMPARE(service.requestValue["op"].toString(),"list");QVERIFY(network.busy());
        service.list({first,second});network.activate(0);QCOMPARE(text.size(),1);QVERIFY(text.first()[2].toBool());
        network.cancelText();QCOMPARE(service.requests,1);network.activate(0);network.applyText("test password");
        QCOMPARE(service.requestValue["password"].toString(),"test password");QVERIFY(network.busy());
        network.dispatch(Action::Back);QCOMPARE(service.cancellations,1);QVERIFY(network.busy());
        service.list({first,second});network.dispatch(Action::Down);QCOMPARE(network.focusIndex(),1);
        service.list({second,first});QCOMPARE(network.focusIndex(),0);QVERIFY(network.canForget());
        network.dispatch(Action::LocalAction);QVERIFY(!network.prompt().isEmpty());
        const int requests=service.requests;network.dispatch(Action::Back);QCOMPARE(service.requests,requests);
        network.dispatch(Action::LocalAction);network.dispatch(Action::Confirm);
        QCOMPARE(service.requestValue["op"].toString(),"forget");QCOMPARE(service.requestValue["id"].toString(),"saved:two");
        network.setActive(false);QCOMPARE(service.cancellations,2);
    }
    void bluetoothPairingOwnsItsConfirmationAndKeyboard() {
        NetworkProbe service;NetworkController network;network.configure(&service);
        QSignalSpy keyboard(&network,&NetworkController::textRequested);
        network.setActive(true);service.list({});
        network.selectFace(true);service.list({QJsonObject{{"id","/device"},{"title","Gamepad"},{"saved",false}}});
        network.activate(0);QCOMPARE(service.requestValue["op"].toString(),"pair");
        emit service.event({{"event","prompt"},{"kind","confirm"},{"text","Confirm 123456"}});
        QCOMPARE(network.prompt(),"Confirm 123456");network.dispatch(Action::Confirm);
        QVERIFY(service.reply["accept"].toBool());QVERIFY(network.busy());
        emit service.event({{"event","prompt"},{"kind","passkey"}});
        QCOMPARE(keyboard.size(),1);QCOMPARE(keyboard.first()[1].toInt(),6);
        network.applyText("123456");QCOMPARE(service.reply["value"].toString(),"123456");
        emit service.event({{"event","prompt"},{"kind","display"},{"text","Enter code on device"}});
        QCOMPARE(network.confirmLabel(),"Wait");network.dispatch(Action::Back);QCOMPARE(service.cancellations,1);
        emit service.finished("Cancelled");QVERIFY(!network.busy());QVERIFY(network.prompt().isEmpty());
        network.setActive(false);
        emit service.event({{"event","prompt"},{"kind","passkey"}});
        QCOMPARE(keyboard.size(),1);QCOMPARE(service.cancellations,2);
    }
    void radioControlsReadBackAndDoNotInventSuccessfulWrites() {
        DeviceSnapshot value; value.wifi=1;value.bluetooth=0;value.airplane=0;
        bool fail=false;
        DeviceService service({[&]{return value;},[&](const QString& control,int target){
            if(fail)return QString("Radio unavailable");
            if(control=="wifi")value.wifi=target;
            if(control=="bluetooth")value.bluetooth=target;
            if(control=="airplane"){value.airplane=target;value.wifi=!target;value.bluetooth=!target;}
            return QString();
        }});
        DeviceController device;device.configure(&service,true);device.begin();QTRY_VERIFY(!service.busy());
        device.adjustQuick(3,Action::Confirm);QTRY_VERIFY(!service.busy());QCOMPARE(service.snapshot().bluetooth,1);
        fail=true;device.adjustQuick(2,Action::Confirm);QTRY_VERIFY(!service.busy());QCOMPARE(service.snapshot().wifi,1);QVERIFY(!device.error().isEmpty());
        fail=false;device.adjustQuick(4,Action::Confirm);QTRY_VERIFY(!service.busy());QCOMPARE(service.snapshot().airplane,1);QCOMPARE(service.snapshot().wifi,0);
        SettingsController settings;settings.selectCategory(10);QSignalSpy requested(&settings,&SettingsController::quickAdjustment);
        settings.dispatch(Action::Down);settings.dispatch(Action::Confirm);QCOMPARE(requested.size(),0);
    }
    void volumeReadingsAreHonest() {
        QCOMPARE(parseVolume("Volume: 0.11\n").volume, 11);
        const auto muted = parseVolume("Volume: 0.35 [MUTED]\n");
        QCOMPARE(muted.volume, 35); QVERIFY(muted.muted);
        QCOMPARE(parseVolume("Volume: 1.25").volume, 125);
        for (const auto& value : {"", "Volume: nan", "Volume: -1", "Volume: 0,11", "Volume: 11", "error\nVolume: 0.50"})
            QCOMPARE(parseVolume(value).volume, -1);
    }
    void radioTogglesQueueDuringRefreshAndKeepTheirOrder() {
        DeviceSnapshot value; value.wifi = 1; value.bluetooth = 1;
        QSemaphore release;
        bool block = false;
        DeviceService service({[&] {
            if (block) release.tryAcquire(1, 2000);
            return value;
        }, [&](const QString& control, int target) {
            if (control == "bluetooth") value.bluetooth = target;
            return QString();
        }});
        service.refresh(); QTRY_VERIFY(!service.busy());
        block = true; service.refresh();
        service.toggleRadio("bluetooth"); service.toggleRadio("bluetooth");
        release.release(3); QTRY_VERIFY(!service.busy());
        QCOMPARE(service.snapshot().bluetooth, 1);
        block = false; service.toggleRadio("bluetooth");
        QTRY_VERIFY(!service.busy()); QCOMPARE(service.snapshot().bluetooth, 0);
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
        device.setQuickLevel(1,0); QTRY_VERIFY(!service.busy()); QCOMPARE(service.snapshot().brightness,5);
        device.setQuickLevel(1,50); QTRY_VERIFY(!service.busy()); QCOMPARE(service.snapshot().brightness,50);
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
    void coalescedControlsAndExternalVolume() {
        DeviceSnapshot value; value.volume = 35; value.brightness = 50;
        int writes = 0; bool fail = false;
        DeviceService service({[&] { return value; }, [&](const QString& control, int setting) {
            ++writes; QThread::msleep(20);
            if (fail) return QString("Rejected");
            if (control == "volume") value.volume = setting;
            if (control == "brightness") value.brightness = setting;
            if (control == "mute") value.muted = setting;
            return QString();
        }});
        service.refresh(); QTRY_VERIFY(!service.busy());
        for (int i = 0; i < 8; ++i) service.adjust("volume", 5);
        service.adjust("brightness", -5); service.toggleMute();
        QTRY_VERIFY(!service.busy());
        QCOMPARE(service.snapshot().volume, 75); QCOMPARE(service.snapshot().brightness, 45);
        QVERIFY(service.snapshot().muted); QVERIFY(writes <= 4);
        value.volume = 20; // An external hardware/desktop change, absent from our snapshot.
        service.hardwareVolume(5); QTRY_VERIFY(!service.busy()); QCOMPARE(service.snapshot().volume, 25);
        fail = true; service.adjust("volume", 5); service.adjust("volume", 5);
        QTRY_VERIFY(!service.busy()); QCOMPARE(service.snapshot().volume, 25); QCOMPARE(service.error(), "Rejected");
        service.refresh(); QTRY_VERIFY(!service.busy()); QCOMPARE(service.error(), "Rejected");
        fail = false; service.adjust("volume", -100); QTRY_VERIFY(!service.busy());
        QCOMPARE(service.snapshot().volume, 0); QVERIFY(service.error().isEmpty());
        service.adjust("brightness", -100); QTRY_VERIFY(!service.busy()); QCOMPARE(service.snapshot().brightness, 5);
        service.adjust("volume", 200); service.adjust("brightness", 200);
        QTRY_VERIFY(!service.busy()); QCOMPARE(service.snapshot().volume, 100); QCOMPARE(service.snapshot().brightness, 100);
    }
    void volumeKeyEdgesAndRelease() {
        VolumeKeys keys(false); QSignalSpy changes(&keys, &VolumeKeys::adjustmentRequested);
        keys.keyEvent(116, 1); QVERIFY(changes.isEmpty()); // Never power.
        keys.keyEvent(115, 1); keys.keyEvent(115, 2); keys.keyEvent(115, 1);
        QCOMPARE(changes.size(), 1); QCOMPARE(changes.first().first().toInt(), 5);
        keys.keyEvent(115, 0); QTest::qWait(480); QCOMPARE(changes.size(), 1);
        keys.keyEvent(114, 1); QTRY_VERIFY_WITH_TIMEOUT(changes.size() >= 3, 700);
        keys.keyEvent(114, 0); const auto count = changes.size(); QTest::qWait(150); QCOMPARE(changes.size(), count);
    }
    void startPowerAndQuickControls() {
        MockLibraryRepository library; MockTrainerRepository profiles; MockAdventureAdapter adapter;
        DevelopmentPlatformService platform; MockPokedexRepository dex; MockHallOfFameRepository archive; MockAchievementProvider achievements;
        ShellController shell(library, profiles, adapter, platform, dex, dex, archive, achievements);
        DeviceSnapshot value; value.volume = 35; value.brightness = 60;
        DeviceService service({[&] { return value; }, [&](const QString& control, int setting) {
            if (control == "volume") value.volume = setting;
            if (control == "brightness") value.brightness = setting;
            if (control == "mute") value.muted = setting;
            return QString();
        }});
        shell.device()->configure(&service, true);
        QSignalSpy requested(&shell, &ShellController::modeRequested);
        shell.dispatch(Action::SystemMenu); QTRY_VERIFY(!service.busy());
        shell.activate(7); QTRY_VERIFY(!service.busy()); QVERIFY(service.snapshot().muted);
        shell.dispatch(Action::Right); QTRY_VERIFY(!service.busy()); QCOMPARE(service.snapshot().volume, 40);
        shell.activate(6); QVERIFY(shell.powerMenu()); QCOMPARE(shell.focusIndex(), 2);
        shell.dispatch(Action::Confirm); QVERIFY(!shell.powerMenu()); QCOMPARE(shell.focusIndex(), 6);
        shell.dispatch(Action::Confirm); shell.activate(0); QVERIFY(shell.modeConfirmation()); QVERIFY(requested.isEmpty());
        shell.dispatch(Action::Back); QVERIFY(shell.powerMenu()); QVERIFY(requested.isEmpty());
        QCOMPARE(shell.device()->focusIndex(), 0); // Overlay must not move the underlying service focus.
        shell.activate(1); shell.dispatch(Action::NextPage); QVERIFY(!shell.modeConfirmation()); QVERIFY(!shell.powerMenu());
        shell.dispatch(Action::SystemMenu); shell.activate(6); shell.activate(1); shell.dispatch(Action::Confirm);
        QCOMPARE(requested.size(), 1); QCOMPARE(requested.first().first().toString(), "reboot");
        shell.dispatch(Action::Back); shell.dispatch(Action::Back); QVERIFY(!shell.menuOpen());
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
        shell.dispatch(Action::SystemMenu); shell.activate(0); shell.activate(5);
        QCOMPARE(shell.service(), "settings"); QVERIFY(service.busy());
        shell.activate(2); QVERIFY(shell.modeConfirmation()); shell.dispatch(Action::Back);
        QVERIFY(!shell.modeConfirmation()); QVERIFY(requested.isEmpty()); QCOMPARE(shell.focusIndex(), 2);
        shell.activate(1); shell.dispatch(Action::Confirm);
        QCOMPARE(requested.first().first().toString(), "reboot"); QVERIFY(ready.isEmpty());
        release.release(); QTRY_VERIFY(!ready.isEmpty());
        session.cancelPendingExit(); shell.dispatch(Action::Back);
        QCOMPARE(shell.service(), "settings"); QCOMPARE(shell.settings()->category(),5); QCOMPARE(shell.focusIndex(),5);
        shell.dispatch(Action::Confirm); shell.dispatch(Action::NextPage);
        QVERIFY(!shell.serviceOpen()); QCOMPARE(shell.page(), 1);
        release.release();
    }
};
QTEST_GUILESS_MAIN(DeviceTests)
#include "DeviceTests.moc"
