#include "SocialMedia.h"
#include <QAudioSource>
#include <QAudioSink>
#include <QAudioOutput>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QImageWriter>
#include <QPainter>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QStandardPaths>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QtEndian>
#include <cmath>
namespace trainer {
namespace {
constexpr int Rate=24000, MaxBytes=16*1024*1024;
QByteArray pictureCopy(QByteArray data) {
    QBuffer input(&data);input.open(QIODevice::ReadOnly);QImageReader reader(&input);
    const auto format=reader.format();const auto size=reader.size();
    if(!QList<QByteArray>{"jpeg","png","webp"}.contains(format)||size.isEmpty()
        ||qint64(size.width())*size.height()>24000000)return {};
    reader.setAutoTransform(true);reader.setScaledSize(size.scaled(1600,1600,Qt::KeepAspectRatio));
    auto image=reader.read();if(image.isNull())return {};
    // A fresh image discards EXIF, comments, location and source filename.
    QImage clean(image.size(),QImage::Format_RGB32);clean.fill(Qt::white);
    { QPainter painter(&clean); painter.drawImage(0,0,image); }
    QByteArray result;QBuffer output(&result);output.open(QIODevice::WriteOnly);
    QImageWriter writer(&output,"jpeg");writer.setQuality(86);if(!writer.write(clean))return {};
    return result.size()<=MaxBytes?result:QByteArray();
}
}
SocialMedia::SocialMedia(QObject* parent):QObject(parent) {
    network_=new QNetworkAccessManager(this);
    limit_.setSingleShot(true);limit_.setInterval(120000);
    connect(&limit_,&QTimer::timeout,this,&SocialMedia::stopRecording);
}
SocialMedia::~SocialMedia(){clear();}
bool SocialMedia::playing() const{return player_&&player_->playbackState()==QMediaPlayer::PlayingState;}
double SocialMedia::progress() const{return player_&&player_->duration()>0?double(player_->position())/player_->duration():0.;}
QVariantList SocialMedia::levels() const{QVariantList result;for(auto byte:waveform_)result.append(int(quint8(byte)));return result;}
void SocialMedia::ensurePlayer(){
    if(player_)return;
    output_=new QAudioOutput(this);player_=new QMediaPlayer(this);player_->setAudioOutput(output_);
    connect(player_,&QMediaPlayer::playbackStateChanged,this,&SocialMedia::changed);
    connect(player_,&QMediaPlayer::positionChanged,this,&SocialMedia::changed);
    connect(player_,&QMediaPlayer::errorOccurred,this,[this]{fail("Couldn't play this recording.");});
}
void SocialMedia::clear() {
    ++generation_;limit_.stop();
    if(input_){input_->disconnect(this);input_->stop();delete input_;input_=nullptr;inputStream_=nullptr;}
    if(download_){download_->disconnect(this);download_->abort();download_->deleteLater();download_=nullptr;}
    if(encoder_){encoder_->disconnect(this);encoder_->kill();encoder_->deleteLater();encoder_=nullptr;}
    if(player_){player_->stop();player_->setSource({});}
    temporary_.reset();state_.clear();error_.clear();picture_=QUrl();pcm_.clear();prepared_.clear();waveform_.clear();seconds_=0;voice_=false;emit changed();
}
void SocialMedia::fail(QString message){state_="error";error_=std::move(message);emit changed();}
QVariantList SocialMedia::pictures() const {
    QVariantList result;
    const QDir folder(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
    // Deliberately bounded, flat picker. No whole-library crawl or implicit upload.
    for(const auto& file:folder.entryInfoList({"*.png","*.jpg","*.jpeg","*.webp"},QDir::Files|QDir::NoSymLinks,QDir::Time)) {
        if(file.size()>0&&file.size()<=MaxBytes)result.append(QVariantMap{{"name",file.completeBaseName()},{"path",file.absoluteFilePath()}});
        if(result.size()==80)break;
    }
    return result;
}
void SocialMedia::choosePicture(const QString& path) {
    clear();bool allowed=false;for(const auto& item:pictures())if(item.toMap()["path"]==path)allowed=true;
    if(!allowed){fail("This picture is no longer available.");return;}
    QFile file(path);if(!file.open(QIODevice::ReadOnly)||file.size()>MaxBytes){fail("Couldn't open this picture.");return;}
    preparePicture(file.read(MaxBytes+1));
}
void SocialMedia::preparePicture(QByteArray bytes) {
    state_="preparing";emit changed();const auto generation=generation_;
    auto* watcher=new QFutureWatcher<QByteArray>(this);
    connect(watcher,&QFutureWatcher<QByteArray>::finished,this,[this,watcher,generation]{
        const auto result=watcher->result();watcher->deleteLater();if(generation!=generation_)return;
        if(result.isEmpty()){fail("Couldn't read this picture. Try a smaller PNG, JPEG or WebP.");return;}
        temporary_=std::make_unique<QTemporaryDir>();QFile file(temporary_->filePath("picture.jpg"));
        if(!file.open(QIODevice::WriteOnly)||file.write(result)!=result.size()){fail("Couldn't prepare this picture.");return;}
        file.close();prepared_=result;picture_=QUrl::fromLocalFile(file.fileName());state_="picture";emit changed();
    });
    watcher->setFuture(QtConcurrent::run(pictureCopy,std::move(bytes)));
}
void SocialMedia::record() {
    clear();const auto device=QMediaDevices::defaultAudioInput();
    QAudioFormat format;format.setSampleRate(Rate);format.setChannelCount(1);format.setSampleFormat(QAudioFormat::Int16);
    if(device.isNull()||device.id().endsWith(".monitor")||device.description().startsWith("Monitor of",Qt::CaseInsensitive)
        ||!device.isFormatSupported(format)||QStandardPaths::findExecutable("ffmpeg").isEmpty()){
        fail("A microphone and audio encoder are needed to record.");return;
    }
    input_=new QAudioSource(device,format,this);input_->setBufferSize(Rate/5);
    connect(input_,&QAudioSource::stateChanged,this,[this](QAudio::State){if(input_&&input_->error()!=QAudio::NoError){input_->disconnect(this);input_->stop();limit_.stop();fail("Microphone disconnected or unavailable.");}});
    state_="recording";voice_=true;inputStream_=input_->start();
    if(!inputStream_){fail("Couldn't open the microphone.");return;}
    connect(inputStream_,&QIODevice::readyRead,this,&SocialMedia::receivePcm);limit_.start();emit changed();
    const auto generation=generation_;
    QTimer::singleShot(3000,this,[this,generation]{if(generation==generation_&&state_=="recording"&&pcm_.isEmpty()){
        input_->disconnect(this);input_->stop();delete input_;input_=nullptr;inputStream_=nullptr;limit_.stop();
        fail("No microphone audio. Connect a headset and try again.");
    }});
}
void SocialMedia::receivePcm(){
    if(!inputStream_||state_!="recording")return;
    pcm_.append(inputStream_->readAll());seconds_=pcm_.size()/(Rate*2);
    if(pcm_.size()>=120*Rate*2)stopRecording();else emit changed();
}
void SocialMedia::stopRecording() {
    if(state_!="recording")return;
    limit_.stop();input_->stop();delete input_;input_=nullptr;inputStream_=nullptr;
    if(pcm_.size()<Rate){fail("The recording was too short.");return;}
    pcm_.truncate(qMin(pcm_.size(),qsizetype(120*Rate*2)));seconds_=qMax(1,int(pcm_.size()/(Rate*2)));
    waveform_.resize(64);const auto samples=pcm_.size()/2;
    for(int i=0;i<64;++i){int peak=0;for(qsizetype n=i*samples/64;n<(i+1)*samples/64;++n)peak=qMax(peak,std::abs(int(qFromLittleEndian<qint16>(pcm_.constData()+n*2))));waveform_[i]=char(qMin(255,peak/128));}
    temporary_=std::make_unique<QTemporaryDir>();const auto path=temporary_->filePath("voice.ogg");
    encoder_=new QProcess(this);state_="encoding";emit changed();
    connect(encoder_,&QProcess::errorOccurred,this,[this]{fail("Couldn't encode this recording.");});
    connect(encoder_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,path](int code,QProcess::ExitStatus status){
        encoder_->deleteLater();encoder_=nullptr;pcm_.clear();QFile file(path);
        if(code||status!=QProcess::NormalExit||!file.open(QIODevice::ReadOnly)||file.size()>MaxBytes){fail("Couldn't encode this recording.");return;}
        prepared_=file.readAll();ensurePlayer();player_->setSource(QUrl::fromLocalFile(path));state_="voice";emit changed();
    });
    encoder_->start(QStandardPaths::findExecutable("ffmpeg"),{"-hide_banner","-loglevel","error","-f","s16le","-ar",QString::number(Rate),"-ac","1","-i","pipe:0","-map_metadata","-1","-c:a","libopus","-b:a","32k","-y",path});
    encoder_->write(pcm_);encoder_->closeWriteChannel();
    QTimer::singleShot(15000,encoder_,[process=encoder_]{if(process->state()!=QProcess::NotRunning)process->kill();});
}
void SocialMedia::play(){if(!player_)return;if(playing())player_->pause();else if(!player_->source().isEmpty())player_->play();}
void SocialMedia::open(const QVariantMap& attachment) {
    clear();const QUrl url(attachment["url"].toString());
    // Only the instance's media proxy. Never follow arbitrary text links or forward credentials.
    if(url.scheme()!="https"||!QStringList{"media.fluxer.app","fluxerusercontent.com"}.contains(url.host())
        ||!url.userInfo().isEmpty()||url.port(443)!=443){fail("This attachment's media address is unavailable.");return;}
    const auto mime=attachment["content_type"].toString();voice_=mime.startsWith("audio/");
    if(!voice_&&!mime.startsWith("image/")){fail("This attachment can't be opened here.");return;}
    if(attachment["size"].toLongLong()>MaxBytes){fail("This attachment is too large to preview.");return;}
    state_="loading";emit changed();QNetworkRequest request(url);request.setTransferTimeout(20000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    download_=network_->get(request);download_->setReadBufferSize(MaxBytes+1);auto bytes=std::make_shared<QByteArray>();
    connect(download_,&QIODevice::readyRead,this,[this,bytes]{bytes->append(download_->readAll());if(bytes->size()>MaxBytes)download_->abort();});
    connect(download_,&QNetworkReply::finished,this,[this,bytes,attachment]{
        auto* reply=download_;download_=nullptr;bytes->append(reply->readAll());reply->deleteLater();
        if(reply->error()!=QNetworkReply::NoError||reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()!=200||bytes->size()>MaxBytes){fail("Couldn't load the attachment. Refresh the conversation and try again.");return;}
        if(!voice_){preparePicture(*bytes);return;}
        temporary_=std::make_unique<QTemporaryDir>();QFile file(temporary_->filePath("audio"));
        if(!file.open(QIODevice::WriteOnly)||file.write(*bytes)!=bytes->size()){fail("Couldn't prepare playback.");return;}
        file.close();seconds_=attachment["duration"].toInt();waveform_=QByteArray::fromBase64(attachment["waveform"].toByteArray()).left(128);ensurePlayer();player_->setSource(QUrl::fromLocalFile(file.fileName()));state_="voice";emit changed();
    });
}
void SocialMedia::chime() {
    if(state_=="recording"||playing())return;
    if(sound_){sound_->stop();delete sound_;sound_=nullptr;}
    QAudioFormat format;format.setSampleRate(24000);format.setChannelCount(1);format.setSampleFormat(QAudioFormat::Int16);
    if(!QMediaDevices::defaultAudioOutput().isFormatSupported(format))return;
    sound_=new QAudioSink(format,this);sound_->setVolume(.23);
    auto* buffer=new QBuffer(sound_);QByteArray data(7200*2,0);
    for(int n=0;n<7200;++n){const double t=n/24000.;const double amplitude=std::sin(3.141592653589793*n/7200.);const double f=n<3200?660:880;
        qToLittleEndian<qint16>(qint16(11000*amplitude*std::sin(6.283185307179586*f*t)),data.data()+n*2);}
    buffer->setData(data);buffer->open(QIODevice::ReadOnly);sound_->start(buffer);
}
}
