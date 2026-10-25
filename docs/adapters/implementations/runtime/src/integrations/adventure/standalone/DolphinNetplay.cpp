#include "DolphinNetplay.h"
#include "NetplayCache.h"
#include "core/model/GamePlayers.h"
#include "integrations/adventure/retroarch/RetroArchConfiguration.h"
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QSet>
#include <QProcess>
#include <QElapsedTimer>

namespace trainer::dolphin {
QString bridgeRoot() {return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/TrainerOS/emulators/dolphin-netplay";}
QString bridgeFile() {return bridgeRoot()+"/bin/dolphin-emu";}
QJsonObject netplayIdentity(const AdventureRegistration& r,const StandaloneInstallation& i,const std::atomic_bool& cancel,int players) {
    const bool wii=r.adventure.platformId=="wii";
    if((!wii&&r.adventure.platformId!="gc")||r.adventure.adapterId!="dolphin"||!i.platforms.contains(r.adventure.platformId))return {};
    if(players<2||players>4)return {};
    const auto extension=QFileInfo(r.contentPath).suffix().toLower();
    const bool raw=extension=="iso"||extension=="gcm";
    if(!raw&&!QStringList{"rvz","wia","gcz","ciso"}.contains(extension)&&!(wii&&extension=="wbfs"))return {};
    const qint64 maxSize=wii?9LL*1024*1024*1024:1500LL*1024*1024;
    QFile file(r.contentPath);
    if(!file.open(QIODevice::ReadOnly)||file.size()<0x20||file.size()>maxSize)return {};
    const auto header=file.read(0x20);
    if(!wii&&raw&&header.mid(0x1c,4)!=QByteArray::fromHex("c2339f3d"))return {};
    QFile manifest(bridgeRoot()+"/manifest.json");
    if(!manifest.open(QIODevice::ReadOnly)||manifest.size()>8192)return {};
    const auto m=QJsonDocument::fromJson(manifest.readAll()).object();
    if(m["protocol"]!=1||m["source"]!="c77bbaa0f372c3f72281602a8b087206706542cb")return {};
    const auto runtime=retroarch::fileDigest(bridgeFile(),256*1024*1024,cancel);
    if(runtime.isEmpty()||runtime!=m["sha256"].toString())return {};
    if(wii&&m["wiimote"]!="dolphin-wiimote-v1")return {};
    if(wii||!raw) {
        // Use Dolphin's own volume reader for compressed discs. Extensions do
        // not establish the console type; preserve legacy raw-GC installations.
        const auto tool=bridgeRoot()+"/bin/dolphin-tool";
        const auto toolHash=retroarch::fileDigest(tool,128*1024*1024,cancel);
        if(toolHash.isEmpty()||toolHash!=m["toolSha256"].toString())return {};
        QProcess probe;probe.setProgram(tool);probe.setArguments({"header","-j","-i",r.contentPath});
        probe.setStandardErrorFile(QProcess::nullDevice());probe.start();
        if(!probe.waitForStarted(1500))return {};
        QElapsedTimer elapsed;elapsed.start();
        while(!probe.waitForFinished(50)) {
            if(cancel||elapsed.elapsed()>10000||probe.bytesAvailable()>8192) {
                probe.kill();probe.waitForFinished(1000);return {};
            }
        }
        const auto output=probe.readAllStandardOutput();
        if(cancel||probe.exitStatus()!=QProcess::NormalExit||probe.exitCode()!=0||output.size()>8192||
           QJsonDocument::fromJson(output).object()["platform"]!=r.adventure.platformId)return {};
    }
    const auto content=retroarch::fileDigest(r.contentPath,maxSize,cancel);
    if(content.isEmpty())return {};
    QJsonObject identity;
    if(wii)identity={{"id","runtime.dolphin.wii.v1"},{"label",r.adventure.title.left(96)},
        {"content",content},{"runtime",runtime},{"settings","dolphin-wiimote-isolated-v1"},
        {"players",players},{"lateJoin",false}};
    else if(content=="53a5d0f7c480045a435c889ad47b6d351faff0fc409a7285f3eecc61706b3d9e")
        identity={{"id","runtime.dolphin.melee-us-rev1.v1"},{"label","Super Smash Bros. Melee"},
            {"content",content},{"runtime",runtime},{"settings","dolphin-native-isolated-v1"},
            {"players",4},{"lateJoin",false}};
    else identity={{"id","runtime.dolphin.gc.v1"},{"label",r.adventure.title.left(96)},
        {"content",content},{"runtime",runtime},{"settings","dolphin-gc-pads-isolated-v1"},
        {"players",players},{"lateJoin",false}};
    identity["temporaryProgress"]=true;
    identity["runtimePlayers"]=4; // Native pads/remotes, independent of catalogue completeness.
    if(!wii&&m["hotseat"]=="dolphin-golf-p1-v1")
        identity["controllerModes"]=QJsonArray{"separate","pass-p1"};
    return identity;
}
QString configureNetplay(ProcessCommand& cmd,const StandaloneInstallation& i,const QString& content,
                         const NetplayRequest& request,const std::atomic_bool& cancel) {
    if(cancel)return "Opening cancelled.";
    if(!QRegularExpression("^[a-f0-9]{32}$").match(request.token).hasMatch()||
       request.port<1||request.port>65535||request.slot<1||request.slot>4||request.host!=(request.slot==1))
        return "This game invitation is invalid.";
    QSet<int> seats;
    for(const auto& v:request.seats){const int seat=v.toInt();if(seat<1||seat>4||seats.contains(seat))return "This game party is invalid.";seats.insert(seat);}
    if(seats.size()<2||!seats.contains(1)||!seats.contains(request.slot))return "This game party is invalid.";
    const bool hotseat=usesSharedController(request.expected);
    const bool wii=request.expected["id"]=="runtime.dolphin.wii.v1";
    if(wii&&hotseat)return "Wii Remote play needs separate controllers.";
    if(hotseat&&(seats.size()!=2||!seats.contains(2)))return "This controller mode needs two players.";
    if(!request.host&&(request.online?!QRegularExpression("^[a-zA-Z0-9]{8}$").match(request.address).hasMatch():
        QHostAddress(request.address).protocol()!=QAbstractSocket::IPv4Protocol))return "This multiplayer address is invalid.";
    auto directory=std::make_shared<QTemporaryDir>();
    if(!directory->isValid())return "Couldn't prepare multiplayer settings.";
    const auto root=directory->path();
    if(!prepareNetplayCache(root+"/Cache/Shaders","dolphin",request.expected["runtime"].toString()))
        return "Couldn't prepare the graphics cache.";
    QDir().mkpath(root+"/Config");
    // Copy only input mapping. All NetPlay/game settings are session-owned;
    // no memory-card path, per-game override or ordinary save is writable here.
    const QString inputFile=wii?"WiimoteNew.ini":"GCPadNew.ini";
    const auto source=i.configFile.isEmpty()?QDir::homePath()+"/.var/app/org.DolphinEmu.dolphin-emu/config/dolphin-emu/"+inputFile:QFileInfo(i.configFile).dir().filePath(inputFile);
    if(QFileInfo(source).size()>1024*1024||!QFile::copy(source,root+"/Config/"+inputFile))return "Couldn't read your controller settings.";
    const auto write=[](const QString& path,const QByteArray& bytes){QSaveFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size()&&f.commit();};
    QByteArray config="[Interface]\nConfirmStop = False\nOnScreenDisplayMessages = False\n[Display]\nFullscreen = True\nRenderToMain = False\n"
        "[Core]\nCPUThread = False\nDSPHLE = True\nGFXBackend = Vulkan\nEnableCheats = False\n"
        "SIDevice0 = 6\nSIDevice1 = 6\nSIDevice2 = 6\nSIDevice3 = 6\nSkipIPL = True\n"
        "[Analytics]\nPermissionAsked = True\nEnabled = False\n[NetPlay]\nTraversalChoice = "+QByteArray(request.online?"traversal":"direct")+
        "\nHostPort = 2626\nListenPort = 2626\nUseUPNP = False\nUseIndex = False\nSyncSaves = False\nWriteSaveData = False\n"
        "SyncCodes = False\nRecordInputs = False\nStrictSettingsSync = True\nNetworkMode = fixeddelay\nBufferSize = 5\n"
        "Address = "+request.address.toUtf8()+"\nHostCode = "+request.address.toUtf8()+"\nConnectPort = "+QByteArray::number(request.port)+"\n";
    QJsonObject session{{"protocol",1},{"game",content},{"host",request.host},{"token",request.token},{"slot",request.slot},{"slots",request.seats}};
    if(hotseat)session["hotseat"]=true;
    if(wii)session["controllers"]="wiimote";
    if(!write(root+"/Config/Dolphin.ini",config)||!write(root+"/Config/GFX.ini","[Settings]\nInternalResolution = 1\nAspectRatio = 0\nShaderCompilationMode = 0\n")||
       !write(root+"/session.json",QJsonDocument(session).toJson(QJsonDocument::Compact)))return "Couldn't prepare multiplayer settings.";
    // Retain the installed controller bridge, replace only its emulator child.
    const int separator=i.prefixArguments.indexOf("--");
    cmd.program="/usr/bin/env";
    cmd.arguments={"TRAINEROS_DOLPHIN_SESSION="+root+"/session.json"};
    if(separator>=0) { // Flip uses the installed input bridge; Odin can use native SDL.
        cmd.arguments<<i.program;
        cmd.arguments+=i.prefixArguments.mid(0,separator+1);
    }
    // The Home overlay recognizes this explicit batch/no-second-confirmation
    // contract, including when the emulator uses an isolated user directory.
    cmd.arguments<<bridgeFile()<<"-b"<<"-C"<<"Dolphin.Interface.ConfirmStop=False"<<"-u"<<root;
    cmd.runtimeControls["netplay"]=request.expected.toVariantMap();cmd.runtimeControls["netplayHost"]=request.host;
    if(hotseat)cmd.runtimeControls["hotseat"]="dolphin-golf-p1-v1";
    cmd.runtimeControls["temporaryProgress"]=true;
    const auto settled=cmd.settled;
    cmd.settled=[settled,directory](const ProcessOutcome& outcome){if(settled)settled(outcome);directory->remove();};
    return {};
}
}
