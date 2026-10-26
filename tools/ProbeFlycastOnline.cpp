// SPDX-License-Identifier: GPL-3.0-or-later
// Explicit integration probe: owned disc, separate diagnostic saves, normal Home exit.
#include "integrations/adventure/standalone/FlycastNetplay.h"
#include "platform/input/AdventureOverlayService.h"
#include <QGuiApplication>
#include <QFile>
#include <QJsonDocument>
#include <QTextStream>
#include <QDir>
#include <QCryptographicHash>
using namespace trainer;
int main(int argc,char** argv) {
 QGuiApplication app(argc,argv);app.setApplicationName("TrainerOSOnlineProbe");
 if(argc!=5)return 2;
 AdventureRegistration r;r.adventure.id="online-integration-probe";r.adventure.title="Owned Dreamcast Online probe";
 r.adventure.platformId="dreamcast";r.adventure.adapterId="retroarch";r.contentPath=QString::fromUtf8(argv[1]);
 const QString backups=QString::fromUtf8(argv[2]),helper=QString::fromUtf8(argv[3]),output=QString::fromUtf8(argv[4]);
 std::atomic_bool cancel=false;const auto identity=flycast::onlineIdentity(r,cancel);if(identity.isEmpty())return 3;
 ProcessService process;AdventureLaunchController launch(process);AdventureExitPresentation view(launch.exitController());
 AdventureOverlayService overlay(process,launch,view,helper);QString stage;QJsonObject seeded;bool menu=false,confirmed=false;
 QObject::connect(&launch.exitController(),&AdventureExitController::changed,&app,[&]{QTextStream(stdout)<<"EXIT_STATE "<<launch.exitController().available()<<' '<<int(launch.exitController().phase())<<Qt::endl;});
 QObject::connect(&app,&QCoreApplication::aboutToQuit,&app,[&]{QTextStream(stdout)<<"PROBE_END "<<launch.state()<<' '<<menu<<' '<<confirmed<<Qt::endl;});
 auto command=flycast::onlineCommand(r,identity,"diagnostic-owner",backups);
 const auto prepare=command.prepare;
 command.prepare=[&](ProcessCommand& out,const std::atomic_bool& stop){
  const auto error=prepare(out,stop);if(!error.isEmpty())return error;
  stage=out.workingDirectory;
  for(const auto& n:{"dc_nvmem.bin","vmu_save_A1.bin","vmu_save_A2.bin"}) {
   QFile f(stage+"/data/flycast/"+n);if(f.open(QIODevice::ReadOnly))seeded[n]=QString::fromLatin1(QCryptographicHash::hash(f.readAll(),QCryptographicHash::Sha256).toHex());
  }
  QFile cfg(stage+"/config/flycast/emu.cfg");if(!cfg.open(QIODevice::ReadOnly))return QString("No configuration");
  auto bytes=cfg.readAll();cfg.close();bytes.replace("[config]\n","[config]\naica.Volume = 0\n");
  if(!cfg.open(QIODevice::WriteOnly)||cfg.write(bytes)!=bytes.size())return QString("Cannot mute diagnostic session");
  return QString();
 };
 QObject::connect(&launch,&AdventureLaunchController::checkpointRequested,&app,[&](quint64 token){launch.checkpointCompleted(token,{});});
 QObject::connect(&process,&ProcessService::runtimeOutput,&app,[](QByteArray b){QTextStream(stdout)<<QString::fromUtf8(b)<<Qt::flush;});
 QObject::connect(&process,&ProcessService::started,&app,[&]{QTextStream(stdout)<<"NATIVE_STARTED"<<Qt::endl;QTimer::singleShot(18000,&app,[&]{overlay.openOptions();});});
 QObject::connect(&view,&AdventureExitPresentation::changed,&app,[&]{
  if(view.menuOpen()&&!menu){menu=true;view.frame().save(output+".png");QTimer::singleShot(500,&app,[&]{if(!view.exitFromMenu())app.exit(6);});}
 });
 QObject::connect(&launch.exitController(),&AdventureExitController::confirmationRequested,&app,[&]{confirmed=launch.exitController().confirm();});
 QObject::connect(&process,&ProcessService::finished,&app,[&](int code,bool crashed,const QString& error){
  const auto archive=QFileInfo(stage).dir().filePath("progress.bin");QFile file(archive);QJsonObject saved;
  if(file.open(QIODevice::ReadOnly)) {
   const auto bytes=file.readAll();const QStringList names{"dc_nvmem.bin","vmu_save_A1.bin","vmu_save_A2.bin"};
   for(int i=0;i<names.size();++i)if(bytes.size()>5+(i+1)*131073&&bytes[5+i*131073]==1)
    saved[names[i]]=QString::fromLatin1(QCryptographicHash::hash(bytes.mid(6+i*131073,131072),QCryptographicHash::Sha256).toHex());
  }
  const bool ok=code==0&&!crashed&&error.isEmpty()&&menu&&confirmed&&!saved.isEmpty();
  const QJsonObject result{{"ok",ok},{"exit",code},{"error",error},{"homeMenu",menu},{"confirmedExit",confirmed},{"product",identity["product"]},{"seeded",seeded},{"saved",saved}};
  QFile report(output+".json");if(!report.open(QIODevice::WriteOnly)){app.exit(9);return;}report.write(QJsonDocument(result).toJson());
  QTextStream(stdout)<<QJsonDocument(result).toJson(QJsonDocument::Compact)<<Qt::endl;app.exit(ok?0:7);
 });
 QTimer::singleShot(180000,&app,[&]{QTextStream(stdout)<<"PROBE_TIMEOUT"<<Qt::endl;process.stop();});
 QTimer::singleShot(195000,&app,[&]{app.exit(8);});
 if(!launch.launch(command,{},r.adventure.id,AdventureSavePolicy::ManualConfirm))return 4;
 return app.exec();
}
