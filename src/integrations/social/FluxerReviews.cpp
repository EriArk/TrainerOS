#include "FluxerSession.h"
#include "AdventureReviews.h"
#include <QFile>
#include <QSettings>
#include <QJsonArray>
#include <QDateTime>
#include <QUuid>
#include <algorithm>

namespace trainer {
namespace {
QString cacheKey(const QString& user,const QString& channel,const QString& identity) {
    return "reviews/"+user+"/"+channel+"/"+identity;
}
bool validId(const QString& id) {static const QRegularExpression re("^[1-9][0-9]{0,19}$");return re.match(id).hasMatch();}
bool newerReview(const QVariantMap& a,const QVariantMap& b) {
    const auto x=a["id"].toString(),y=b["id"].toString();return x.size()==y.size()?x>y:x.size()>y.size();
}
}
void FluxerSession::publishReviews() {
    emit reviewsChanged(generation_,reviewIdentity_,{{"rows",reviewRows_},{"mine",ownReview_},
        {"busy",reviewBusy_},{"more",reviewMore_},{"fresh",reviewFresh_},{"status",reviewStatus_},
        {"signedIn",!self_.isEmpty()},{"user",self_}});
}
void FluxerSession::loadReviews(int page) {
    if(reviewBusy_)return;
    if(self_.isEmpty()||state_!="connected") {reviewFresh_=false;reviewStatus_="Sign in to Fluxer to read shared reviews.";publishReviews();return;}
    if(!validId(reviewChannel_)){reviewStatus_="The review service has not been configured.";publishReviews();return;}
    reviewBusy_=true;reviewFresh_=false;reviewStatus_="Loading reviews…";publishReviews();
    const auto revision=reviewRevision_;const auto identity=reviewIdentity_;
    QJsonObject query{{"scope","current"},{"context_channel_id",reviewChannel_},{"content",identity},
        {"sort_by","timestamp"},{"sort_order","desc"},{"page",page},{"hits_per_page",25}};
    request("POST","/v1/search/messages",query,[this,revision,page,query](Reply reply) mutable {
        if(revision!=reviewRevision_)return;
        if(reply.status!=200||reply.body.object()["indexing"].toBool()) {
            reviewBusy_=false;reviewStatus_=reply.body.object()["indexing"].toBool()?"Reviews are being indexed. Refresh in a moment.":reviewRows_.isEmpty()?"Couldn't load reviews. Try Refresh.":"Reviews unavailable · saved copy shown";publishReviews();return;
        }
        if(page==1)reviewRows_.clear();
        for(const auto& value:reply.body.object()["messages"].toArray()) {
            const auto message=value.toObject();if(message["channel_id"]!=reviewChannel_)continue;
            const auto row=reviews::decode(message,reviewIdentity_).toVariantMap();if(row.isEmpty())continue;
            auto found=std::find_if(reviewRows_.begin(),reviewRows_.end(),[&](const QVariant& v){return v.toMap()["author"]==row["author"];});
            if(found==reviewRows_.end())reviewRows_.append(row);else if(newerReview(row,found->toMap()))*found=row;
        }
        reviewPage_=page;reviewMore_=page*25<reply.body.object()["total"].toInt()&&page<40;
        query["author_id"]=QJsonArray{self_};query["page"]=1;
        request("POST","/v1/search/messages",query,[this,revision](Reply own) {
            if(revision!=reviewRevision_)return;
            reviewBusy_=false;
            if(own.status!=200||own.body.object()["indexing"].toBool()){reviewStatus_="Couldn't check your review. Refresh before posting.";publishReviews();return;}
            ownReview_.clear();
            for(const auto& value:own.body.object()["messages"].toArray()) {
                const auto message=value.toObject();const auto data=reviews::decode(message,reviewIdentity_).toVariantMap();
                if(message["channel_id"]==reviewChannel_&&data["author"]==self_&&(ownReview_.isEmpty()||newerReview(data,ownReview_)))ownReview_=data;
            }
            // A recent own post may not have reached the search index yet.
            QSettings settings;const auto key=cacheKey(self_,reviewChannel_,reviewIdentity_);
            auto known=settings.value(key+"/own").toString();
            if(!ownReview_.isEmpty()&&(known.isEmpty()||newerReview(ownReview_,QVariantMap{{"id",known}})))known=ownReview_["id"].toString();
            auto finish=[this,key](const QVariantMap& own){
                ownReview_=own;
                reviewRows_.removeIf([this](const QVariant& v){return v.toMap()["author"]==self_;});
                if(!ownReview_.isEmpty())reviewRows_.prepend(ownReview_);
                reviewFresh_=true;reviewStatus_=reviewRows_.isEmpty()?"No reviews yet.":QString();
                QSettings settings;settings.setValue(key+"/cache",QJsonDocument(QJsonArray::fromVariantList(reviewRows_)).toJson(QJsonDocument::Compact));
                publishReviews();
            };
            if(validId(known)) {
                reviewBusy_=true;
                request("GET","/v1/channels/"+reviewChannel_+"/messages/"+known,{},[this,revision,key,finish](Reply r){
                    if(revision!=reviewRevision_)return;reviewBusy_=false;
                    const auto row=reviews::decode(r.body.object(),reviewIdentity_).toVariantMap();
                    if(r.status==200&&row["author"]==self_)finish(row);
                    else if(r.status==404){QSettings().remove(key+"/own");finish({});}
                    else {reviewStatus_="Couldn't check your last review. Refresh before posting.";publishReviews();}
                });
            } else finish({});
        });
    });
}
void FluxerSession::reviewCommand(const QString& operation,const QVariantMap& args) {
    const auto identity=args["identity"].toString();
    if(operation=="reviews-close"){++reviewRevision_;reviewBusy_=false;reviewIdentity_.clear();return;}
    if(!reviews::validIdentity(identity))return;
    if(operation=="reviews-open") {
        ++reviewRevision_;reviewIdentity_=identity;reviewRows_.clear();ownReview_.clear();reviewBusy_=reviewMore_=reviewFresh_=false;reviewPage_=1;
        QFile config("/var/opt/traineros/integrations/reviews.json");reviewChannel_.clear();
        if(config.size()<8192&&config.open(QIODevice::ReadOnly))reviewChannel_=QJsonDocument::fromJson(config.readAll()).object()["channel"].toString();
        if(transport_)reviewChannel_=args["testChannel"].toString();
        QSettings settings;const auto data=settings.value(cacheKey(self_,reviewChannel_,identity)+"/cache").toByteArray();
        if(data.size()<1024*1024)reviewRows_=QJsonDocument::fromJson(data).array().toVariantList();
        loadReviews();return;
    }
    if(identity!=reviewIdentity_||reviewBusy_)return;
    if(operation=="reviews-refresh"){loadReviews();return;}
    if(operation=="reviews-more"){if(reviewMore_&&reviewRows_.size()<1000)loadReviews(reviewPage_+1);return;}
    if(self_.isEmpty()||state_!="connected"||!reviewFresh_)return;
    const auto revision=reviewRevision_;QByteArray method;QString path;QJsonObject body;
    if(operation=="reviews-save") {
        // Eligibility is issued by the freshly read exact adapter through the
        // library controller, not by network data, time or the review itself.
        if(!args["completed"].toBool()||args["policy"]!="emerald-en/champion-v1"
            ||identity!="a9dec84dfe7f62ab2220bafaef7479da0929d066ece16a6885f6226db19085af")return;
        const auto content=reviews::encode(identity,args["text"].toString(),args["spoiler"].toBool(),args["policy"].toString());if(content.isEmpty())return;
        const auto own=ownReview_["id"].toString();method=own.isEmpty()?"POST":"PATCH";
        path="/v1/channels/"+reviewChannel_+"/messages"+(own.isEmpty()?QString():"/"+own);
        body={{"content",content},{"allowed_mentions",QJsonObject{{"parse",QJsonArray{}}}}};
        if(own.isEmpty())body["nonce"]=QUuid::createUuid().toString(QUuid::Id128);
    } else if(operation=="reviews-delete") {
        if(!validId(ownReview_["id"].toString()))return;
        method="DELETE";path="/v1/channels/"+reviewChannel_+"/messages/"+ownReview_["id"].toString();
    } else if(operation=="reviews-report") {
        const auto id=args["id"].toString();const auto category=args["category"].toString();
        if(category!="spam"&&category!="harassment"&&category!="other")return;
        bool exists=false;for(const auto& v:reviewRows_)if(v.toMap()["id"]==id&&v.toMap()["author"]!=self_)exists=true;
        if(!exists)return;method="POST";path="/v1/reports/message";body={{"channel_id",reviewChannel_},{"message_id",id},{"category",category}};
    } else return;
    reviewBusy_=true;reviewStatus_="Sending…";publishReviews();
    verifiedRequest(method,path,body,[this,revision,operation](Reply reply){
        if(revision!=reviewRevision_)return;reviewBusy_=false;
        if(reply.status<200||reply.status>=300){reviewFresh_=false;reviewStatus_=reply.status==0||reply.status>=500?"Delivery unknown. Refresh before trying again.":"Not sent. Refresh and try again.";publishReviews();return;}
        const auto key=cacheKey(self_,reviewChannel_,reviewIdentity_);QSettings settings;
        if(operation=="reviews-save") {
            ownReview_=reviews::decode(reply.body.object(),reviewIdentity_).toVariantMap();
            if(ownReview_.isEmpty()||ownReview_["author"]!=self_){reviewFresh_=false;reviewStatus_="Delivery unknown. Refresh before trying again.";publishReviews();return;}
            settings.setValue(key+"/own",ownReview_["id"]);
            reviewRows_.removeIf([this](const QVariant& v){return v.toMap()["author"]==self_;});reviewRows_.prepend(ownReview_);
            reviewStatus_="Review published";
        } else if(operation=="reviews-delete") {
            settings.remove(key+"/own");ownReview_.clear();reviewRows_.removeIf([this](const QVariant& v){return v.toMap()["author"]==self_;});reviewStatus_="Review deleted";
        } else reviewStatus_="Report sent";
        settings.setValue(key+"/cache",QJsonDocument(QJsonArray::fromVariantList(reviewRows_)).toJson(QJsonDocument::Compact));publishReviews();
    });
}
}
