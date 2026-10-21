// SPDX-License-Identifier: GPL-3.0-or-later
// Explicit live-network acceptance tool; never part of ordinary offline CTest.
#include <QtTest>
#include <QTemporaryDir>
#include <QProcessEnvironment>
#include <QSettings>
#include <QSysInfo>
#include "features/crates/CratesController.h"
#include "features/device/ControlsDashboard.h"
#include "platform/sdk/PlatformHost.h"
using namespace trainer;
static QString helpers;
class ReleaseCycle : public QObject {
    Q_OBJECT
private slots:
    void publishedPackages() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto isolatedHome=temp.path()+"/home";
        QVERIFY(QDir().mkpath(isolatedHome));
        qputenv("HOME",isolatedHome.toUtf8());
        const auto root=temp.path()+"/packages";
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());
        ControlsDashboard dashboard;dashboard.setOwner("release-check");
        PlatformHost host(dashboard);
        host.setContext({{"profile","release-check"},{"game",QJsonObject{{"state","idle"}}}});
        const auto xdg=temp.path()+"/share";
        QVERIFY(host.start(xdg+"/traineros/platform-v1/endpoint.json"));
        CratesController crates;
        crates.installer()->configure(helpers+"/installer.py",helpers+"/packages.json",root);
        crates.configureCatalogue(helpers+"/catalogue.py",temp.path()+"/catalogue");
        QVERIFY(crates.installer()->tasks().isEmpty());
        crates.open(1,true); // Same entry/Download/confirmation actions as the UI.
        QTRY_VERIFY_WITH_TIMEOUT(crates.syncStatus().isEmpty(),30000);
        QVERIFY(crates.projectCount()>=2);
        QVERIFY(crates.installer()->tasks().isEmpty()); // Browsing never installs.
        for(const auto& id: {QString("session-timer"),QString("pocket-artillery")}) {
            crates.closeDetail();
            for(int i=0;i<crates.projectCount();++i){crates.selectProject(i);if(crates.project()["id"]==id)break;}
            QCOMPARE(crates.project()["id"].toString(),id);
            crates.openProject(crates.projectIndex());
            QCOMPARE(crates.primaryLabel(),QString("Download"));
            crates.primary();QVERIFY(crates.actionsOpen());
            QCOMPARE(crates.actions().first().toMap()["id"].toString(),QString("confirm-install"));
            crates.activateAction(0);
            QTRY_COMPARE_WITH_TIMEOUT(crates.installer()->state(id)["installedVersion"].toString(),QString("0.1.0"),60000);
            QCOMPARE(crates.installer()->state(id)["job"].toMap()["state"].toString(),QString("done"));
            auto arch=QSysInfo::currentCpuArchitecture();if(arch=="arm64")arch="aarch64";
            bool matchingReceipt=false;
            for(const auto& item:crates.installer()->installed()) {
                const auto receipt=item.toMap();
                if(receipt["id"]==id){QCOMPARE(receipt["architecture"].toString(),arch);matchingReceipt=true;}
            }
            QVERIFY(matchingReceipt);
            const bool game=id=="pocket-artillery";
            QCOMPARE(crates.primaryLabel(),game?QString("Play"):QString("Open"));
            QSignalSpy launch(&crates,&CratesController::launchRequested);
            crates.primary();QCOMPARE(launch.size(),1);
            QCOMPARE(launch.first()[0].toString(),id);
            QCOMPARE(launch.first()[1].toString(),game?QString("game"):QString("app"));
            const auto desktop=QDir::homePath()+"/.local/share/applications/org.traineros.crates."+id+".desktop";
            QCOMPARE(QFile::exists(desktop),!game);
            auto env=QProcessEnvironment::systemEnvironment();
            env.insert("XDG_DATA_HOME",xdg);env.remove("TRAINEROS_SDK_ENDPOINT");
            env.insert("QT_QPA_PLATFORM","offscreen");
            QStringList args{helpers+"/launch.py",root,id};
            if(game)args<<QString(64,'a');
            QProcess child;child.setProcessEnvironment(env);
            child.start("/usr/bin/python3",args);QVERIFY(child.waitForStarted());
            const QString key="sdk:art.abysstail.session-timer:timer";
            if(!game) {
                QTRY_COMPARE_WITH_TIMEOUT(dashboard.available().size(),1,10000);
                dashboard.edit();dashboard.pick();dashboard.restore(key);dashboard.edit();dashboard.activate(key);
                QTRY_VERIFY(dashboard.widgets().first().toMap()["text"].toString().contains("00:00:01"));
            } else QTest::qWait(1800);
            QCOMPARE(child.state(),QProcess::Running);
            const auto errors=child.readAllStandardError();
            QVERIFY2(!errors.contains("failed to load")&&!errors.contains("is not installed"),errors.constData());
            QProcess busy;
            busy.start("/usr/bin/python3",{helpers+"/installer.py",temp.path()+"/catalogue/current.json",root,"remove",id,""});
            QVERIFY(busy.waitForReadyRead(10000));
            QVERIFY(busy.readAllStandardOutput().contains("Waiting for the application to close"));
            QCOMPARE(busy.state(),QProcess::Running);
            QVERIFY(QFile::exists(root+"/active/"+id+".json"));
            busy.terminate();QVERIFY(busy.waitForFinished(3000));QVERIFY(busy.exitCode()!=0);
            child.terminate();QVERIFY(child.waitForFinished(10000));
            if(!game)QTRY_COMPARE(dashboard.pages(),1);
            const auto data=root+"/data/"+id+(game?"/owners/"+QString(64,'a'):QString());
            QFile marker(data+"/retained.txt");QVERIFY(marker.open(QIODevice::WriteOnly));marker.write("retained");marker.close();
            crates.showActions();
            int remove=-1;
            for(int i=0;i<crates.actions().size();++i)if(crates.actions()[i].toMap()["id"]=="remove")remove=i;
            QVERIFY(remove>=0);crates.activateAction(remove);crates.activateAction(0);
            QTRY_VERIFY_WITH_TIMEOUT(crates.installer()->state(id)["installedVersion"].toString().isEmpty(),10000);
            QVERIFY(!QFile::exists(desktop));QVERIFY(marker.exists());
            crates.primary();crates.activateAction(0);
            QTRY_COMPARE_WITH_TIMEOUT(crates.installer()->state(id)["installedVersion"].toString(),QString("0.1.0"),60000);
            QVERIFY(marker.open(QIODevice::ReadOnly));QCOMPARE(marker.readAll(),QByteArray("retained"));marker.close();
            qInfo().noquote()<<id<<"public download, install, launch, busy lease, remove, reinstall and preserved data: PASS";
        }
        // HOME and package storage are both disposable; no player profile is used.
    }
};
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    app.setOrganizationName("TrainerOSReleaseCheck");app.setApplicationName("CratesReleaseCheck");
    if(argc!=2)return 2;
    helpers=QString::fromLocal8Bit(argv[1]);
    ReleaseCycle test;return QTest::qExec(&test,1,argv);
}
#include "main.moc"
