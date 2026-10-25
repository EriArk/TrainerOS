#include "Core/Loaders.h"
#include "Core/FileSystems/BlockDevices.h"
#include "Core/ELF/PBPReader.h"
#include "integrations/adventure/standalone/PspContentIdentity.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>

namespace {
constexpr quint64 Limit=2ULL*1024*1024*1024;
class LocalReader final : public FileLoader {
public:
    QFile file;
    bool failed=false;
    explicit LocalReader(const QString& path):file(path){failed=!file.open(QIODevice::ReadOnly);}
    bool Exists() override{return file.isOpen();}
    bool IsDirectory() override{return false;}
    s64 FileSize() override{return file.size();}
    Path GetPath() const override{return {};}
    std::string GetFileExtension() const override{return {};}
    size_t ReadAt(s64 offset,size_t bytes,size_t count,void* data,Flags=Flags::NONE) override {
        if(!bytes||count>8*1024*1024/bytes||offset<0||offset>file.size()||
           bytes*count>quint64(file.size()-offset)||!file.seek(offset)){failed=true;return 0;}
        const auto read=file.read(static_cast<char*>(data),bytes*count);
        if(read!=qint64(bytes*count)){failed=true;return 0;}
        return count;
    }
    QByteArray read(quint64 offset,quint64 size) {
        if(size>8*1024*1024)return {};
        QByteArray result(size,Qt::Uninitialized);
        return ReadAt(offset,1,size,result.data())==size?result:QByteArray{};
    }
};
quint32 le32(const QByteArray& b,int p){return qFromLittleEndian<quint32>(b.constData()+p);}
QString cso(LocalReader& file) {
    const auto h=file.read(0,24);if(h.size()!=24||h.first(4)!="CISO"||quint8(h[20])>1)return {};
    const auto total=qFromLittleEndian<quint64>(h.constData()+8);
    const quint32 frame=le32(h,16),shift=quint8(h[21]);
    // Bound the upstream allocations/shifts and validate index spans before the
    // native reader sees them. Decompression itself belongs to PPSSPP/zlib.
    if(total<18*2048||total>Limit||total%2048||frame<2048||frame>1024*1024||
       (frame&(frame-1))||shift>20)return {};
    const auto frames=(total+frame-1)/frame;
    const auto index=file.read(24,(frames+1)*4);if(index.size()!=qint64((frames+1)*4))return {};
    const quint64 dataStart=24+index.size();
    for(quint64 n=0;n<frames;++n){
        const auto entry=le32(index,n*4);
        const quint64 from=quint64(entry&0x7fffffff)<<shift,to=quint64(le32(index,(n+1)*4)&0x7fffffff)<<shift;
        if(from<dataStart||to<=from||to>quint64(file.FileSize())||to-from>quint64(frame)+(1ULL<<shift)||
           ((entry&0x80000000)&&to-from<qMin<quint64>(frame,total-n*frame)))return {};
    }
    CISOFileBlockDevice native(&file);if(file.failed||!native.IsOK()||native.GetUncompressedSize()!=total)return {};
    const auto read=[&](quint64 offset,quint64 bytes)->QByteArray {
        if(bytes>1024*1024||offset>total||bytes>total-offset)return {};
        QByteArray result;result.reserve(bytes);quint8 block[2048];
        while(bytes){
            if(!native.ReadBlock(offset/2048,block)||file.failed||!native.IsOK())return {};
            const auto within=offset%2048,take=qMin<quint64>(bytes,2048-within);
            result.append(reinterpret_cast<char*>(block)+within,take);offset+=take;bytes-=take;
        }
        return result;
    };
    return trainer::ppsspp::content::umdDiscId(total,read);
}
QString pbp(LocalReader& file) {
    const auto h=file.read(0,40);if(h.size()!=40||h.first(4)!=QByteArray("\0PBP",4))return {};
    quint64 previous=40;
    for(int p=8;p<40;p+=4){const auto next=le32(h,p);if(next<previous||next>quint64(file.FileSize()))return {};previous=next;}
    const auto sfoSize=quint64(le32(h,12))-le32(h,8);
    if(sfoSize<20||sfoSize>1024*1024)return {};
    const auto executable=le32(h,32),psar=le32(h,36);
    const bool umd=quint64(file.FileSize())-psar>=256&&file.read(psar,8)=="NPUMDIMG";
    const auto magic=psar-executable>=4?file.read(executable,4):QByteArray{};
    if(!umd&&magic!=QByteArray("\x7f" "ELF",4)&&magic!="~PSP")return {};
    if(quint64(file.FileSize())-psar>=12&&file.read(psar,12)=="PSISOIMG0000")return {};
    PBPReader native(&file);std::vector<u8> sfo;
    if(!native.IsValid()||!native.GetSubFile(PBP_PARAM_SFO,&sfo)||file.failed||sfo.size()!=sfoSize)return {};
    return trainer::ppsspp::content::sfoDiscId(QByteArray(reinterpret_cast<const char*>(sfo.data()),sfo.size()),false);
}
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);const auto args=app.arguments();
    if(args.size()!=3||args[1]!="--psp-content"||!QFileInfo(args[2]).isAbsolute())return 2;
    LocalReader file(args[2]);if(!file.Exists()||file.FileSize()<24||quint64(file.FileSize())>Limit)return 3;
    const auto extension=QFileInfo(args[2]).suffix().toLower();
    const auto disc=extension=="cso"?cso(file):extension=="pbp"?pbp(file):QString{};
    if(file.failed||disc.isEmpty())return 4;
    const auto json=QJsonDocument(QJsonObject{{"protocol",1},{"disc",disc},{"format",extension}}).toJson(QJsonDocument::Compact);
    return std::fwrite(json.constData(),1,json.size(),stdout)==size_t(json.size())?0:5;
}
