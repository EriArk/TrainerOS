#include "integrations/adventure/standalone/PpssppNetplay.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QTextStream>
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    if(argc!=3){QTextStream(stderr)<<"Usage: inspect_psp ROM PPSSPP-ELF\n";return 2;}
    trainer::AdventureRegistration record;
    record.adventure.platformId="psp";record.adventure.adapterId="ppsspp";
    record.contentPath=app.arguments()[1];
    trainer::StandaloneInstallation installation;
    installation.program=installation.runtimeFile=app.arguments()[2];installation.platforms={"psp"};
    std::atomic_bool cancel=false;
    const auto identity=trainer::ppsspp::netplayIdentity(record,installation,cancel);
    QTextStream(stdout)<<QJsonDocument(identity).toJson();
    return identity.isEmpty()?1:0;
}
