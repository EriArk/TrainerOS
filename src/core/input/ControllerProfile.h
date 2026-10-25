#pragma once
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVariantList>
#include <array>
#include <algorithm>
#include <cmath>

namespace trainer {
// Device-wide shell preferences. Game bindings live in a separate owner/game profile.
struct ControllerProfile {
    QString family = "auto";
    QString confirm = "auto";
    int deadzone = 20;
    bool invertX = false, invertY = false;
    // SDL-normalized center and endpoints; no evdev units or device guesses.
    std::array<double,6> center{0,0,0,0,0,0}, minimum{-1,-1,-1,-1,0,0}, maximum{1,1,1,1,1,1};
    double axis(int index,double value) const {
        const double delta=value-center[index];
        const double span=delta<0?center[index]-minimum[index]:maximum[index]-center[index];
        return span<=0?0:std::clamp(delta/span,index<4?-1.0:0.0,1.0);
    }
    static QStringList families() { return {"auto", "nintendo", "xbox", "playstation", "steam", "generic"}; }
    static ControllerProfile read(const QVariantMap& v) {
        ControllerProfile p;
        if (families().contains(v.value("family").toString())) p.family=v.value("family").toString();
        if (QStringList{"auto","east","south"}.contains(v.value("confirm").toString())) p.confirm=v.value("confirm").toString();
        bool ok=false; const int dz=v.value("deadzone",20).toInt(&ok);
        if(ok && dz>=5 && dz<=40)p.deadzone=dz;
        p.invertX=v.value("invertX",false).toBool();p.invertY=v.value("invertY",false).toBool();
        const auto axes=v.value("axes").toList();
        if(axes.size()==6) for(int i=0;i<6;++i) {
            const auto a=axes[i].toMap();const double c=a.value("center",999).toDouble(),lo=a.value("min",999).toDouble(),hi=a.value("max",999).toDouble();
            if(std::isfinite(c)&&std::isfinite(lo)&&std::isfinite(hi)&&lo>=-1&&hi<=1&&hi-c>=.4
                &&(i<4?(std::abs(c)<=.25&&c-lo>=.4):(c>=0&&c<=.25&&lo==0))) {
                p.center[i]=c;p.minimum[i]=lo;p.maximum[i]=hi;
            }
        }
        return p;
    }
    QVariantMap json() const { QVariantList axes;for(int i=0;i<6;++i)axes.append(QVariantMap{{"center",center[i]},{"min",minimum[i]},{"max",maximum[i]}});
        return {{"family",family},{"confirm",confirm},{"deadzone",deadzone},{"invertX",invertX},{"invertY",invertY},{"axes",axes}}; }
};
}
