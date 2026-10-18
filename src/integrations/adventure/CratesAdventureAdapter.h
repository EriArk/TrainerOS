#pragma once
#include "AdventureAdapter.h"
#include "platform/process/ProcessService.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QCryptographicHash>
namespace trainer {
// Package runtime binding, not a game-specific Experience or save parser.
class CratesAdventureAdapter final : public AdventureAdapter {
public:
    QString root,helper;
    std::function<AdventureResult(const ProcessCommand&,const QString&)> requestLaunch;
    std::function<QString()> ownerId;
    QString id() const override{return "crates-native";}
    AdventureCapabilities capabilities(const Adventure& a) const override{return {a.adapterId==id()&&!a.collectionOnly,false,true};}
    QString setupIssue(const AdventureRegistration&) const override{return {};}
    QString verifyInstallation(const AdventureRegistration& r) const override {
        const auto project=r.adventure.id.mid(7);
        if(!r.adventure.id.startsWith("crates:")||!QRegularExpression("^[a-z0-9][a-z0-9._-]{0,79}$").match(project).hasMatch())return "Invalid package identity";
        QFile f(root+"/active/"+project+".json");
        if(!f.open(QIODevice::ReadOnly)||f.size()>1024*1024)return "Install this game from Crates first.";
        const auto receipt=QJsonDocument::fromJson(f.readAll()).object();
        return receipt["id"]==project&&receipt["kind"]=="game"?QString():QString("Package registration changed");
    }
    AdventureResult launch(const Adventure& a) override {
        AdventureRegistration r;r.adventure=a;const auto error=verifyInstallation(r);
        if(!error.isEmpty())return {false,error};
        const auto owner=ownerId?ownerId():QString();if(owner.isEmpty())return {false,"Choose a Trainer before playing"};
        ProcessCommand command;command.program="/usr/bin/python3";command.arguments={helper,root,a.id.mid(7),QString::fromLatin1(QCryptographicHash::hash(owner.toUtf8(),QCryptographicHash::Sha256).toHex())};
        command.runtimeControls={{"kind","native"}};
        return requestLaunch?requestLaunch(command,a.id):AdventureResult{false,"Game runtime is unavailable"};
    }
    AdventureResult resume(const Adventure& a,const ResumePoint&) override{return launch(a);}
};
}
