#pragma once
#include <QObject>
#include <QVariantList>
#include <QUrl>
#include <QByteArray>
#include <QTemporaryDir>
#include <QTimer>
#include <memory>
class QAudioSource;
class QAudioSink;
class QAudioOutput;
class QMediaPlayer;
class QNetworkAccessManager;
class QNetworkReply;
class QProcess;
namespace trainer {
// One owner, one explicit capture/preview at a time. Originals are never edited.
class SocialMedia final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QUrl picture READ picture NOTIFY changed)
    Q_PROPERTY(int seconds READ seconds NOTIFY changed)
    Q_PROPERTY(bool playing READ playing NOTIFY changed)
    Q_PROPERTY(QVariantList levels READ levels NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
public:
    explicit SocialMedia(QObject* parent=nullptr);
    ~SocialMedia() override;
    QString state() const{return state_;}
    QString error() const{return error_;}
    QUrl picture() const{return picture_;}
    int seconds() const{return seconds_;}
    bool playing() const;
    QVariantList levels() const;
    double progress() const;
    QVariantList pictures() const;
    void choosePicture(const QString& path);
    void record();
    void stopRecording();
    void open(const QVariantMap& attachment);
    Q_INVOKABLE void play();
    void clear();
    void chime();
    void setAudioDevices(QString input,QString output,int volume);
    QByteArray bytes() const{return prepared_;}
    QByteArray waveform() const{return waveform_;}
    bool voice() const{return voice_;}
signals:
    void changed();
private:
    void preparePicture(QByteArray bytes);
    void ensurePlayer();
    void fail(QString message);
    void receivePcm();
    QString state_,error_;
    QString inputDevice_,outputDevice_;
    int volume_=100;
    QUrl picture_;
    QByteArray pcm_,prepared_,waveform_;
    int seconds_=0;
    bool voice_=false;
    quint64 generation_=0;
    std::unique_ptr<QTemporaryDir> temporary_;
    QAudioSource* input_=nullptr;
    QIODevice* inputStream_=nullptr;
    QAudioOutput* output_=nullptr;
    QMediaPlayer* player_=nullptr;
    QAudioSink* sound_=nullptr;
    QNetworkAccessManager* network_=nullptr;
    QNetworkReply* download_=nullptr;
    QProcess* encoder_=nullptr;
    QTimer limit_;
};
}
