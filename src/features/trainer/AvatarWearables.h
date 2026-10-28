#pragma once
#include <QJsonObject>
#include <QByteArray>
#include <QString>
namespace trainer {
// Merge trusted, hashed pack garments onto the existing rig, including correctives.
bool applyAvatarWearables(QJsonObject& gltf, QByteArray& binary, const QJsonObject& manifest,
                         const QJsonObject& appearance, const QString& root);
bool compactAvatar(QJsonObject& gltf, QByteArray& binary);
}
