#include "AltchaProof.h"
#include <QJsonDocument>
#include <QPasswordDigestor>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QTimer>
#include <QtEndian>

namespace trainer {
AltchaProof::AltchaProof(QJsonObject challenge, Completed completed, QObject* parent)
    : QObject(parent), challenge_(std::move(challenge)), completed_(std::move(completed)) {
    const auto p = challenge_["parameters"].toObject();
    const auto hex = [](const QString& s) {
        return !s.isEmpty() && s.size() <= 512 && s.size() % 2 == 0
            && QRegularExpression("^[0-9a-fA-F]+$").match(s).hasMatch();
    };
    const auto nonce = p["nonce"].toString(), salt = p["salt"].toString();
    prefix_ = p["keyPrefix"].toString().toLatin1().toLower();
    cost_ = p["cost"].toInt(); length_ = p["keyLength"].toInt();
    const bool valid = p["algorithm"] == "PBKDF2/SHA-256" && hex(nonce) && hex(salt)
        && cost_ > 0 && cost_ <= 1000000 && length_ > 0 && length_ <= 64
        && !prefix_.isEmpty() && prefix_.size() <= length_ * 2
        && QRegularExpression("^[0-9a-f]+$").match(QString::fromLatin1(prefix_)).hasMatch()
        && !challenge_["signature"].toString().isEmpty()
        && QJsonDocument(challenge_).toJson(QJsonDocument::Compact).size() <= 2500;
    nonce_ = QByteArray::fromHex(nonce.toLatin1()); salt_ = QByteArray::fromHex(salt.toLatin1());
    elapsed_.start();
    QTimer::singleShot(0, this, [this, valid] { if (valid) step(); else finish(); });
}
void AltchaProof::finish(QByteArray token) {
    auto done = std::move(completed_); deleteLater(); done(std::move(token));
}
void AltchaProof::step() {
    QElapsedTimer slice; slice.start();
    do {
        if (elapsed_.elapsed() > 90000) { finish(); return; }
        auto password = nonce_; const auto offset = password.size(); password.resize(offset + 4);
        qToBigEndian(counter_, password.data() + offset);
        const auto key = QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256,
            password, salt_, cost_, length_).toHex();
        if (key.startsWith(prefix_)) {
            const QJsonObject solution{{"counter",qint64(counter_)},{"derivedKey",QString::fromLatin1(key)}};
            auto token = QJsonDocument(QJsonObject{{"challenge",challenge_},{"solution",solution}})
                .toJson(QJsonDocument::Compact).toBase64();
            finish(token.size() <= 4096 ? token : QByteArray()); return;
        }
        if (++counter_ == 0) { finish(); return; }
    } while (slice.elapsed() < 8);
    QTimer::singleShot(1, this, [this] { step(); });
}
}
