#include "ArchiveExtractor.h"
#include <archive.h>
#include <archive_entry.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStorageInfo>
#include <QRegularExpression>
#include <array>
#include <memory>
#include <algorithm>

namespace trainer {
QString ArchiveExtractor::extract(const QString& source, const QString& destination,
                                 const std::function<bool()>& cancelled,
                                 const std::function<void(double)>& progress, qint64 byteLimit) {
    const QFileInfo target(destination), input(source);
    const auto root=target.canonicalFilePath();
    if(root.isEmpty() || target.isSymLink() || !target.isDir() ||
       !QDir(root).entryList(QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden|QDir::System).isEmpty())
        return "Extraction needs a new, empty local folder.";
    if(!input.isFile() || input.isSymLink())return "Choose a local ZIP, 7Z or RAR archive.";
    QStorageInfo storage(root);
    if(!storage.isReady())return "The selected storage is unavailable.";
    byteLimit=std::min(byteLimit,storage.bytesAvailable()-64*1024*1024);
    if(byteLimit<=0)return "Not enough free space to extract this archive.";
    std::unique_ptr<archive,decltype(&archive_read_free)> reader(archive_read_new(),archive_read_free);
    auto* a=reader.get();
    archive_read_support_filter_all(a);
    archive_read_support_format_zip(a);archive_read_support_format_7zip(a);
    archive_read_support_format_rar(a);archive_read_support_format_rar5(a);
#ifdef Q_OS_WIN
    const int opened=archive_read_open_filename_w(a,reinterpret_cast<const wchar_t*>(source.utf16()),128*1024);
#else
    const int opened=archive_read_open_filename(a,QFile::encodeName(source).constData(),128*1024);
#endif
    if(opened!=ARCHIVE_OK)return "Cannot read this archive: "+QString::fromUtf8(archive_error_string(a));
    QSet<QString> entries;
    qint64 written=0;int count=0;archive_entry* entry=nullptr;
    std::array<char,128*1024> buffer{};
    while(true) {
        if(cancelled())return "Extraction cancelled. Partial files were kept.";
        const int status=archive_read_next_header(a,&entry);
        if(status==ARCHIVE_EOF)break;
        if(status!=ARCHIVE_OK)return "Archive is damaged or unsupported: "+QString::fromUtf8(archive_error_string(a));
        if(++count>200000)return "Archive contains too many entries.";
        if(QFileInfo(root).isSymLink()||QFileInfo(root).canonicalFilePath()!=root)
            return "Archive destination changed during extraction.";
        if(archive_entry_is_encrypted(entry)>0)return "Encrypted archives are not supported. Extract it before importing.";
        const auto* utf8=archive_entry_pathname_utf8(entry);
        const auto* raw=archive_entry_pathname(entry);
        if(!utf8&&!raw)return "Archive entry has no file name.";
        QString relative=utf8?QString::fromUtf8(utf8):QString::fromLocal8Bit(raw);
        relative.replace('\\','/');
        while(relative.endsWith('/'))relative.chop(1);
        while(relative.startsWith("./"))relative.remove(0,2);
        const auto parts=relative.split('/');
        const auto type=archive_entry_filetype(entry);
        if(relative=="." && type==AE_IFDIR)continue;
        if(relative.isEmpty()||QDir::isAbsolutePath(relative)||relative.contains(':')||
           archive_entry_symlink(entry)||archive_entry_hardlink(entry)||
           (type!=AE_IFDIR && type!=AE_IFREG))return "Archive contains an unsafe path, link or special file.";
        // The same archive must be safe on case-insensitive storage as on Linux.
        static const QRegularExpression reserved("^(con|prn|aux|nul|com[1-9]|lpt[1-9])($|\\.)",QRegularExpression::CaseInsensitiveOption);
        for(const auto& part:parts) {
            if(part.isEmpty()||part=="."||part==".."||part.endsWith('.')||part.endsWith(' ')||
               part.contains(QRegularExpression("[<>\"|?*\\x00-\\x1f]"))||reserved.match(part).hasMatch())
                return "Archive contains a path that cannot be imported safely.";
        }
        const auto key=relative.normalized(QString::NormalizationForm_C).toCaseFolded();
        if(entries.contains(key))return "Archive contains duplicate file names.";
        entries.insert(key);
        const auto path=root+'/'+relative;
        // Check every existing ancestor before creating anything. No links are
        // accepted, including links introduced after the initial empty check.
        QString parent=root;
        for(int i=0;i<parts.size()-1;++i) {
            parent+='/'+parts[i];const QFileInfo info(parent);
            if(info.isSymLink() || (info.exists()&&!info.isDir()))return "Archive destination changed during extraction.";
            if(!QDir().mkpath(parent))return "Cannot create an archive folder.";
        }
        const QFileInfo existing(path);
        if(existing.isSymLink() || (existing.exists() && !(type==AE_IFDIR && existing.isDir())))
            return "Archive would overwrite an existing file.";
        if(type==AE_IFDIR) {if(!QDir().mkpath(path))return "Cannot create an archive folder.";continue;}
        if(archive_entry_size(entry)>byteLimit-written)return "Archive exceeds the available space or extraction limit.";
        QFile out(path);
        if(!out.open(QIODevice::WriteOnly|QIODevice::NewOnly))return "Cannot create an extracted file: "+out.errorString();
        while(true) {
            if(cancelled())return "Extraction cancelled. Partial files were kept.";
            const auto size=archive_read_data(a,buffer.data(),buffer.size());
            if(size<0)return "Cannot decode archive data: "+QString::fromUtf8(archive_error_string(a));
            if(!size)break;
            if(size>byteLimit-written)return "Archive exceeds the available space or extraction limit.";
            if(out.write(buffer.data(),size)!=size)return "Cannot write extracted data: "+out.errorString();
            written+=size;
            progress(std::clamp(double(archive_filter_bytes(a,-1))/std::max<qint64>(1,input.size()),0.0,0.99));
        }
        if(!out.flush())return "Cannot finish writing extracted data: "+out.errorString();
        auto permissions=QFile::ReadOwner|QFile::WriteOwner|QFile::ReadGroup|QFile::ReadOther;
        if(archive_entry_perm(entry)&0100)permissions|=QFile::ExeOwner|QFile::ExeGroup|QFile::ExeOther;
        out.setPermissions(permissions);
    }
    if(!count)return "This archive is empty.";
    progress(1);return {};
}
}
