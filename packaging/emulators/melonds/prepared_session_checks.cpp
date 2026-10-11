// SPDX-License-Identifier: GPL-3.0-or-later
#include "PreparedSession.h"
#include <QTemporaryDir>
#include <iostream>

using namespace traineros::ds;
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
int main() {
    try {
        QTemporaryDir temporary;require(temporary.isValid(),"Temporary directory unavailable");
        QDir folder(temporary.path());
        const QByteArray seed(8192,char(0x35));const QString content(64,'a');
        QJsonObject manifest{{"version",1},{"ownerSeat",1},{"content",content},
            {"seats",QJsonArray{QJsonObject{{"fresh",true}},QJsonObject{{"bytes",8192},{"sha256",StateHash(seed)}}}}};
        auto writeManifest=[&](QJsonObject m) {
            QFile f(folder.filePath("session.json"));require(f.open(QIODevice::WriteOnly),"Manifest fixture");
            f.write(QJsonDocument(m).toJson(QJsonDocument::Compact));
        };
        auto rejected=[&](QJsonObject m,QString hash=QString(64,'a'),unsigned seats=2) {
            writeManifest(m);bool refused=false;
            try {ReadPreparedSession(temporary.path(),hash,seats);}catch(const std::exception&){refused=true;}
            require(refused,"Invalid preparation admitted");
        };
        WriteNewStagedFile(folder.filePath("seat-1.sav"),seed);
        writeManifest(manifest);
        const auto prepared=ReadPreparedSession(temporary.path(),content,2);
        require(prepared.ownerSeat==1 && !prepared.startup.seats[0]
                && prepared.startup.seats[1]==std::vector<uint8_t>(seed.begin(),seed.end()),"Startup ownership mismatch");
        auto pending=manifest;pending["version"]=2;
        pending["seats"]=QJsonArray{QJsonObject{{"peer",true}},manifest["seats"].toArray()[1]};
        rejected(pending);
        writeManifest(pending);
        const auto network=ReadPreparedSession(temporary.path(),content,2,true);
        require(network.peerPending && !network.startup.seats[0] && network.startup.seats[1]==prepared.startup.seats[1],
                "Pending peer became fresh progress or changed owner state");
        auto missing=pending;missing["seats"]=manifest["seats"];writeManifest(missing);
        bool missingRejected=false;
        try {ReadPreparedSession(temporary.path(),content,2,true);}catch(const std::exception&){missingRejected=true;}
        require(missingRejected,"Network mirror silently fell back to fresh state");
        rejected(manifest,QString(64,'b'));rejected(manifest,content,3);
        auto bad=manifest;bad["ownerSeat"]=0.5;rejected(bad);
        bad=manifest;bad["ownerSeat"]=2;rejected(bad);
        bad=manifest;bad["originalSave"]="must-not-be-used.sav";rejected(bad);
        bad=manifest;bad["seats"]=QJsonArray{QJsonObject{{"fresh",false}},QJsonObject{{"fresh",true}}};rejected(bad);
        bad=manifest;bad["seats"]=QJsonArray{QJsonObject{{"fresh",true}},QJsonObject{{"bytes",8191},{"sha256",StateHash(seed)}}};rejected(bad);
        bad=manifest;bad["seats"]=QJsonArray{QJsonObject{{"fresh",true}},QJsonObject{{"bytes",8192},{"sha256",content}}};rejected(bad);
        writeManifest(manifest);
        OwnerCartridgeReturn value{123,std::vector<uint8_t>(seed.begin(),seed.end())};
        WriteOwnerReturn(prepared,value);
        const auto returned=ReadStagedFile(folder.filePath("owner-return.sav"),8192);
        const auto receipt=QJsonDocument::fromJson(ReadStagedFile(folder.filePath("owner-return.json"),4096)).object();
        require(returned==seed && receipt["ownerSeat"]==1 && receipt["frame"]==123
                && receipt["sha256"]==StateHash(seed),"Owner receipt mismatch");
        bool overwrite=false;
        try {WriteOwnerReturn(prepared,{124,std::vector<uint8_t>(8192,0xFF)});}catch(const std::exception&){overwrite=true;}
        require(overwrite && ReadStagedFile(folder.filePath("owner-return.sav"),8192)==seed,"Repeated return overwrote progress");
        rejected(manifest);
        require(ReadStagedFile(folder.filePath("seat-1.sav"),8192)==seed,"Preparation changed initial progress");
        QTemporaryDir noSaveDirectory;require(noSaveDirectory.isValid(),"No-save fixture");
        PreparedSession noSave{QFileInfo(noSaveDirectory.path()).canonicalFilePath(),content,0,{}};
        WriteOwnerReturn(noSave,{17,{},false});
        const auto noSaveReceipt=QJsonDocument::fromJson(ReadStagedFile(noSave.directory+"/owner-return.json",4096)).object();
        require(noSaveReceipt["hasSave"]==QJsonValue(false) && noSaveReceipt["bytes"]==0
                && !QFileInfo::exists(noSave.directory+"/owner-return.sav"),"No-save cartridge fabricated progress");
        std::cout<<"PASS local staging identity, explicit fresh state, bounded reads, owner receipt, no overwrite\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
