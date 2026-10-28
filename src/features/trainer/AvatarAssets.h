#pragma once
#include <QJsonObject>
#include <QString>
#include <QStringList>
namespace trainer {
// Console-owned data, independent of the renderer and game engine.
class AvatarAssets {
public:
    static QJsonObject defaults();
    static QJsonObject normalized(const QJsonObject&);
    static bool valid(const QJsonObject&);
    static QStringList keys();
    static QStringList choices(const QString& key);
    static bool onePiece(const QString& id);
    static QJsonObject equipped(const QJsonObject&, const QString& key, const QString& value);
    static QString packRoot();
    static bool available();
    // Worker-thread operation: resolve a content-addressed standard GLB, never game files.
    static QJsonObject resolve(const QJsonObject&, const QString& root = {}, const QString& cache = {});
    static QJsonObject resolveRecipe(const QJsonObject&, const QString& cache);
};
}
