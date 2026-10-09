#pragma once
#include "core/model/Models.h"
#include <QMap>
#include <QVariantMap>
#include <QDir>
#include <QFileInfo>

namespace trainer {
// Rules are alternatives (OR). Every field of one rule must match (AND).
// Evidence is published by a catalogue/file/save observer, never by a QML view.
// This selects presentation only; semantic providers validate every read/write.
struct ExperienceMatchRule {
    QString id, platform;
    QMap<QString,QString> required;
};
struct ExperienceAssetFamily {
    QString id, profile;
    int version=1;
    QStringList extractionSources; // Empty means extraction is not implemented.
};
struct ExperienceManifest {
    QString id;
    int version=1;
    QList<ExperienceMatchRule> alternatives;
    QList<ExperienceAssetFamily> assets;
    int match(const std::optional<Adventure>& game, QVariantMap evidence={}) const {
        if(version!=1 || id.isEmpty() || !game)return 0;
        // Explicit stored legacy context is a migration fallback, not a ROM proof.
        evidence["legacy.domain"]=game->domain;
        evidence["local.game"]=game->id;
        evidence["catalog.traineros"]=game->catalogueId;
        int result=0;
        for(const auto& rule:alternatives) {
            if(rule.id.isEmpty() || rule.required.isEmpty() || (!rule.platform.isEmpty() && rule.platform!=game->platformId))continue;
            bool accepted=true;int score=1;
            for(auto i=rule.required.cbegin();i!=rule.required.cend();++i) {
                if(i.value().isEmpty() || evidence.value(i.key()).toString()!=i.value()){accepted=false;break;}
                if(i.key()!="legacy.domain")score=2;
            }
            if(accepted)result=qMax(result,score);
        }
        return result;
    }
    // User packs live outside replaceable adapter code. Existing private roots
    // are an explicit compatibility fallback only when no scoped family exists.
    QString assetRoot(const QString& state,const QString& family,const QString& legacy={}) const {
        if(state.isEmpty())return legacy;
        if(id.isEmpty() || id.contains('/') || id.contains('\\') || id=="..")return {};
        for(const auto& asset:assets)if(asset.id==family && !family.contains('/') && !family.contains('\\') && family!="..") {
            const auto root=QDir(state).filePath("adapters/"+id+"/packs/"+family);
            return QFileInfo::exists(root) || legacy.isEmpty()?root:legacy;
        }
        return {};
    }
};
}
