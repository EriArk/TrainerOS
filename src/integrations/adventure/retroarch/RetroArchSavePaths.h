#pragma once
#include "RetroArchConfiguration.h"
#include <QCryptographicHash>
#include <QDir>

namespace trainer::retroarch {
inline QString digestId(const QString& id) {
    return QString::fromLatin1(QCryptographicHash::hash(id.toUtf8(),QCryptographicHash::Sha256).toHex());
}
inline bool ownedDirectory(const QString& base,const QString& path,bool create) {
    if(!safePath(base) || !safePath(path) || QFileInfo(base).isSymLink())return false;
    const auto relative=QDir(base).relativeFilePath(path);
    if(relative.startsWith("..") || QDir::isAbsolutePath(relative))return false;
    QString current=base;
    for(const auto& component:relative.split('/')) {
        current=QDir(current).filePath(component);
        if(QFileInfo(current).isSymLink() || (QFileInfo::exists(current) && !QFileInfo(current).isDir()))return false;
        if(create && !QDir().mkpath(current))return false;
    }
    return true;
}
}
