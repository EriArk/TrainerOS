#pragma once
#include <QString>
#include <functional>

namespace trainer {
// Streaming decoding is libarchive's responsibility. This sink only admits
// ordinary files/directories into a new, empty, local destination.
class ArchiveExtractor {
public:
    static QString extract(const QString& source, const QString& destination,
                           const std::function<bool()>& cancelled,
                           const std::function<void(double)>& progress,
                           qint64 byteLimit = 512LL * 1024 * 1024 * 1024);
};
}
