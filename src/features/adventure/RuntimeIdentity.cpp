#include "RuntimeIdentity.h"
#include "integrations/adventure/standalone/XboxLan.h"
#include "integrations/adventure/retroarch/PS2Netplay.h"
#include "integrations/adventure/standalone/PpssppNetplay.h"
#include "integrations/adventure/standalone/DolphinNetplay.h"
#include "integrations/adventure/standalone/RmgNetplay.h"
#include "integrations/adventure/standalone/FlycastNetplay.h"
#include "integrations/adventure/standalone/MelonDsNetplay.h"
#include "integrations/adventure/standalone/AzaharNetplay.h"
#include "integrations/adventure/standalone/VitaNetplay.h"
#include "integrations/adventure/standalone/Ps2LanNetplay.h"
namespace trainer {
QJsonObject runtimeIdentity(const AdventureRegistration& original,const RetroArchInstallation& installation,
 const StandaloneInstallation& pspInstallation,const StandaloneInstallation& dolphinInstallation,
 const std::atomic_bool& cancel,int capacity,retroarch::NetplayDigestCache* digests) {
    const auto r=retroarch::netplayRegistration(original);
    const auto platform=r.adventure.platformId;
    if(platform=="ps2") {
        const auto native=ps2lan::netplayIdentity(r,cancel,[digests](const QString& path){if(digests)digests->observe(path);});
        return native.isEmpty()?ps2::netplayIdentity(r,installation,cancel):native;
    }
    if(platform=="xbox")return xboxlan::netplayIdentity(r,cancel);
    if(platform=="psvita")return vita::netplayIdentity(r,cancel);
    if(platform=="n3ds")return azahar::netplayIdentity(r,cancel,[digests](const QString& path){if(digests)digests->observe(path);});
    if(platform=="nds")return melonds::netplayIdentity(r,cancel);
    if(platform=="dreamcast"||flycast::arcadePlatform(platform))return flycast::netplayIdentity(r,cancel,[digests](const QString& path){if(digests)digests->observe(path);});
    if(platform=="n64")return rmg::netplayIdentity(r,cancel,capacity);
    if(platform=="gc"||platform=="wii")return dolphin::netplayIdentity(r,dolphinInstallation,cancel,capacity,
        [digests](const QString& path){if(digests)digests->observe(path);});
    if(platform=="psp")return ppsspp::netplayIdentity(r,pspInstallation,cancel);
    return retroarch::netplayIdentity(r,installation,cancel,digests,capacity);
}
}
