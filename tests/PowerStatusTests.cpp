#include "platform/power/PowerStatus.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QSemaphore>

using namespace trainer;
namespace {
void write(const QString& path, const QByteArray& text) {
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(text), text.size());
}
void battery(const QString& root, const QString& name, const QByteArray& capacity = "55", const QByteArray& status = "Discharging") {
    write(root+"/"+name+"/type", "Battery\n"); write(root+"/"+name+"/capacity", capacity); write(root+"/"+name+"/status", status);
}
}
class PowerStatusTests final : public QObject {
    Q_OBJECT
private slots:
    void selectsSystemBatteryAndIgnoresPeripheralAndUsb() {
        QTemporaryDir dir; battery(dir.path(), "battery");
        battery(dir.path(), "controller", "90"); write(dir.filePath("controller/scope"), "Device");
        write(dir.filePath("usb/type"), "USB"); write(dir.filePath("usb/capacity"), "100");
        const auto result = readBatteryStatus(dir.path()); QCOMPARE(result.percent, 55); QCOMPARE(result.state, "Discharging");
        write(dir.filePath("battery/capacity"), "100\n"); write(dir.filePath("battery/status"), "Charging\n");
        QCOMPARE(readBatteryStatus(dir.path()).percent, 100); QCOMPARE(readBatteryStatus(dir.path()).state, "Charging");
        write(dir.filePath("battery/capacity"), "0"); QCOMPARE(readBatteryStatus(dir.path()).percent, 0);
    }
    void missingMalformedAbsentAndAmbiguousStayUnknown() {
        QTemporaryDir dir; QCOMPARE(readBatteryStatus(dir.path()).percent, -1);
        battery(dir.path(), "battery");
        for (const auto& bad : {QByteArray("101"), QByteArray("-1"), QByteArray("25.5"), QByteArray("50%"), QByteArray(300,'9')}) {
            write(dir.filePath("battery/capacity"), bad); QCOMPARE(readBatteryStatus(dir.path()).percent, -1);
        }
        battery(dir.path(), "battery", "55", "Unsupported"); QCOMPARE(readBatteryStatus(dir.path()).state, "Unknown");
        write(dir.filePath("battery/present"), "0"); QCOMPARE(readBatteryStatus(dir.path()).percent, -1);
        write(dir.filePath("battery/present"), "1"); battery(dir.path(), "second"); QCOMPARE(readBatteryStatus(dir.path()).percent, -1);
    }
    void slowRefreshDoesNotBlockInputAndFailureClearsOldPercentage() {
        QSemaphore entered, release; int calls = 0;
        PowerStatus status([&] { entered.release(); release.acquire(); return ++calls == 1 ? BatterySnapshot{55,"Discharging"} : BatterySnapshot{}; });
        struct Unblock { QSemaphore& semaphore; ~Unblock(){semaphore.release();} } unblock{release};
        QSignalSpy refreshed(&status, &PowerStatus::refreshed);
        status.refresh(); QTRY_VERIFY(entered.available() > 0); status.refresh();
        int ticks = 0; QTimer timer; connect(&timer,&QTimer::timeout,this,[&]{++ticks;}); timer.start(5); QTest::qWait(30); QVERIFY(ticks > 0);
        release.release(); QTRY_COMPARE(refreshed.size(),1); QCOMPARE(status.percent(),55); QVERIFY(status.available());
        entered.acquire(); status.refresh(); QTRY_VERIFY(entered.available() > 0); release.release();
        QTRY_COMPARE(refreshed.size(),2); QVERIFY(!status.available()); QCOMPARE(status.percent(),-1); QVERIFY(!status.charging());
    }
};
QTEST_GUILESS_MAIN(PowerStatusTests)
#include "PowerStatusTests.moc"
