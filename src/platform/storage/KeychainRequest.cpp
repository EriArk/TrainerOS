#include "KeychainRequest.h"
#include <QCoreApplication>
#include <QThread>
#include <qt6keychain/keychain.h>

namespace trainer {
void KeychainRequest::run(QString service, QString key, Operation operation, QByteArray data,
                          QObject* receiver, Completion completed) {
    auto* request = new KeychainRequest;
    request->service_ = std::move(service);
    request->key_ = std::move(key);
    request->operation_ = operation;
    request->data_ = std::move(data);
    request->keepAlive_ = completed;
    connect(request, &KeychainRequest::finished, receiver, std::move(completed));
    request->moveToThread(QCoreApplication::instance()->thread());
    QMetaObject::invokeMethod(request, &KeychainRequest::start, Qt::QueuedConnection);
}
void KeychainRequest::start() {
    Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
    QKeychain::Job* job;
    if (operation_ == Read) job = new QKeychain::ReadPasswordJob(service_, this);
    else if (operation_ == Remove) job = new QKeychain::DeletePasswordJob(service_, this);
    else {
        auto* write = new QKeychain::WritePasswordJob(service_, this);
        write->setBinaryData(data_);
        job = write;
    }
    data_.clear();
    job->setKey(key_);
    job->setInsecureFallback(false);
    connect(job, &QKeychain::Job::finished, this, [this](QKeychain::Job* result) {
        const auto bytes = operation_ == Read ? static_cast<QKeychain::ReadPasswordJob*>(result)->binaryData() : QByteArray{};
        emit finished(result->error(), bytes);
        deleteLater();
    });
    job->start();
}
}
