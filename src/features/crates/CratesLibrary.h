#pragma once
#include "core/repository/LibraryRepository.h"
#include <QTimer>
#include <QSet>
namespace trainer {
// Serial reconciliation retains library identity/history through remove/reinstall.
class CratesLibrary final : public QObject {
    Q_OBJECT
public:
    CratesLibrary(LibraryRepository& library,QString root,QObject* parent=nullptr):QObject(parent),library_(library),root_(std::move(root)){}
    void update(const QVariantList& packages){packages_=packages;reconcile();}
signals:
    void changed();
    void error(QString);
private:
    void reconcile(){
        if(busy_)return;QSet<QString> live;
        for(const auto& v:packages_){const auto p=v.toMap();if(p["kind"]!="game")continue;
            const QString id="crates:"+p["id"].toString();live.insert(id);
            const auto previous=library_.registration(id);
            if(previous&&previous->removed){edit(LibraryEditKind::RestorePackage,*previous);return;}
            if(previous&&previous->integrationConfig["version"].toString()==p["version"].toString())continue;
            AdventureRegistration r=previous.value_or(AdventureRegistration{});
            r.adventure.id=id;r.adventure.title=p["title"].toString();r.adventure.adapterId="crates-native";
            r.adventure.platformId="ports";r.adventure.domain="multiverse";
            r.contentPath=root_+"/active/"+p["id"].toString()+".json";
            r.integrationConfig={{"version",p["version"].toString()}};
            busy_=true;library_.saveAdventureAsync(r,this,[this](LibraryWriteResult r){finish(r.success?QString():r.error);});return;
        }
        for(const auto& r:library_.registrations())if(r.adventure.adapterId=="crates-native"&&!r.removed&&!live.contains(r.adventure.id)){
            edit(LibraryEditKind::DeactivatePackage,r);return;
        }
        emit changed();
    }
    void edit(LibraryEditKind kind,const AdventureRegistration& r){busy_=true;library_.editLibraryAsync({kind,r.adventure.id,r.revision},this,[this](QString e){finish(e);});}
    void finish(const QString& e){busy_=false;if(!e.isEmpty()){emit error(e);return;}QTimer::singleShot(0,this,[this]{reconcile();});}
    LibraryRepository& library_;QString root_;QVariantList packages_;bool busy_=false;
};
}
