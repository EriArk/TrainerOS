#pragma once
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStringDecoder>
#include <functional>

namespace trainer::disc {
inline bool playlist(const QString& path) {
    const auto ext=QFileInfo(path).suffix().toLower();
    return ext=="m3u"||ext=="m3u8";
}
// A list describes ordered local discs, never nested lists, commands or URLs.
// Consumers validate each image using their native reader before admission.
inline QStringList entries(const QString& path,const QStringList& formats,
                           const std::function<void(const QString&)>& observe={}) {
    if(observe)observe(path);
    if(!playlist(path))return formats.contains(QFileInfo(path).suffix().toLower())?QStringList{path}:QStringList{};
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)||file.size()<1||file.size()>65536)return {};
    QStringDecoder decoder(QStringDecoder::Utf8);
    QString text=decoder(file.readAll());
    if(decoder.hasError()||text.contains(QChar(0)))return {};
    if(text.startsWith(QChar(0xfeff)))text.remove(0,1);
    QStringList result;QSet<QString> seen;
    for(auto line:text.split('\n')) {
        line=line.trimmed();if(line.isEmpty()||line.startsWith('#'))continue;
        if(line.contains("://")||line.contains('\r')||line.contains('\t'))return {};
        line.replace('\\','/');
        const auto resolved=QDir::cleanPath(QFileInfo(line).isAbsolute()?line:QFileInfo(path).dir().absoluteFilePath(line));
        if(observe)observe(resolved);
        const QFileInfo info(resolved);const auto canonical=info.canonicalFilePath();
        if(!formats.contains(info.suffix().toLower())||playlist(resolved)||!info.isFile()||!info.isReadable()
           ||canonical.isEmpty()||seen.contains(canonical)||result.size()>=16)return {};
        seen.insert(canonical);result.append(resolved);
    }
    return result;
}
}
