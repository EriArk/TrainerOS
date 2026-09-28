#include "platform/emulation/EmulatorRefresh.h"
#include <QtTest>
#include <QSemaphore>
#include <atomic>
using namespace trainer;

class EmulatorRefreshTests : public QObject {
    Q_OBJECT
private slots:
    void defersDuringGameAndKeepsGuiResponsive() {
        std::atomic<int> reads = 0;
        QSemaphore finish;
        bool idle = false; int applied = 0; bool scan = false;
        EmulatorRefresh refresh([&](const QString& root) {
            ++reads; finish.tryAcquire(1, 3000);
            EmulatorEnvironment result; result.libraryRoot = root; return result;
        });
        refresh.idle = [&] { return idle; };
        refresh.apply = [&](const EmulatorEnvironment& value, bool requestedScan) {
            QCOMPARE(value.libraryRoot, QString("roms")); ++applied; scan = requestedScan;
        };
        refresh.request("roms"); QTest::qWait(300); QCOMPARE(reads.load(), 0);
        idle = true; QTRY_COMPARE(reads.load(), 1);
        // A game starts while the read is outstanding. Inventory may finish,
        // but the running game's launch/save snapshot must remain unchanged.
        idle = false; refresh.request("roms", true); finish.release(2);
        int frames = 0; QTimer::singleShot(0, [&] { ++frames; });
        QTRY_COMPARE(frames, 1); QTest::qWait(350); QCOMPARE(applied, 0);
        idle = true; QTRY_COMPARE(applied, 1); QVERIFY(scan); QCOMPARE(reads.load(), 2);
    }
    void changedStorageDiscardsStaleInventory() {
        std::atomic<int> reads = 0; QSemaphore finish;
        QStringList applied;
        EmulatorRefresh refresh([&](const QString& root) {
            if (++reads == 1) finish.tryAcquire(1, 3000);
            EmulatorEnvironment result; result.libraryRoot = root; return result;
        });
        refresh.idle = [] { return true; };
        refresh.apply = [&](const EmulatorEnvironment& result, bool scan) { applied << result.libraryRoot; QVERIFY(scan); };
        refresh.request("removed-card"); QTRY_COMPARE(reads.load(), 1);
        refresh.request("replacement-card", true); finish.release();
        QTRY_COMPARE(applied, QStringList{"replacement-card"}); QCOMPARE(reads.load(), 2);
    }
};
QTEST_GUILESS_MAIN(EmulatorRefreshTests)
#include "EmulatorRefreshTests.moc"
