#include "FluxerSession.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QJsonArray>
#include <QDir>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QDateTime>
#include <QRegularExpression>
#include <QUuid>
#include <QSettings>
#include <QCryptographicHash>
#include <memory>
namespace trainer {
namespace {
bool snowflake(const QString& id){static const QRegularExpression re("^[0-9]{1,20}$");return re.match(id).hasMatch();}
QString errorText(const QJsonObject& body,int status){
    const auto code=body["code"].toVariant().toString();
    return status==0?QString("Delivery unknown. Retry within 4 minutes or check the room."):
        QString("Fluxer refused the file (%1). %2").arg(status).arg(body["message"].toString(code).left(220));
}
}
void FluxerSession::resetFiles(){
    if(fileUpload_)fileUpload_->abort();fileUpload_=nullptr;
    const auto replies=fileReplies_;fileReplies_.clear();for(auto reply:replies)if(reply)reply->abort();
    fileJobs_.clear();fileStoreKey_.clear();
}
void FluxerSession::sendFiles(const QVariantMap& args){
    const auto channel=args["channel"].toString(),nonce=args["nonce"].toString();
    const auto files=args["files"].toList();const auto epoch=epoch_;
    auto finish=[this,epoch,channel,nonce](Reply result){
        if(epoch!=epoch_)return;fileUpload_=nullptr;
        const bool ok=result.status>=200&&result.status<300;
        if(ok&&channel==channel_)mergeMessage(result.body.object());
        if(result.status==429)blockedUntil_=QDateTime::currentMSecsSinceEpoch()+qMax(1,result.retrySeconds)*1000LL;
        emit fileEvent(generation_,{{"nonce",nonce},{"finished",true},{"ok",ok},{"uncertain",result.status==0},
            {"progress",ok?1.:0.},{"status",ok?QString():errorText(result.body.object(),result.status)}});publish();
    };
    if(fileUpload_||self_.isEmpty()||!snowflake(channel)||channel!=channel_||!channels_.contains(channel)||files.isEmpty()
        ||nonce.isEmpty()||nonce.size()>32){finish({409,{}});return;}
    if(QDateTime::currentMSecsSinceEpoch()<blockedUntil_){finish({429,{}});return;}
    // Multipart is an official Fluxer route. Fluxer resolves the caller/channel
    // size and count limits; no second size ceiling is imposed by TrainerOS.
    auto* multipart=new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QJsonArray attachments;int index=0;
    for(const auto& value:files){
        const auto row=value.toMap();const QFileInfo info(row["path"].toString());
        auto* file=new QFile(info.absoluteFilePath(),multipart);
        if(!info.isFile()||info.isSymLink()||info.size()!=row["size"].toLongLong()
            ||info.lastModified().toMSecsSinceEpoch()!=row["modified"].toLongLong()||!file->open(QIODevice::ReadOnly)){
            delete multipart;finish({400,QJsonDocument(QJsonObject{{"code","FILE_CHANGED_OR_UNAVAILABLE"}})});return;
        }
        auto headerName=info.fileName();headerName.replace('"','_').replace('\r','_').replace('\n','_').replace('\\','_');
        const auto mime=QMimeDatabase().mimeTypeForFile(info,QMimeDatabase::MatchExtension).name();
        attachments.append(QJsonObject{{"id",index},{"filename",info.fileName()},{"content_type",mime}});
        QHttpPart part;part.setHeader(QNetworkRequest::ContentDispositionHeader,QString("form-data; name=\"files[%1]\"; filename=\"%2\"").arg(index++).arg(headerName));
        part.setHeader(QNetworkRequest::ContentTypeHeader,mime);part.setBodyDevice(file);multipart->append(part);
    }
    const QJsonObject payload{{"content",args["text"].toString()},{"nonce",nonce},{"attachments",attachments},
        {"allowed_mentions",QJsonObject{{"parse",QJsonArray{}}}}};
    if(transport_){delete multipart;transport_("POST","/v1/channels/"+channel+"/messages",payload,finish,{});return;}
    QHttpPart json;json.setHeader(QNetworkRequest::ContentDispositionHeader,"form-data; name=\"payload_json\"");
    json.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");json.setBody(QJsonDocument(payload).toJson(QJsonDocument::Compact));multipart->append(json);
    if(!network_)network_=new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl("https://api.fluxer.app/v1/channels/"+channel+"/messages"));
    request.setRawHeader("Authorization",token_.toUtf8());request.setRawHeader("User-Agent","TrainerOS/0.1");
    request.setTransferTimeout(60000);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    auto* reply=network_->post(request,multipart);multipart->setParent(reply);fileUpload_=reply;
    reply->setReadBufferSize(1024*1024);auto response=std::make_shared<QByteArray>();
    connect(reply,&QNetworkReply::uploadProgress,this,[this,epoch,nonce](qint64 sent,qint64 total){if(epoch==epoch_)emit fileEvent(generation_,{{"nonce",nonce},{"status",sent==total?"Confirming…":"Sending…"},{"progress",total>0?double(sent)/total:0.}});});
    connect(reply,&QIODevice::readyRead,reply,[reply,response]{response->append(reply->readAll());if(response->size()>1024*1024)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[reply,response,finish]{
        response->append(reply->readAll());int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(reply->error()!=QNetworkReply::NoError&&status<400)status=0;
        finish({status,QJsonDocument::fromJson(*response),reply->rawHeader("Retry-After").toInt()});reply->deleteLater();
    });
}
void FluxerSession::fileCommand(QString operation,const QVariantMap& args){
    if(self_.isEmpty())return;
    const auto key="social/files/"+QString::fromLatin1(QCryptographicHash::hash((owner_+"\n"+self_).toUtf8(),QCryptographicHash::Sha256).toHex());
    if(fileStoreKey_!=key){
        fileStoreKey_=key;
        if(!transport_)for(const auto& value:QSettings().value(key).toList()){
            auto row=value.toMap();const auto id=row["id"].toString();if(id.isEmpty())continue;
            if(row["state"]=="running"||row["state"]=="queued"){row["state"]="error";row["detail"]="Interrupted. Retry to continue.";}
            fileJobs_[id]=row;
        }
    }
    if(operation=="files-sync"){publishFiles();return;}
    if(operation=="files-send"){sendFiles(args);return;}
    if(operation=="files-cancel"){if(fileUpload_)fileUpload_->abort();return;}
    if(operation=="files-download"){
        const auto channel=args["channel"].toString(),message=args["message"].toString(),attachment=args["attachment"].toString();
        if(self_.isEmpty()||!channels_.contains(channel)||!snowflake(message)||!snowflake(attachment))return;
        const auto id=channel+":"+message+":"+attachment;
        if(fileJobs_.contains(id)){publishFiles();return;}
        fileJobs_[id]={{"id",id},{"channel",channel},{"message",message},{"attachment",attachment},{"title","Attachment"},{"state","queued"}};
        downloadFile(id);return;
    }
    if(operation!="files-control")return;
    const auto action=args["action"].toString(),id=args["id"].toString();
    const auto ids=id.isEmpty()?fileJobs_.keys():QStringList{id};
    for(const auto& key:ids){if(!fileJobs_.contains(key))continue;auto& job=fileJobs_[key];
        if((action=="retry"||action=="resume-all")&&job["state"]=="error")downloadFile(key);
        if((action=="cancel"||action=="cancel-all")&&(job["state"]=="running"||job["state"]=="queued")){
            job["state"]="cancelled";if(fileReplies_.value(key))fileReplies_[key]->abort();
        }
    }publishFiles();
}
void FluxerSession::publishFiles(){
    QVariantList rows;bool running=false;for(auto job:fileJobs_){
        job["source"]="Lobby files";
        const auto state=job["state"].toString();const bool done=state=="done",error=state=="error";
        running|=state=="running";
        job["terminal"]=done||error||state=="cancelled";
        job["actions"]=done?QVariantList{QVariantMap{{"id","open"},{"label","Show in folder"}}}:
            error?QVariantList{QVariantMap{{"id","retry"},{"label","Retry"}}}:
            state=="cancelled"?QVariantList():QVariantList{QVariantMap{{"id","cancel"},{"label","Cancel"}}};
        rows.append(job);
    }
    if(!transport_&&!fileStoreKey_.isEmpty()&&!running)QSettings().setValue(fileStoreKey_,rows);
    emit fileEvent(generation_,{{"kind","downloads"},{"rows",rows}});
}
void FluxerSession::downloadFile(QString task){
    if(!fileJobs_.contains(task)||fileReplies_.value(task))return;
    auto& job=fileJobs_[task];job["state"]="queued";job["detail"]="Checking attachment…";publishFiles();
    const auto epoch=epoch_;const auto channel=job["channel"].toString(),message=job["message"].toString(),attachment=job["attachment"].toString();
    // Re-authorize exact message on every attempt. Never download a URL supplied
    // by QML or arbitrary message text; refreshed URLs are not authorization.
    request("GET","/v1/channels/"+channel+"/messages/"+message,{},[this,epoch,task,attachment](Reply result){
        if(epoch!=epoch_||!fileJobs_.contains(task)||fileJobs_[task]["state"]!="queued")return;
        auto fail=[this,task](QString why){auto& j=fileJobs_[task];j["state"]="error";j["detail"]=why;publishFiles();};
        if(result.status!=200){fail("Attachment unavailable. Check your connection and room access.");return;}
        QJsonObject file;for(const auto& a:result.body.object()["attachments"].toArray())if(a.toObject()["id"]==attachment)file=a.toObject();
        const QUrl url(file["url"].toString());const auto size=file["size"].toInteger(-1);
        if(file.isEmpty()||size<0||url.scheme()!="https"||!url.userInfo().isEmpty()||url.port(443)!=443
            ||!QStringList{"media.fluxer.app","fluxerusercontent.com"}.contains(url.host())
            ||!url.path().startsWith("/attachments/")){fail("Attachment address is unavailable.");return;}
        auto name=file["filename"].toString();name.replace('\\','/');name=QFileInfo(name).fileName();
        name.remove(QRegularExpression("[\\x00-\\x1f]"));if(name.isEmpty()||name=="."||name=="..")name="attachment";
        const auto folder=QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)+"/TrainerOS/"+QUuid::createUuid().toString(QUuid::Id128);
        if(!QDir().mkpath(folder)){fail("Couldn't create the download folder.");return;}
        const QStorageInfo storage(folder);if(storage.isValid()&&storage.bytesAvailable()<size){QDir().rmdir(folder);fail("Not enough free space.");return;}
        auto output=std::make_shared<QSaveFile>(folder+"/"+name);output->setDirectWriteFallback(false);
        if(!output->open(QIODevice::WriteOnly)){QDir().rmdir(folder);fail("Couldn't save this file.");return;}
        if(!network_)network_=new QNetworkAccessManager(this);
        QNetworkRequest request(url);request.setTransferTimeout(60000);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
        // Signed CDN URLs need no account token, including on redirects.
        auto* reply=network_->get(request);reply->setReadBufferSize(256*1024);fileReplies_[task]=reply;
        auto bytes=std::make_shared<qint64>(0);auto failed=std::make_shared<bool>(false);
        auto& job=fileJobs_[task];job["state"]="running";job["title"]=name;job["detail"]="Downloading";job["progress"]=0.;publishFiles();
        connect(reply,&QIODevice::readyRead,this,[this,epoch,task,reply,output,bytes,failed,size]{
            const auto data=reply->readAll();*bytes+=data.size();
            if(epoch!=epoch_||*bytes>size||output->write(data)!=data.size()){*failed=true;reply->abort();return;}
            if(fileJobs_.contains(task)){fileJobs_[task]["progress"]=size?double(*bytes)/size:0.;publishFiles();}
        });
        connect(reply,&QNetworkReply::finished,this,[this,epoch,task,reply,output,bytes,failed,size,folder]{
            const bool ok=!*failed&&reply->error()==QNetworkReply::NoError&&reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==200&&*bytes==size;
            reply->deleteLater();
            if(epoch!=epoch_){output->cancelWriting();QDir().rmdir(folder);return;}
            fileReplies_.remove(task);auto& job=fileJobs_[task];
            if(ok&&job["state"]!="cancelled"&&output->commit()){job["state"]="done";job["progress"]=1.;job["folder"]=folder;job["detail"]="Saved in Downloads";}
            else {output->cancelWriting();QDir().rmdir(folder);if(job["state"]!="cancelled"){job["state"]="error";job["detail"]="Download interrupted. Retry to refresh the attachment.";}}
            publishFiles();
        });
    });
}
}
