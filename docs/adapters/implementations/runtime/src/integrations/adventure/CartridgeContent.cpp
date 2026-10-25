#include "CartridgeContent.h"
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <archive.h>
#include <archive_entry.h>
#include <memory>

namespace trainer::cartridge {
bool archived(const QString& path) {
    return QStringList{"zip","7z"}.contains(QFileInfo(path).suffix().toLower());
}
QString Image::digest() const {
    return bytes.isEmpty()?QString():QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
}
Image read(const QString& path,const QString& platform,const std::atomic_bool& cancel) {
    const QStringList extensions=platform=="gba"?QStringList{"gba"}:
        platform=="gb"||platform=="gbc"?QStringList{"gb","gbc"}:
        platform=="n64"?QStringList{"z64","v64","n64"}:QStringList{};
    const qint64 limit=(platform=="gba"?32:platform=="n64"?64:8)*1024LL*1024;
    const qint64 inputLimit=archived(path)?limit+16*1024*1024:limit;
    const auto extension=QFileInfo(path).suffix().toLower();
    if(cancel||extensions.isEmpty()||(!archived(path)&&!extensions.contains(extension)))return {};
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)||file.size()<=0||file.size()>inputLimit)return {};
    QByteArray input;
    while(!file.atEnd()) {
        if(cancel)return {};
        const auto block=file.read(65536);if(block.isEmpty())return {};
        input+=block;
        if(input.size()>inputLimit)return {};
    }
    if(file.error()!=QFile::NoError)return {};
    if(!archived(path))return {input,extension};
    std::unique_ptr<archive,decltype(&archive_read_free)> reader(archive_read_new(),archive_read_free);
    if(!reader)return {};
    archive_read_support_filter_none(reader.get());
    if(extension=="zip")archive_read_support_format_zip(reader.get());
    else archive_read_support_format_7zip(reader.get());
    if(archive_read_open_memory(reader.get(),input.constData(),input.size())!=ARCHIVE_OK)return {};
    Image result;QSet<QString> names;int entries=0;quint64 total=0;
    QElapsedTimer timer;timer.start();archive_entry* entry=nullptr;
    for(;;) {
        if(cancel||timer.elapsed()>15000)return {};
        const int status=archive_read_next_header(reader.get(),&entry);
        if(status==ARCHIVE_EOF)break;
        if(status!=ARCHIVE_OK||++entries>256||archive_entry_is_encrypted(entry)||
           archive_entry_symlink(entry)||archive_entry_hardlink(entry))return {};
        const auto rawName=archive_entry_pathname_utf8(entry);
        if(!rawName)return {};
        const auto name=QString::fromUtf8(rawName);
        const auto parts=name.split('/');
        if(name.isEmpty()||name.size()>1024||name.startsWith('/')||name.contains('\\')||name.contains(':')||
           parts.contains("..")||parts.contains(".")||names.contains(name))return {};
        names.insert(name);
        if(archive_entry_filetype(entry)==AE_IFDIR)continue;
        if(archive_entry_filetype(entry)!=AE_IFREG||!archive_entry_size_is_set(entry))return {};
        const auto size=archive_entry_size(entry);
        if(size<0||quint64(size)>128*1024*1024-total)return {};
        total+=size;
        const auto suffix=QFileInfo(name).suffix().toLower();
        const bool selected=extensions.contains(suffix);
        if(selected&&(!result.bytes.isEmpty()||size<=0||size>limit))return {};
        QByteArray payload;char block[65536];qint64 read=0;
        for(;;) {
            if(cancel||timer.elapsed()>15000)return {};
            const auto count=archive_read_data(reader.get(),block,sizeof(block));
            if(count<0)return {};
            if(!count)break;
            read+=count;if(read>size)return {};
            if(selected)payload.append(block,count);
        }
        if(read!=size)return {};
        if(selected)result={payload,suffix};
    }
    if(cancel||archive_read_has_encrypted_entries(reader.get())>0)return {};
    return result;
}
QString stage(const Image& image,const QString& original,const QString& directory,const QString& expected) {
    if(image.bytes.isEmpty()||image.digest()!=expected||!QDir().mkpath(directory))return {};
    const auto name=QFileInfo(original).completeBaseName();
    if(name.isEmpty()||name=="."||name=="..")return {};
    const auto target=QDir(directory).filePath(name+'.'+image.extension);
    QFile file(target);
    if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly)||file.write(image.bytes)!=image.bytes.size()||!file.flush())return {};
    return target;
}
}
