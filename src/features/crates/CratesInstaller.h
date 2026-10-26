#pragma once
#include <QObject>
#include <QProcess>
#include <QVariantList>
#include <QTimer>
#include <QMap>
#include <QFileSystemWatcher>
#include <functional>
namespace trainer {
class CratesInstaller final : public QObject {
    Q_OBJECT
public:
    explicit CratesInstaller(QObject* parent=nullptr);
    ~CratesInstaller() override;
    void configure(const QString& helper,const QString& registry,const QString& root);
    void useCatalogue(const QVariantList& packages,const QString& signedRegistry);
    void setOnline(bool online);
    QVariantMap state(const QString& id) const;
    QVariantList tasks() const;
    QVariantList installed() const;
    QVariantList activities() const;
    void request(const QString& id,const QString& version={},const QString& operation="install");
    void queueAutomaticUpdates();
    void stopAutomaticUpdates();
    void command(const QString& id,const QString& command);
    void moveBefore(const QString& id,const QString& before);
    QString root() const {return root_;}
    QString helper() const {return helper_;}
    std::function<QString()> replacementGuard;
signals:
    void changed();
    void inventoryChanged();
private:
    void pump();
    void readInventory();
    void watchInventory();
    void event(const QByteArray&);
    void persistJobs();
    bool hasTransactions() const;
    void settleInterruptedJobs(bool recovered=true);
    QString helper_,registry_,root_,active_;
    QVariantList packages_,jobs_;
    QMap<QString,QVariantMap> inventory_;
    QProcess worker_;
    QByteArray buffer_;
    QTimer retry_;
    QTimer persist_;
    QFileSystemWatcher inventoryWatch_;
    QTimer inventoryRefresh_;
    bool online_=true;
    bool recoveryPending_=false;
};
}
