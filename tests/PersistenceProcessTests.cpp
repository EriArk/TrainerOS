#include <QtTest>
#include <QProcess>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>

class PersistenceProcessTests final : public QObject {
    Q_OBJECT
private slots:
    void controllerBackupRestoreAndProtection() {
        QTemporaryDir dir;QVERIFY(dir.isValid());
        const auto executable=QDir(QCoreApplication::applicationDirPath()).filePath(
#ifdef Q_OS_WIN
            "traineros.exe"
#else
            "traineros"
#endif
        );
        QProcess p;p.setProcessChannelMode(QProcess::MergedChannels);
        p.start(executable,{"--persistence-smoke-test","library-center","--data-dir",dir.path(),"--screenshot-dir",QCoreApplication::applicationDirPath()+"/screenshots/center"});
        QVERIFY(p.waitForStarted());
        if(!p.waitForFinished(25000)){p.kill();p.waitForFinished();QFAIL(qPrintable(p.readAll()));}
        const auto output=p.readAll();QVERIFY2(p.exitCode()==0&&p.exitStatus()==QProcess::NormalExit,output.constData());
        QFile save(dir.filePath("content/center.gba.srm"));QVERIFY(save.open(QIODevice::ReadOnly));QCOMPARE(save.readAll(),QByteArray("SECOND SAVE"));
    }
    void homeSelectionAndHistoryAcrossRestart() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const auto executable = QDir(QCoreApplication::applicationDirPath()).filePath(
#ifdef Q_OS_WIN
            "traineros.exe"
#else
            "traineros"
#endif
        );
        const auto screenshots = QCoreApplication::applicationDirPath() + "/screenshots/home";
        for (const auto& phase : {QString("library-home"), QString("library-home-reopen")}) {
            QProcess p; p.setProcessChannelMode(QProcess::MergedChannels);
            p.start(executable, {"--persistence-smoke-test", phase, "--data-dir", dir.path(), "--screenshot-dir", screenshots});
            QVERIFY(p.waitForStarted());
            if (!p.waitForFinished(18000)) { p.kill(); p.waitForFinished(); QFAIL(qPrintable(p.readAll())); }
            const auto output = p.readAll(); QVERIFY2(p.exitCode() == 0 && p.exitStatus() == QProcess::NormalExit, qPrintable(phase + " exit " + QString::number(p.exitCode()) + ": " + output));
        }
    }
    void controllerCollectionAttachment() {
        QTemporaryDir dir; QVERIFY(dir.isValid()); QVERIFY(QDir().mkpath(dir.filePath("content")));
        { QFile f(dir.filePath("content/test.gba")); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("Content-free collection fixture"); }
        const auto app = QDir(QCoreApplication::applicationDirPath()).filePath(
#ifdef Q_OS_WIN
            "traineros.exe"
#else
            "traineros"
#endif
        );
        QProcess p; p.setProcessChannelMode(QProcess::MergedChannels);
        p.start(app, {"--persistence-smoke-test", "collection", "--data-dir", dir.path(), "--screenshot-dir", QCoreApplication::applicationDirPath() + "/screenshots/collection"});
        QVERIFY(p.waitForStarted());
        if (!p.waitForFinished(15000)) { p.kill(); p.waitForFinished(); QFAIL(qPrintable(p.readAll())); }
        const auto output = p.readAll(); QVERIFY2(p.exitCode() == 0 && p.exitStatus() == QProcess::NormalExit, output.constData());
    }
    void operatingSystemCanTerminateTheShell() {
#ifdef Q_OS_UNIX
        QTemporaryDir dir; QVERIFY(dir.isValid());
        QProcess process; process.setProcessChannelMode(QProcess::MergedChannels);
        const auto executable = QDir(QCoreApplication::applicationDirPath()).filePath("traineros");
        process.start(executable, {"--windowed", "--data-dir", dir.path()});
        QVERIFY(process.waitForStarted());
        // The database opens after controller initialization and QML construction.
        QTRY_VERIFY(QFile::exists(dir.filePath("traineros.sqlite3")));
        QTest::qWait(100);
        process.terminate();
        const bool stopped = process.waitForFinished(2000);
        if (!stopped) { process.kill(); process.waitForFinished(); }
        QVERIFY2(stopped, "SDL must not swallow SIGTERM and force systemd to kill the shell after its stop timeout.");
#else
        QSKIP("POSIX service termination check");
#endif
    }
    void controllerLibraryAndSettingsAcrossRestart() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const auto content = dir.path() + "/content"; QVERIFY(QDir().mkpath(content + "/empty"));
        { QFile file(content + "/journey.bin"); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("Original content-free test fixture.\n"); }
        const auto app = QDir(QCoreApplication::applicationDirPath()).filePath(
#ifdef Q_OS_WIN
            "traineros.exe"
#else
            "traineros"
#endif
        );
        const auto screenshots = QDir(QCoreApplication::applicationDirPath()).filePath("screenshots/library");
        for (const auto& phase : {QString("library-seed"), QString("library-verify"), QString("library-final"), QString("library-launch")}) {
            if (phase == "library-verify") QVERIFY(QFile::rename(content + "/journey.bin", content + "/journey-moved.bin"));
            QProcess process; process.setProcessChannelMode(QProcess::MergedChannels);
            process.start(app, {"--persistence-smoke-test", phase, "--data-dir", dir.path(), "--screenshot-dir", screenshots});
            QVERIFY(process.waitForStarted());
            if (!process.waitForFinished(18000)) { process.kill(); process.waitForFinished(); QFAIL(qPrintable(phase + ": timed out\n" + process.readAll())); }
            const auto output = process.readAll();
            QVERIFY2(process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0, output.constData());
        }
        QFile file(content + "/journey-moved.bin"); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("Original content-free test fixture.\n"));
    }
    void controllerSaveRestartAndRecovery() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const auto app = QDir(QCoreApplication::applicationDirPath()).filePath(
#ifdef Q_OS_WIN
            "traineros.exe"
#else
            "traineros"
#endif
        );
        const auto screenshots = QDir(QCoreApplication::applicationDirPath()).filePath("screenshots/persistence");
        for (const auto& phase : {QString("seed"), QString("verify"), QString("error")}) {
            const auto data = dir.path() + (phase == "error" ? "/corrupt" : "/user");
            if (phase == "error") {
                QVERIFY(QDir().mkpath(data)); QFile file(data + "/traineros.sqlite3");
                QVERIFY(file.open(QIODevice::WriteOnly)); file.write("preserve me");
            }
            QProcess process; process.setProcessChannelMode(QProcess::MergedChannels);
            process.start(app, {"--persistence-smoke-test", phase, "--data-dir", data, "--screenshot-dir", screenshots});
            QVERIFY(process.waitForStarted());
            if (!process.waitForFinished(20000)) { process.kill(); process.waitForFinished(); QFAIL(qPrintable(phase + ": child timed out\n" + process.readAll())); }
            const auto output = process.readAll();
            QVERIFY2(process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0, output.constData());
            if (phase == "error") { QFile file(data + "/traineros.sqlite3"); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("preserve me")); }
        }
    }
};
QTEST_GUILESS_MAIN(PersistenceProcessTests)
#include "PersistenceProcessTests.moc"
