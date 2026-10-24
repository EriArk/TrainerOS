#pragma once
#include "core/navigation/ShellController.h"
#include <QJsonDocument>

namespace trainer {
// Local-only provider responses, never a real account or network request.
class FirstRunAccountFixture {
public:
    static bool prepareScraper(ShellController& shell, const QString& directory) {
        if(!scraper::writeCredentials(directory+"/secrets/screenscraper.json",{"fixture-developer","fixture-secret","SetupUser","fixture-password"}))return false;
        shell.scraper()->configure(directory);
        shell.scraper()->setTransport([](const QUrl&,qint64,const scraper::Cancellation&) {
            return scraper::Reply{200,QJsonDocument(QJsonObject{{"response",QJsonObject{{"ssuser",QJsonObject{{"id","SetupUser"},{"maxthreads","1"}}}}}}).toJson(),0};
        });
        return true;
    }
    static void connectSocial(ShellController& shell) {
        shell.social()->receive(shell.social()->accountGeneration(),{{"state","connected"},{"userId","100000000000000010"},{"name","Setup Test"}});
    }
};
}
