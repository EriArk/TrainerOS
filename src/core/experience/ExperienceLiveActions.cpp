#include "core/navigation/ShellController.h"
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
namespace trainer {
QVariantList ShellController::liveExperienceActions(const QString& game,const QString& session) {
    liveModuleActions_.clear();QVariantList result;
    if(game.isEmpty() || session.isEmpty())return result;
    const auto record=repository_.registration(game);
    if(!record || record->removed || !record->contentAvailable)return result;
    if(auto* module=resolveExperience(record->adventure)) {
        const auto descriptor=module->descriptor();

        const ExperienceLiveContext context{trainer_.profile()["id"].toString(),game,session,descriptor.id,record->revision,descriptor.version};
        for(const auto& value:module->liveActions(context)) {
            auto row=value.toMap();const auto action=row["id"].toString();
            if(action.isEmpty() || row["label"].toString().isEmpty())continue;
            const auto bytes=QJsonDocument(QJsonArray{context.owner,game,session,descriptor.id,record->revision,descriptor.version,action}).toJson(QJsonDocument::Compact);
            const auto id="experience-live:"+QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
            if(liveModuleActions_.contains(id))continue;
            row["id"]=id;result.append(row);liveModuleActions_.insert(id,{module,context,action});
        }
    }
    return result;
}
bool ShellController::invokeLiveExperienceAction(const QString& id,const QString& game,const QString& session) {
    if(!id.startsWith("experience-live:"))return false;
    const auto found=liveModuleActions_.constFind(id);
    if(found==liveModuleActions_.cend())return true;
    const auto captured=*found;
    // Rebuild from the actual process context, never the currently selected Home.
    const auto rows=liveExperienceActions(game,session);
    const auto current=liveModuleActions_.constFind(id);
    if(current==liveModuleActions_.cend() || current->module!=captured.module || current->context!=captured.context)return true;
    for(const auto& value:rows){const auto row=value.toMap();if(row["id"]==id && !row["readOnly"].toBool() && row.value("enabled",true).toBool()){
        captured.module->invokeLiveAction(captured.action,captured.context);return true;
    }}
    return true;
}
}
