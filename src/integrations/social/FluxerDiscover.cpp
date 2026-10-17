#include "FluxerSession.h"
#include <QJsonArray>
#include <QRegularExpression>
#include <QUrlQuery>

namespace trainer {
namespace {
bool discoveryId(const QString& id){return QRegularExpression("^[0-9]{1,20}$").match(id).hasMatch();}
QString artwork(const QJsonObject& value,const QString& field,const QString& folder) {
    const auto id=value["id"].toString(),hash=value[field].toString();
    if(!discoveryId(id)||!QRegularExpression("^[a-zA-Z0-9_]{1,128}$").match(hash).hasMatch())return {};
    return "https://fluxerusercontent.com/"+folder+"/"+id+"/"+hash+".png?size=512";
}
QVariantMap guildCard(const QJsonObject& g) {
    return {{"id",g["id"].toString()},{"name",g["name"].toString().left(100)},
        {"description",g["description"].toString().left(1000)}, {"members",g["member_count"].toInt(-1)},
        {"language",g["primary_language"].toString()}, {"tags",g["custom_tags"].toArray().toVariantList()},
        {"avatar",artwork(g,"icon","icons")},{"banner",artwork(g,"banner","banners")},
        {"kind","community"},{"action","Join"}};
}
}
void FluxerSession::search(QString mode,QString text,int offset) {
    if(mode!="people"&&mode!="communities"&&mode!="traineros"&&mode!="invite")return;
    const auto revision=++searchRevision_;searchMode_=mode;searchText_=text.trimmed().left(256);
    searchOffset_=qMax(0,offset);searchTotal_=0;searchResults_.clear();searching_=false;searchStatus_.clear();
    if(mode=="people") {
        const auto query=searchText_;
        QSet<QString> seen;
        auto add=[&](const QJsonObject& user){const auto id=user["id"].toString();
            if(!discoveryId(id)||id==self_||seen.contains(id)||relationships_.value(id)["type"]==2)return;
            const auto tag=user["username"].toString()+"#"+user["discriminator"].toString();
            if(!label(user).contains(query,Qt::CaseInsensitive)&&!tag.contains(query,Qt::CaseInsensitive)&&id!=query)return;
            seen.insert(id);searchResults_.append(QVariantMap{{"id",id},{"name",label(user)},{"tag",tag},{"avatar",artwork(user,"avatar","avatars")},
                {"kind","person"},{"action","Profile"},{"detail",relationships_.value(id)["type"]==1?"Friends":""}});
        };
        if(!query.isEmpty()) {
            for(const auto& relation:relationships_)if(relation["type"]!=2)add(relation["user"].toObject());
            for(const auto& c:channels_)if(c["type"]==1)for(const auto& p:c["recipients"].toArray())add(p.toObject());
        }
        if(discoveryId(query)&&!seen.contains(query)&&query!=self_&&relationships_.value(query)["type"]!=2) {
            searching_=true;searchStatus_="Looking for this person…";publish();
            request("GET","/v1/users/"+query,{},[this,revision,query](Reply r){
                if(revision!=searchRevision_)return;searching_=false;
                const auto user=r.body.object();
                if(r.status==200&&user["id"]==query)searchResults_.append(QVariantMap{{"id",query},{"name",label(user)},
                    {"avatar",artwork(user,"avatar","avatars")},{"kind","person"},{"action","Profile"}});
                searchTotal_=searchResults_.size();searchStatus_=searchTotal_?QString():"Person unavailable. Check the ID and try again.";publish();
            });return;
        }
        if(searchResults_.isEmpty()&&QRegularExpression("^[^\\s#]{1,32}#[0-9]{4}$").match(query).hasMatch()) {
            // The provider accepts a full tag for a request, but does not resolve
            // it into a public profile. Never fabricate an account/online result.
            searchResults_.append(QVariantMap{{"id",query},{"name",query},{"kind","request-tag"},{"action","Send request"},
                {"description","Send a friend request using their exact Fluxer tag."}});
        }
        searchTotal_=searchResults_.size();
        if(searchResults_.isEmpty())searchStatus_=query.isEmpty()?"Find someone you know, or enter a full tag or user ID.":"No known people match. Try their full tag or user ID.";
        publish();return;
    }
    QString path;
    if(mode=="invite") {
        QString code=searchText_;
        if(code.contains('/')) {
            const QUrl url(code.contains("://")?code:"https://"+code);
            if(!QStringList{"fluxer.gg","fluxer.app"}.contains(url.host().toLower())||!url.userInfo().isEmpty()||url.scheme()!="https") {
                searchStatus_="Use a Fluxer invitation link.";publish();return;
            }
            code=url.path().section('/',-1);
        }
        if(!QRegularExpression("^[A-Za-z0-9_-]{1,100}$").match(code).hasMatch()){searchStatus_="Check the invitation code.";publish();return;}
        path="/v1/invites/"+code;
    } else {
        QUrlQuery q;if(!searchText_.isEmpty())q.addQueryItem("query",searchText_.left(100));
        q.addQueryItem("tag","traineros-v1");q.addQueryItem("sort_by","member_count");
        q.addQueryItem("limit","24");q.addQueryItem("offset",QString::number(searchOffset_));
        path="/v1/discovery/guilds?"+q.toString(QUrl::FullyEncoded);
    }
    searching_=true;searchStatus_="Looking around…";publish();
    request("GET",path,{},[this,revision,mode](Reply r){
        if(revision!=searchRevision_)return;searching_=false;
        if(r.status!=200){searchStatus_=r.status==404&&mode=="invite"?"Invite not found or expired":r.status==429?"Please wait a moment, then retry.":"Search is unavailable. Try again.";if(r.status==401)fail(r,searchStatus_);else publish();return;}
        const auto b=r.body.object();
        if(mode=="invite") {
            const auto g=b["guild"].toObject(),c=b["channel"].toObject();
            auto card=guildCard(g);const auto code=b["code"].toString();
            if(!QRegularExpression("^[A-Za-z0-9_-]{1,100}$").match(code).hasMatch()){searchStatus_="Invitation unavailable.";publish();return;}
            card["id"]=code;card["guild"]=g["id"].toString();card["channel"]=c["id"].toString();card["kind"]="invite";
            card["name"]=g.isEmpty()?c["name"].toString("Group conversation"):g["name"].toString();
            card["members"]=b["member_count"].toInt(-1);card["detail"]=g.isEmpty()?"Group invitation":"Community invitation";
            if((!g.isEmpty()&&guilds_.contains(g["id"].toString()))||(g.isEmpty()&&channels_.contains(c["id"].toString())))card["action"]="Joined";
            searchResults_.append(card);searchTotal_=1;
        } else {
            if(!b["guilds"].isArray()){searchStatus_="Community discovery is unavailable.";publish();return;}
            searchTotal_=b["total"].toInt();
            for(const auto& v:b["guilds"].toArray()) {
                const auto g=v.toObject();bool marked=false;
                for(const auto& tag:g["custom_tags"].toArray())if(tag.toString()=="traineros-v1")marked=true;
                if(!marked||!discoveryId(g["id"].toString()))continue;
                auto card=guildCard(g);card["action"]=guilds_.contains(g["id"].toString())?"Joined":"Join";searchResults_.append(card);
            }
        }
        if(mode!="invite"&&searchResults_.isEmpty()&&searchTotal_==0&&QRegularExpression("^[A-Za-z0-9_-]{1,100}$").match(searchText_).hasMatch()) {
            // A bare invitation code shares the search field. Lookup is read-only;
            // even a valid invite still opens a preview before explicit Join.
            search("invite",searchText_);return;
        }
        searchStatus_=searchResults_.isEmpty()?"No public TrainerOS communities yet. You can still join with an invitation.":QString();publish();
    });
}

void FluxerSession::communityMembership(QString operation,QVariantMap args) {
    const auto id=args["id"].toString();if(mutationBusy_)return;
    if(!discoveryId(id)||!guilds_.contains(id)) {
        communityStatus_="This community is no longer available.";publish();emit mutationFinished(generation_,operation,{},id,false);return;
    }
    mutationBusy_=true;communityStatus_=operation=="delete-community"?"Deleting community…":"Leaving community…";publish();
    // Re-read authority; UI visibility or an old cache is not permission.
    request("GET","/v1/guilds/"+id,{},[this,id,operation,args](Reply r){
        const auto g=r.body.object();const bool owner=g["owner_id"]==self_,deleting=operation=="delete-community";
        if(r.status!=200||g["id"]!=id||owner!=deleting) {
            mutationBusy_=false;communityStatus_=owner?"Transfer ownership in Fluxer before leaving, or delete your community.":"This action is unavailable. Check your community permissions.";
            publish();emit mutationFinished(generation_,operation,{},id,false);return;
        }
        QJsonObject body;
        if(deleting) {
            if(args["method"]=="password")body["password"]=args["secret"].toString();
            else if(args["method"]=="totp"){body["mfa_method"]="totp";body["mfa_code"]=args["secret"].toString();}
        }
        verifiedRequest(deleting?"POST":"DELETE",deleting?"/v1/guilds/"+id+"/delete":"/v1/users/@me/guilds/"+id,body,[this,id,operation](Reply reply){
            mutationBusy_=false;
            if(reply.status<200||reply.status>=300) {
                const auto error=reply.body.object();
                if(operation=="delete-community"&&error["code"]=="SUDO_MODE_REQUIRED") {
                    const auto methods=error["methods"].toObject();
                    const auto method=!error["has_mfa"].toBool()?QString("password"):methods["totp"].toBool()||methods["backup_codes"].toBool()?QString("totp"):QString();
                    communityStatus_=method.isEmpty()?"Confirm deletion in Fluxer with your security key.":"Confirm your identity to delete this community.";
                    publish();if(!method.isEmpty())emit communityVerificationRequired(generation_,id,method);
                    else emit mutationFinished(generation_,operation,{},id,false);
                    return;
                }
                communityStatus_="Could not complete the action. Your community has not been removed from this list.";
                publish();emit mutationFinished(generation_,operation,{},id,false);return;
            }
            ++guildListRevision_;guilds_.remove(id);communityMarked_.remove(id);communityChecked_.remove(id);communityChannels_.remove(id);++communityRevision_[id];
            for(auto it=channels_.begin();it!=channels_.end();)if(it.value()["guild_id"]==id){
                const auto channel=it.key();if(voiceChannel_==channel)leaveVoice();
                historyCache_.remove(channel);historyLru_.removeAll(channel);unread_.remove(channel);mentions_.remove(channel);
                if(channel_==channel){channel_.clear();messages_.clear();messageOrder_.clear();}it=channels_.erase(it);
            }else ++it;
            if(guild_==id){guild_.clear();communityInvite_.clear();remember("guild",{});}
            ++channelRevision_;communityStatus_.clear();ensureConversation();publish();emit mutationFinished(generation_,operation,{},id,true);
        });
    });
}
}
