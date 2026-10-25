#pragma once
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace trainer {
// Device-wide shell preferences. Game bindings live in a separate owner/game profile.
struct ControllerProfile {
    QString family = "auto";
    QString confirm = "auto";
    int deadzone = 20;
    bool invertX = false, invertY = false;
    static QStringList families() { return {"auto", "nintendo", "xbox", "playstation", "steam", "generic"}; }
    static ControllerProfile read(const QVariantMap& v) {
        ControllerProfile p;
        if (families().contains(v.value("family").toString())) p.family=v.value("family").toString();
        if (QStringList{"auto","east","south"}.contains(v.value("confirm").toString())) p.confirm=v.value("confirm").toString();
        bool ok=false; const int dz=v.value("deadzone",20).toInt(&ok);
        if(ok && dz>=5 && dz<=40)p.deadzone=dz;
        p.invertX=v.value("invertX",false).toBool();p.invertY=v.value("invertY",false).toBool();
        return p;
    }
    QVariantMap json() const { return {{"family",family},{"confirm",confirm},{"deadzone",deadzone},{"invertX",invertX},{"invertY",invertY}}; }
};
}
