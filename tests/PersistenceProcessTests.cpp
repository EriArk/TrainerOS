#include <QtTest>
#include <QProcess>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>

class PersistenceProcessTests final : public QObject {
    Q_OBJECT
private slots:
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
