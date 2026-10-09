#include "ExperienceNavigation.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

namespace trainer {
const ExperienceDescriptor& genericExperienceDescriptor() {
    static const ExperienceDescriptor value{"generic",1,"game-home",
        {"Game",{{"details","Details","game-details"}}},
        {"History",{{"sessions","Sessions","game-history"},{"ra","Achievements","achievements"}}}};
    return value;
}
const ExperienceDescriptor& pokemonExperienceDescriptor() {
    static const ExperienceDescriptor value{"pokemon",1,"pokemon-home",
        {"Companions",{{"dex","Guide","pokemon-guide"},{"party","Party","pokemon-party"},
            {"boxes","Boxes","pokemon-boxes"},{"center","Center","pokemon-center"},
            {"playroom","Playroom","pokemon-playroom"},{"shops","Shops","pokemon-shops"}}},
        {"Trainer",{{"profile","Profile","pokemon-persona"},{"journey","Journey","pokemon-journey"},
            {"hall","Hall","pokemon-hall"},{"ra","RA","achievements"}}}};
    return value;
}
ExperienceDescriptor ExperienceNavigation::resolve(const std::optional<Adventure>& game) {
    return game && game->domain=="pokemon" ? pokemonExperienceDescriptor() : genericExperienceDescriptor();
}
bool ExperienceNavigation::valid(const ExperienceDescriptor& descriptor) {
    if(descriptor.id.isEmpty() || descriptor.version!=1 || descriptor.homeView.isEmpty())return false;
    for(const auto* group:{&descriptor.first,&descriptor.second}) {
        if(group->label.isEmpty() || group->faces.isEmpty())return false;
        QSet<QString> ids;
        for(const auto& face:group->faces) {
            if(face.id.isEmpty() || face.label.isEmpty() || face.view.isEmpty() || ids.contains(face.id))return false;
            ids.insert(face.id);
        }
    }
    return true;
}
bool ExperienceNavigation::select(const QString& owner,const QString& adventure,
        const ExperienceDescriptor& descriptor,int revision) {
    const auto next=valid(descriptor)?descriptor:genericExperienceDescriptor();
    // Descriptors come from the trusted registry. A changed version or content
    // revision invalidates outstanding callbacks even with the same Adventure.
    const bool changed=owner_!=owner || adventure_!=adventure || descriptor_!=next
        || registrationRevision_!=revision;
    if(!changed)return false;
    if(owner_!=owner)states_.clear();
    owner_=owner;adventure_=adventure;descriptor_=next;registrationRevision_=revision;++generation_;
    auto saved=states_.value(key());
    for(int group=0;group<2;++group) {
        const auto name=QString::number(group);
        if(!faceIds(group).contains(saved[name].toString()))saved[name]=slot(group).faces.first().id;
    }
    states_.insert(key(),saved);
    return true;
}
bool ExperienceNavigation::current(const QString& owner,const QString& adventure,quint64 generation) const {
    return owner_==owner && adventure_==adventure && generation_==generation;
}
const ExperienceSlot& ExperienceNavigation::slot(int group) const {
    return group==0?descriptor_.first:descriptor_.second;
}
QString ExperienceNavigation::key() const {
    return QString::fromUtf8(QJsonDocument(QJsonArray{adventure_,descriptor_.id,descriptor_.version}).toJson(QJsonDocument::Compact));
}
QStringList ExperienceNavigation::labels(int group) const {
    QStringList result;for(const auto& face:slot(group).faces)result.append(face.label);return result;
}
QStringList ExperienceNavigation::faceIds(int group) const {
    QStringList result;for(const auto& face:slot(group).faces)result.append(face.id);return result;
}
QString ExperienceNavigation::face(int group) const {
    const auto saved=states_.value(key()).value(QString::number(group)).toString();
    return faceIds(group).contains(saved)?saved:slot(group).faces.first().id;
}
int ExperienceNavigation::index(int group) const {return faceIds(group).indexOf(face(group));}
QString ExperienceNavigation::view(int group) const {return slot(group).faces.at(index(group)).view;}
bool ExperienceNavigation::show(int group,const QString& faceId) {
    if(group<0 || group>1 || !faceIds(group).contains(faceId))return false;
    states_[key()].insert(QString::number(group),faceId);return true;
}
bool ExperienceNavigation::cycle(int group,int delta) {
    const auto ids=faceIds(group);return show(group,ids[(index(group)+delta%ids.size()+ids.size())%ids.size()]);
}
QJsonObject ExperienceNavigation::navigation() const {return states_.value(key())["navigation"].toObject();}
void ExperienceNavigation::rememberNavigation(const QJsonObject& navigation) {
    if(QJsonDocument(navigation).toJson(QJsonDocument::Compact).size()<=65536)
        states_[key()].insert("navigation",navigation);
}
QJsonObject ExperienceNavigation::state() const {
    QJsonObject entries;for(auto it=states_.cbegin();it!=states_.cend();++it)entries.insert(it.key(),it.value());
    return {{"version",1},{"owner",owner_},{"entries",entries}};
}
void ExperienceNavigation::restore(const QJsonObject& state) {
    states_.clear();++generation_;
    if(state["version"].toInt()!=1 || state["owner"].toString()!=owner_)return;
    const auto entries=state["entries"].toObject();
    // Bounded navigation state, never arbitrary per-module data or executable configuration.
    for(auto it=entries.begin();it!=entries.end() && states_.size()<4096;++it) {
        if(it.key().size()>2048 || !it.value().isObject())continue;
        const auto object=it.value().toObject();QJsonObject faces;
        for(const auto& name:{QString("0"),QString("1")}) {
            const auto value=object[name].toString();if(value.size()<=128)faces.insert(name,value);
        }
        const auto navigation=object["navigation"].toObject();
        if(QJsonDocument(navigation).toJson(QJsonDocument::Compact).size()<=65536)faces.insert("navigation",navigation);
        states_.insert(it.key(),faces);
    }
}
void ExperienceNavigation::clear() {states_.clear();++generation_;}
}
