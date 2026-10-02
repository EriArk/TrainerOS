#include "platform/input/AdventureOverlayService.h"
#include <QtTest>
#include <QFile>
#include <QTemporaryDir>

using namespace trainer;
class AdventureOverlayServiceTests final : public QObject {
    Q_OBJECT
private slots:
    void helperEndingAfterGameDoesNotDiscardCaptureDuringSettlement() {
        QTemporaryDir directory;
        const auto helper=directory.filePath("helper.py"),control=directory.filePath("control");
        QImage capture(32,24,QImage::Format_RGB32);capture.fill(Qt::green);
        QVERIFY(capture.save(directory.filePath("source.png")));
        QFile file(helper);QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"PY(import json,sys,time,shutil
from pathlib import Path
control=Path(__file__).with_name('control')
def emit(event,**data):print(json.dumps(dict(event=event,**data)),flush=True)
emit('ready',protocol=2);emit('request')
for line in sys.stdin:
 c=json.loads(line);op=c['command']
 if op=='context':emit('input',epoch=c['epoch'],connected=True,neutral=True)
 elif op=='preview':emit('previewed',token=c['token'],ok=False)
 elif op=='capture':
  shutil.copyfile(Path(__file__).with_name('source.png'),Path(sys.argv[sys.argv.index('--directory')+1])/'frame.png')
  emit('captured',token=c['token'],ok=True)
 elif op=='close':control.write_text('exit');time.sleep(.1);break
)PY");file.close();
        ProcessService game;AdventureLaunchController launch(game);
        AdventureExitPresentation view(launch.exitController());
        AdventureOverlayService service(game,launch,view,helper);
        connect(&launch,&AdventureLaunchController::checkpointRequested,&launch,
            [&](quint64 token,const QJsonObject&){launch.checkpointCompleted(token,{});});
        QSignalSpy completed(&launch.exitController(),&AdventureExitController::completed);
        QSignalSpy failed(&launch.exitController(),&AdventureExitController::failed);
        ProcessCommand command{QDir(QCoreApplication::applicationDirPath()).filePath("trainer_process_probe"),
            {"controlled",control,directory.filePath("pid")},{}};
        command.settled=[](const ProcessOutcome&){QThread::msleep(500);};
        QVERIFY(launch.launch(command,{},"fixture"));
        QTRY_VERIFY(view.visible());view.setWindowFocused(true);QTRY_VERIFY(view.ready());
        view.activateMenu(1);QTRY_VERIFY(view.confirming());QTRY_VERIFY(view.ready());view.confirm();
        QTRY_VERIFY(!game.active());QCOMPARE(completed.size(),1);QVERIFY(failed.isEmpty());
        QCOMPARE(qvariant_cast<QImage>(completed.first()[1]),capture);QVERIFY(completed.first()[2].toBool());
    }
    void realTransportCancelsAndLosingCloseCannotStrandOrKillGame() {
        QTemporaryDir directory;
        const auto helper = directory.filePath("helper.py");
        QFile file(helper); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"PY(import json,sys
from pathlib import Path
marker=Path(__file__).with_suffix('.log')
def emit(event,**data): print(json.dumps(dict(event=event,**data)),flush=True)
emit('ready',protocol=2); emit('request')
for line in sys.stdin:
 c=json.loads(line); op=c['command']
 if op=='context': emit('input',epoch=c['epoch'],connected=True,neutral=True)
 elif op=='preview': emit('previewed',token=c['token'],ok=False)
 elif op=='capture': emit('captured',token=c['token'],ok=False)
 elif op=='cancel':
  marker.write_text('cancelled');emit('released');emit('request')
 elif op=='close':
  marker.write_text('close dispatched');sys.exit(1)
 elif op=='stop': break
)PY"); file.close();
        ProcessService game; AdventureLaunchController launch(game);
        AdventureExitPresentation view(launch.exitController());
        AdventureOverlayService service(game, launch, view, helper);
        connect(&launch, &AdventureLaunchController::checkpointRequested, &launch,
                [&](quint64 token, const QJsonObject&) { launch.checkpointCompleted(token, {}); });
        QSignalSpy completed(&launch.exitController(), &AdventureExitController::completed);
        QSignalSpy failed(&launch.exitController(), &AdventureExitController::failed);
        const auto probe = QDir(QCoreApplication::applicationDirPath()).filePath("trainer_process_probe");
        const auto control = directory.filePath("control");
        QVERIFY(launch.launch({probe, {"controlled", control, directory.filePath("pid")}, {}}, {}, "fixture"));
        QTRY_VERIFY(view.visible()); view.setWindowFocused(true);
        QTRY_VERIFY(view.ready()); QVERIFY(view.menuOpen());
        const auto pid = game.processId(); QVERIFY(pid > 0);
        view.cancel();
        QTRY_VERIFY(QFile::exists(directory.filePath("helper.log")));
        QTRY_VERIFY(view.ready()); QCOMPARE(game.processId(), pid);
        view.activateMenu(1); QTRY_VERIFY(view.confirming()); QTRY_VERIFY(view.ready()); view.confirm();
        QTRY_COMPARE(failed.size(), 1);
        QVERIFY(!view.visible()); QVERIFY(game.active()); QCOMPARE(game.processId(), pid);
        QVERIFY(completed.isEmpty());
        QFile command(control); QVERIFY(command.open(QIODevice::WriteOnly)); command.write("exit"); command.close();
        QTRY_VERIFY(!game.active()); QVERIFY(completed.isEmpty());
    }
};
QTEST_GUILESS_MAIN(AdventureOverlayServiceTests)
#include "AdventureOverlayServiceTests.moc"
