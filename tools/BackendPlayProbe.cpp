// Isolated CI host, never a production login shortcut. Test session stays in memory.
#include <QGuiApplication>
#include <QFile>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include "features/accounts/BackendAccount.h"
#include "features/device/ControlsDashboard.h"
#include "platform/sdk/PlatformHost.h"
using namespace trainer;
int main(int argc,char** argv) {
    QGuiApplication application(argc,argv);
    if(argc!=3)return 2;
    QFile input(QString::fromLocal8Bit(argv[1]));if(!input.open(QIODevice::ReadOnly)||input.size()>4096)return 2;
    const auto config=QJsonDocument::fromJson(input.readAll()).object();
    const QUrl origin(config["origin"].toString());
    if(origin.scheme()!="http"||origin.host()!="127.0.0.1"||origin.port()!=19091)return 2;
    QStandardPaths::setTestModeEnabled(true);
    application.setOrganizationName("TrainerOSPlayProbe");application.setApplicationName(config["owner"].toString());
    QByteArray saved=QJsonDocument(QJsonObject{{"refresh_token",config["refresh_token"]}}).toJson();
    QNetworkAccessManager network;
    BackendAccount account;
    account.setServices({[&](QByteArray method,QString path,QJsonObject body,QByteArray auth,BackendAccount::Reply done){
        QNetworkRequest request(QUrl(origin.toString()+path));
        request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
        request.setTransferTimeout(15000);if(!auth.isEmpty())request.setRawHeader("Authorization","Bearer "+auth);
        auto* reply=network.sendCustomRequest(request,method,body.isEmpty()?QByteArray():QJsonDocument(body).toJson(QJsonDocument::Compact));
        QObject::connect(reply,&QNetworkReply::finished,&application,[reply,done=std::move(done)]{
            done(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),QJsonDocument::fromJson(reply->readAll()).object());reply->deleteLater();
        });
    },[&](QString,auto done){done(true,saved);},[&](QString,QByteArray bytes,auto done){saved=bytes;done(true);},{}});
    ControlsDashboard dashboard;PlatformHost host(dashboard);host.setBackend(&account);
    const QString endpoint=QString::fromLocal8Bit(argv[2]);
    QObject::connect(&account,&BackendAccount::changed,&application,[&]{
        if(account.snapshot()["state"]!="signed_in")return;
        dashboard.setOwner(config["owner"].toString());
        host.setContext({{"profile",config["owner"]},{"account",account.snapshot()}});
        if(!QFile::exists(endpoint)){QString error;if(!host.start(endpoint,&error))application.exit(3);}
    });
    account.configure(config["owner"].toString());
    QTimer::singleShot(180000,&application,&QCoreApplication::quit);
    return application.exec();
}
