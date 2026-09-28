#include "BattlePlayback.h"
#include <QHash>

namespace trainer {
void BattlePlayback::reset(){frames_.clear();final_={};index_=-1;turn_=0;}
QJsonArray BattlePlayback::sides() const{return active()?frames_[index_].sides:final_;}
QVariantMap BattlePlayback::event() const{return active()?frames_[index_].event:QVariantMap{};}
bool BattlePlayback::advance(){if(active())++index_;return active();}
void BattlePlayback::load(const QJsonObject& before,const QJsonObject& after,const QJsonArray& teams) {
    reset();final_=after["sides"].toArray();
    auto shown=before["sides"].toArray();
    turn_=before["turn"].toInt(after["turn"].toInt());
    if(shown.size()!=2 || final_.size()!=2)return; // Initial entry has no previous turn.
    QString element="Normal";
    const auto lines=after["events"].toArray();
    for(int line=0;line<lines.size();++line) {
        auto value=lines[line].toString();
        if(value.startsWith("|split|") && line+2<lines.size()) {value=lines[++line].toString();++line;}
        const auto p=value.split('|');if(p.size()<3)continue;
        const auto kind=p[1];const int actor=p[2].startsWith("p2")?1:0;
        const bool identified=p[2].startsWith("p1") || p[2].startsWith("p2");
        auto side=shown[actor].toObject();int member=side["member"].toInt();
        for(const auto& part:p)if(part.startsWith("[traineros-member] "))member=part.mid(19).toInt();
        const auto team=actor<teams.size()?teams[actor].toArray():QJsonArray{};
        const auto named=member>=0 && member<team.size()?team[member].toObject():QJsonObject{};
        const auto name=named["name"].toString(p[2].section(": ",1));
        QString text,effect;QJsonObject update;
        if(kind=="move" && p.size()>3) {
            element="Normal";for(const auto& part:p)if(part.startsWith("[traineros-type] "))element=part.mid(17);
            text=name+" used "+p[3]+"!";effect="move";
        } else if((kind=="switch" || kind=="drag" || kind=="replace") && p.size()>4) {
            update["member"]=member;side["member"]=member;
            text="Go, "+name+"!";effect="switch";
        } else if((kind=="-damage" || kind=="-heal") && p.size()>3) {
            effect=kind=="-damage"?"hit":"heal";
            text=name+(kind=="-damage"?" took damage!":" recovered HP!");
        } else if(kind=="faint") {text=name+" fainted!";effect="faint";update["hp"]=0;}
        else if(kind=="-status" && p.size()>3) {
            update["status"]=p[3];effect="status";
            const QHash<QString,QString> labels{{"brn","was burned"},{"par","is paralyzed"},{"slp","fell asleep"},{"psn","was poisoned"},{"tox","was badly poisoned"},{"frz","was frozen"}};
            text=name+" "+labels.value(p[3],"has a status condition")+"!";
        } else if(kind=="-curestatus") {update["status"]="";effect="heal";text=name+" recovered!";}
        else if(kind=="-supereffective")text="It's super effective!";
        else if(kind=="-resisted")text="It's not very effective...";
        else if(kind=="-crit")text="A critical hit!";
        else if(kind=="-miss") {text="The attack missed!";effect="miss";}
        else if(kind=="-immune") {text="It had no effect!";effect="immune";}
        else if(kind=="-fail")text="But it failed!";
        else if(kind=="cant")text=name+" couldn't move!";
        else if(kind=="-boost" || kind=="-unboost") {
            if(p.size()>3){const QHash<QString,QString> labels{{"atk","Attack"},{"def","Defense"},{"spa","Sp. Atk"},{"spd","Sp. Def"},{"spe","Speed"},{"accuracy","Accuracy"},{"evasion","Evasion"}};
                text=name+"'s "+labels.value(p[3],p[3])+(kind=="-boost"?" rose!":" fell!");effect="status";}
        } else if(kind=="message")text=p[2];
        else if(kind=="-transform") {text=name+" transformed!";effect="status";}
        if(text.isEmpty())continue;
        if(effect=="hit" || effect=="heal" && kind=="-heal" || effect=="switch") {
            const auto hp=p[effect=="switch"?4:3].split(' ');bool ok=false;
            const int amount=hp[0].section('/',0,0).toInt(&ok);if(ok)update["hp"]=amount;
            const int maximum=hp[0].section('/',1,1).toInt(&ok);if(ok && maximum>0)update["maxHp"]=maximum;
            update["status"]=hp.size()>1 && hp[1]!="fnt"?hp[1]:QString();
        }
        if(identified && !update.isEmpty()) {
            auto roster=side["team"].toArray();
            for(int i=0;i<roster.size();++i) {
                auto mon=roster[i].toObject();
                if(effect=="switch")mon["active"]=mon["member"].toInt()==member;
                if(mon["member"].toInt()==member){for(auto it=update.begin();it!=update.end();++it)mon[it.key()]=it.value();if(update.contains("hp"))mon["fainted"]=update["hp"].toInt()==0;}
                roster[i]=mon;
            }
            side["team"]=roster;
            if(member==side["member"].toInt())for(auto it=update.begin();it!=update.end();++it)side[it.key()]=it.value();
            shown[actor]=side;
        }
        const bool onField=identified && member==side["member"].toInt();
        frames_.append({{{"text",text},{"kind",kind},{"actor",actor},{"effect",onField?effect:QString()},
            {"element",element},{"serial",++serial_},{"duration",effect=="move"?950:effect=="hit"?750:850}},shown});
    }
    if(!frames_.isEmpty())index_=0;
}
}
