#include "features/trainer/AvatarAssets.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QTextStream>
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    const auto args=app.arguments();
    if(args.size()<4){qCritical("Usage: avatar-resolve PACK_ROOT CACHE_DIRECTORY DESCRIPTOR_JSON [APPEARANCE_JSON]");return 2;}
    auto appearance=trainer::AvatarAssets::defaults();
    if(args.size()>4){QFile f(args[4]);if(!f.open(QIODevice::ReadOnly))return 2;appearance=QJsonDocument::fromJson(f.readAll()).object();}
    auto result=trainer::AvatarAssets::resolve(appearance,args[1],args[2]);
    if(result.contains("error")){qCritical().noquote()<<result["error"].toString();return 1;}
    QFile out(args[3]);if(!out.open(QIODevice::WriteOnly))return 2;out.write(QJsonDocument(result).toJson());
    QTextStream(stdout)<<result["glb"].toString()<<Qt::endl;return 0;
}
