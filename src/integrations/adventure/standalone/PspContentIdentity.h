#pragma once
// Bounded ISO9660/PARAM.SFO identity shared by raw UMD and the native PPSSPP reader.
// A game ID is not proof of a multiplayer mode. No executable or save is loaded.
#include <QByteArray>
#include <QMap>
#include <QRegularExpression>
#include <QtEndian>
namespace trainer::ppsspp::content {
inline QString sfoDiscId(const QByteArray& sfo,bool umd=true) {
    const auto le32=[](const QByteArray& b,int p){return qFromLittleEndian<quint32>(b.constData()+p);};
    if(sfo.size()<20||sfo.left(4)!=QByteArray::fromHex("00505346")||le32(sfo,4)!=0x101)return {};
    const quint64 keys=le32(sfo,8),data=le32(sfo,12),count=le32(sfo,16);
    if(count>256||20+count*16>keys||keys>=data||data>=quint64(sfo.size()))return {};
    QMap<QByteArray,QByteArray> fields;
    for(quint64 n=0;n<count;++n) {
        const int pos=20+n*16;
        const quint64 key=keys+qFromLittleEndian<quint16>(sfo.constData()+pos);
        const quint64 value=data+le32(sfo,pos+12),length=le32(sfo,pos+4),maximum=le32(sfo,pos+8);
        if(key>=data||value>quint64(sfo.size())||length>maximum||maximum>quint64(sfo.size())-value)return {};
        const auto end=sfo.indexOf('\0',key);
        if(end<0||quint64(end)>=data)return {};
        const auto name=sfo.mid(key,end-key);
        if(name!="DISC_ID"&&name!="CATEGORY")continue;
        if(fields.contains(name)||qFromLittleEndian<quint16>(sfo.constData()+pos+2)!=0x204||
           !length||length>32||sfo[value+length-1]!='\0')return {};
        fields[name]=sfo.mid(value,length-1);
    }
    const auto id=QString::fromLatin1(fields.value("DISC_ID"));
    const auto category=fields.value("CATEGORY");
    return (category=="UG"||(!umd&&(category=="EG"||category=="MG")))&&QRegularExpression("^[A-Z]{4}[0-9]{5}$").match(id).hasMatch()?id:QString();
}
template<class Read> QString umdDiscId(quint64 size,const Read& read) {
    if(size<18*2048||size>2ULL*1024*1024*1024||size%2048)return {};
    const auto le32=[](const QByteArray& b,int p){return qFromLittleEndian<quint32>(b.constData()+p);};
    const auto pvd=read(16*2048,2048);
    if(pvd.size()!=2048||pvd.left(7)!=QByteArray::fromHex("01434430303101")||
       qFromLittleEndian<quint16>(pvd.constData()+128)!=2048||qFromBigEndian<quint16>(pvd.constData()+130)!=2048||
       le32(pvd,80)<18||le32(pvd,80)!=qFromBigEndian<quint32>(pvd.constData()+84)||
       quint64(le32(pvd,80))*2048>size)return {};
    const auto extent=[&](const QByteArray& entry,bool directory)->QByteArray {
        if(entry.size()<34||quint8(entry[0])!=entry.size()||quint8(entry[32])+33>entry.size()||
           entry[1]!=0||entry[26]!=0||entry[27]!=0||(quint8(entry[25])&~3)||bool(entry[25]&2)!=directory||
           le32(entry,2)!=qFromBigEndian<quint32>(entry.constData()+6)||
           le32(entry,10)!=qFromBigEndian<quint32>(entry.constData()+14)||
           quint64(le32(entry,2))*2048+le32(entry,10)>quint64(le32(pvd,80))*2048)return {};
        return read(quint64(le32(entry,2))*2048,le32(entry,10));
    };
    const auto child=[&](const QByteArray& directory,const QByteArray& wanted)->QByteArray {
        QByteArray result;
        for(int p=0;p<directory.size();) {
            const int size=quint8(directory[p]);
            if(!size){p=(p/2048+1)*2048;continue;}
            if(size<34||p+size>directory.size()||p%2048+size>2048||quint8(directory[p+32])+33>size)return {};
            const auto entry=directory.mid(p,size);
            auto name=entry.mid(33,quint8(entry[32]));
            if(name.endsWith(";1"))name.chop(2);
            if(name==wanted){if(!result.isEmpty())return {};result=entry;}
            p+=size;
        }
        return result;
    };
    const auto root=extent(pvd.mid(156,quint8(pvd[156])),true);
    const auto game=extent(child(root,"PSP_GAME"),true);
    const auto sfo=extent(child(game,"PARAM.SFO"),false);
    return sfoDiscId(sfo);
}
}
