#pragma once
#include "core/model/Models.h"
#include <QHash>
#include <QVariantMap>

namespace trainer {
// Trusted presentation registry. A view key is resolved by compiled host QML;
// descriptors never contain paths, executable QML, commands or save permissions.
struct ExperienceFace {
    QString id, label, view;
    bool operator==(const ExperienceFace&) const = default;
};
struct ExperienceSlot {
    QString label;
    QList<ExperienceFace> faces;
    bool operator==(const ExperienceSlot&) const = default;
};
struct ExperienceDescriptor {
    QString id;
    int version = 1;
    QString homeView;
    ExperienceSlot first, second;
    bool operator==(const ExperienceDescriptor&) const = default;
};
const ExperienceDescriptor& genericExperienceDescriptor();

class ExperienceNavigation {
public:
    // Explicit library family selects presentation only. Exact-build providers
    // retain independent read/write capability checks behind every operation.
    bool select(const QString& owner, const QString& adventure,
                const ExperienceDescriptor&, int registrationRevision = 0);
    const ExperienceDescriptor& descriptor() const { return descriptor_; }
    QString adventure() const { return adventure_; }
    QString owner() const { return owner_; }
    quint64 generation() const { return generation_; }
    bool current(const QString& owner, const QString& adventure, quint64 generation) const;
    QStringList labels(int slot) const;
    QStringList faceIds(int slot) const;
    QString face(int slot) const;
    QString view(int slot) const;
    int index(int slot) const;
    QJsonObject navigation() const;
    void rememberNavigation(const QJsonObject&);
    bool show(int slot, const QString& face);
    bool cycle(int slot, int delta);
    QJsonObject state() const;
    void restore(const QJsonObject&);
    void clear();
    static bool valid(const ExperienceDescriptor&);
private:
    const ExperienceSlot& slot(int slot) const;
    QString key() const;
    ExperienceDescriptor descriptor_ = genericExperienceDescriptor();
    QString owner_, adventure_;
    int registrationRevision_ = 0;
    quint64 generation_ = 0;
    QHash<QString, QJsonObject> states_;
};
}
