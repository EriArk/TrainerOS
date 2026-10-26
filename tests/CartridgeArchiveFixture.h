#pragma once
#include <QByteArray>
#include <QList>
#include <QString>
#include <archive.h>
#include <archive_entry.h>
#ifdef MoveFile
#undef MoveFile
#endif
#include <memory>

inline QByteArray cartridgeArchive(const QString& format,const QList<QPair<QByteArray,QByteArray>>& files) {
    std::unique_ptr<archive,decltype(&archive_write_free)> writer(archive_write_new(),archive_write_free);
    if(!writer)return {};
    if((format=="7z"?archive_write_set_format_7zip(writer.get()):archive_write_set_format_zip(writer.get()))!=ARCHIVE_OK)return {};
    QByteArray bytes(4*1024*1024,Qt::Uninitialized);size_t used=0;
    if(archive_write_open_memory(writer.get(),bytes.data(),bytes.size(),&used)!=ARCHIVE_OK)return {};
    for(const auto& [name,data]:files) {
        std::unique_ptr<archive_entry,decltype(&archive_entry_free)> entry(archive_entry_new(),archive_entry_free);
        archive_entry_set_pathname(entry.get(),name.constData());archive_entry_set_size(entry.get(),data.size());
        archive_entry_set_filetype(entry.get(),AE_IFREG);archive_entry_set_perm(entry.get(),0644);
        if(archive_write_header(writer.get(),entry.get())!=ARCHIVE_OK||
           archive_write_data(writer.get(),data.data(),data.size())!=data.size())return {};
    }
    if(archive_write_close(writer.get())!=ARCHIVE_OK)return {};
    bytes.resize(used);return bytes;
}
