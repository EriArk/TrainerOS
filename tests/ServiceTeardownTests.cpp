#include "platform/power/PowerControl.h"
#include "platform/device/NetworkService.h"
#include "platform/process/ProcessService.h"
#include "features/apps/AppsController.h"
#include "features/social/SocialMedia.h"
#include "app/SessionConnections.h"
#include <QtTest>
#include <QFile>
#include <cstdio>
#include <memory>

namespace trainer {
class ServiceTeardownTests final : public QObject {
    Q_OBJECT
    void startHelper(QProcess& process) {
        process.start(QCoreApplication::applicationFilePath(), {"--helper"});
        QVERIFY(process.waitForStarted(3000));
        QVERIFY(process.waitForReadyRead(3000));
    }
private slots:
    void sessionBindingsEndBeforeCapturedStateAndServices() {
        SessionConnections bindings;
        QObject receiver;
        auto service=std::make_unique<QObject>();
        auto state=std::make_unique<int>(0);
        int calls=0,internalCleanup=0;
        bindings.connect(service.get(),&QObject::destroyed,&receiver,[&]{++*state;++calls;});
        connect(service.get(),&QObject::destroyed,&receiver,[&]{++internalCleanup;});
        bindings.clear();bindings.clear();
        state.reset();service.reset();
        QCOMPARE(calls,0);QCOMPARE(internalCleanup,1);
    }
    void appsStopWithoutReenteringTheirOwner() {
        auto apps=std::make_unique<AppsController>();
        int changes=0;
        connect(apps.get(),&AppsController::changed,this,[&]{++changes;});
        apps->start(QCoreApplication::applicationFilePath(),"--helper");
        QTRY_VERIFY(apps->ready());
        const int before=changes;
        apps.reset();
        QCOMPARE(changes,before);
    }
    void mediaClearNotifiesButDestructionDoesNot() {
        auto media=std::make_unique<SocialMedia>();
        int changes=0;
        connect(media.get(),&SocialMedia::changed,this,[&]{++changes;});
        media->clear();QCOMPARE(changes,1);
        media.reset();QCOMPARE(changes,1);
    }
    void powerNotifiesWhileAliveButNotDuringDestruction() {
        auto power=std::make_unique<PowerControl>();
        int changes=0;
        connect(power.get(),&PowerControl::changed,this,[&]{++changes;});
        startHelper(power->process_);
        QVERIFY(power->available());
        power->process_.closeWriteChannel();
        QVERIFY(power->process_.waitForFinished(3000));
        QVERIFY(!power->available());
        QCOMPARE(power->error(),QString("Power service stopped."));
        startHelper(power->process_);
        QVERIFY(power->available());
        const int before=changes;
        power.reset(); // EOF causes output + finished during the destructor wait.
        QCOMPARE(changes,before);
    }
    void networkNotifiesWhileAliveButNotDuringDestruction() {
        auto network=std::make_unique<NetworkService>();
        int events=0,finished=0;
        connect(network.get(),&NetworkService::event,this,[&]{++events;});
        connect(network.get(),&NetworkService::finished,this,[&]{++finished;});
        startHelper(network->process_);
        QVERIFY(events>0);
        network->process_.closeWriteChannel();
        QVERIFY(network->process_.waitForFinished(3000));
        QCOMPARE(finished,1);
        startHelper(network->process_);
        const int before=events;
        network.reset();
        QCOMPARE(events,before);
        QCOMPARE(finished,1);
    }
    void processDestructionDoesNotRunRuntimeCallbacks() {
        auto process=std::make_unique<ProcessService>();
        int outputs=0,finished=0;
        connect(process.get(),&ProcessService::runtimeOutput,this,[&]{++outputs;});
        connect(process.get(),&ProcessService::finished,this,[&]{++finished;});
        ProcessCommand command{QCoreApplication::applicationFilePath(),{"--helper"}};
        command.runtimeControls={{"achievements",true}};
        QVERIFY(process->start(command));
        QTRY_VERIFY(outputs>0);
        const int before=outputs;
        process.reset();
        QCOMPARE(outputs,before);
        QCOMPARE(finished,0);
    }
};
}

int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    if(app.arguments().contains("--helper")) {
        std::puts("{\"event\":\"ready\",\"canSleep\":true}");std::fflush(stdout);
        while(std::getchar()!=EOF) {}
        std::puts("{\"event\":\"state\",\"keepAwake\":false}");std::fflush(stdout);
        return 0;
    }
    trainer::ServiceTeardownTests tests;
    return QTest::qExec(&tests,argc,argv);
}
#include "ServiceTeardownTests.moc"
