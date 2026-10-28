#pragma once
#include <QObject>
#include <QJsonObject>
#include <QVariantMap>
#include <QFutureWatcher>
#include <QTemporaryDir>
namespace trainer {
class BackendAccount;
class VisitorProfile final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap data READ data NOTIFY changed)
    Q_PROPERTY(QVariantMap sharing READ sharing NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString avatarPreview READ avatarPreview NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool preview READ preview NOTIFY changed)
public:
    explicit VisitorProfile(BackendAccount&,QObject* parent=nullptr);
    ~VisitorProfile() override;
    QVariantMap data() const {return data_.toVariantMap();}
    QVariantMap sharing() const {return sharing_.toVariantMap();}
    QString status() const {return status_;}
    QString avatarPreview() const {return previewUrl_;}
    bool busy() const {return busy_;}
    bool preview() const {return preview_;}
    QString subject() const {return data_["subject"].toString(subject_);}
    void open(QString subject,QString player={},bool preview=false);
    void close();
    void refresh(bool more=false);
    void loadSharing();
    void toggleSharing(int index);
    void publishFavorites(QJsonArray games);
signals:
    void changed();
private:
    void render();
    BackendAccount& account_;
    QJsonObject data_,sharing_;
    QString owner_,subject_,player_,status_,previewUrl_;
    bool busy_=false,preview_=false,open_=false;
    quint64 generation_=0,renderGeneration_=0;
    QTemporaryDir cache_;
    QFutureWatcher<QJsonObject> job_;
};
}
