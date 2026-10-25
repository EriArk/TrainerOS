#pragma once
#include <QByteArray>
#include <QString>
#include <atomic>

namespace trainer::cartridge {
struct Image {
    QByteArray bytes;
    QString extension;
    QString digest() const;
};
bool archived(const QString& path);
// Exactly one cartridge for this platform. No entry paths are extracted.
// Payload/hash are identical for a raw cartridge and its ZIP/7z representation.
Image read(const QString& path,const QString& platform,const std::atomic_bool& cancel);
// A caller-owned private session directory retains the archive's save basename.
QString stage(const Image&,const QString& original,const QString& directory,const QString& expected);
}
