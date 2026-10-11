// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "CartridgeState.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace traineros::ds {
// Private, local launcher staging contract. Never accept this manifest from a
// network peer. It contains no arbitrary paths; it cannot name an original save.
// The shell owns consent, exact runtime/party identity, original-save protection
// and cleanup. A returned image is only a candidate for that shared commit path.
struct PreparedSession {
    QString directory, contentHash;
    size_t ownerSeat = 0;
    CartridgeStartup startup;
    bool peerPending = false;
};
inline QByteArray ReadStagedFile(const QString& path, qint64 maximum) {
    const QFileInfo info(path);
    if (!info.isFile() || info.isSymLink() || info.size() < 1 || info.size() > maximum)
        throw std::runtime_error("Invalid prepared-session file");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read prepared session");
    auto bytes=f.read(maximum+1);
    if (f.error()!=QFile::NoError || bytes.size()!=info.size() || bytes.size()>maximum)
        throw std::runtime_error("Prepared session changed while reading");
    return bytes;
}
inline bool StateDigest(const QString& value) {
    return QRegularExpression("^[0-9a-f]{64}$").match(value).hasMatch();
}
inline QString StateHash(const QByteArray& bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
}
inline PreparedSession ReadPreparedSession(const QString& directory, const QString& contentHash, unsigned seats,
                                           bool allowPeer = false) {
    const QFileInfo info(directory);
    if (!info.isAbsolute() || !info.isDir() || info.isSymLink()
            || QDir::cleanPath(info.absoluteFilePath())!=info.canonicalFilePath())
        throw std::runtime_error("Prepared session must be a real absolute directory");
    const QDir folder(directory);
    if (QFileInfo::exists(folder.filePath("owner-return.sav"))
            || QFileInfo::exists(folder.filePath("owner-return.json")))
        throw std::runtime_error("Prepared session has already returned progress");
    const auto bytes=ReadStagedFile(folder.filePath("session.json"),4096);
    QJsonParseError parse;
    const auto doc=QJsonDocument::fromJson(bytes,&parse);
    const auto value=doc.object();
    const int owner=value["ownerSeat"].toInt(-1);
    const bool pending=value["version"]==QJsonValue(2);
    if (parse.error!=QJsonParseError::NoError || !doc.isObject() || value.size()!=4
            || (value["version"]!=QJsonValue(1) && !(pending&&allowPeer&&seats==2)) || owner<0 || owner>=int(seats)
            || value["ownerSeat"]!=QJsonValue(owner) || !StateDigest(contentHash)
            || value["content"]!=contentHash || !value["seats"].isArray())
        throw std::runtime_error("Prepared session identity mismatch");
    const auto states=value["seats"].toArray();
    if (seats<2 || seats>4 || states.size()!=int(seats))
        throw std::runtime_error("Prepared session roster mismatch");
    PreparedSession result{info.canonicalFilePath(),contentHash,size_t(owner),{{},size_t(owner)}};
    for (unsigned i=0;i<seats;++i) {
        if (!states[int(i)].isObject()) throw std::runtime_error("Missing cartridge state choice");
        const auto state=states[int(i)].toObject();
        if(pending&&i!=unsigned(owner)) {
            if(state.size()!=1||state["peer"]!=QJsonValue(true))
                throw std::runtime_error("Network mirror requires an explicit accepted peer state");
            result.peerPending=true;result.startup.seats.push_back(std::nullopt);continue;
        }
        if (state.size()==1 && state["fresh"]==QJsonValue(true)) {
            result.startup.seats.push_back(std::nullopt); continue;
        }
        const int size=state["bytes"].toInt(-1);
        if (state.size()!=2 || size<1 || size>int(MaxCartridgeSaveBytes)
                || state["bytes"]!=QJsonValue(size) || !StateDigest(state["sha256"].toString()))
            throw std::runtime_error("Invalid cartridge state description");
        const auto data=ReadStagedFile(folder.filePath(QString("seat-%1.sav").arg(i)),size);
        if (data.size()!=size || StateHash(data)!=state["sha256"].toString())
            throw std::runtime_error("Prepared cartridge state changed");
        result.startup.seats.push_back(std::vector<uint8_t>(data.begin(),data.end()));
    }
    return result;
}
inline void WriteNewStagedFile(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly|QIODevice::NewOnly)
            || !file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner)
            || file.write(bytes)!=bytes.size() || !file.flush())
        throw std::runtime_error("Could not retain session progress; original save is unchanged");
}
inline void WriteOwnerReturn(const PreparedSession& prepared, const OwnerCartridgeReturn& value) {
    if (value.bytes.empty()==value.hasSave || value.bytes.size()>MaxCartridgeSaveBytes)
        throw std::runtime_error("Invalid owner return size");
    const QFileInfo info(prepared.directory);
    if (!info.isDir() || info.isSymLink() || info.canonicalFilePath()!=prepared.directory)
        throw std::runtime_error("Prepared session directory changed");
    const QDir folder(prepared.directory);
    const QByteArray bytes(reinterpret_cast<const char*>(value.bytes.data()),qsizetype(value.bytes.size()));
    if(value.hasSave)WriteNewStagedFile(folder.filePath("owner-return.sav"),bytes);
    const QJsonObject receipt{{"version",1},{"ownerSeat",int(prepared.ownerSeat)},
        {"content",prepared.contentHash},{"frame",qint64(value.frame)},
        {"bytes",qint64(bytes.size())},{"sha256",StateHash(bytes)},{"hasSave",value.hasSave}};
    // Receipt last: an interrupted/failed write cannot be mistaken for success.
    WriteNewStagedFile(folder.filePath("owner-return.json"),QJsonDocument(receipt).toJson(QJsonDocument::Compact));
}
}
