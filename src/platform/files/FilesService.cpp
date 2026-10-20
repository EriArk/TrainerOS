#include "FilesService.h"
#include <QDir>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUrl>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QTimer>
#include <QUuid>
#include <QDateTime>
#include <mutex>
#include <condition_variable>
#ifdef TRAINER_WITH_LIBFM
#include <libfm-qt6/core/dirlistjob.h>
#include <libfm-qt6/core/filetransferjob.h>
#include <libfm-qt6/core/deletejob.h>
#include <libfm-qt6/core/trashjob.h>
#endif

namespace trainer {
namespace {
QString local(const QString& value) { const QUrl url(value); return url.isLocalFile()?url.toLocalFile():value; }
struct Task {
    QString id=QUuid::createUuid().toString(QUuid::WithoutBraces), operation;
    QStringList sources;
    QString destination;
    mutable std::mutex mutex;
    std::condition_variable condition;
    QString answer, error, question;
    bool cancelled=false, finished=false, visible=true;
    double progress=0;
#ifdef TRAINER_WITH_LIBFM
    Fm::Job* job=nullptr; // accessed only under mutex, never owned by the UI
#endif
    QFuture<void> future;
};
QVariantMap action(const QString& id,const QString& label) {return {{"id",id},{"label",label},{"enabled",true}};}
#ifdef TRAINER_WITH_LIBFM
Fm::FilePath fmPath(const QString& value) {return Fm::FilePath::fromPathStr(value.toUtf8().constData());}
void execute(const std::shared_ptr<Task>& task) {
    Fm::FilePathList paths;for(const auto& source:task->sources)paths.push_back(fmPath(source));
    std::unique_ptr<Fm::FileOperationJob> job;
    if(task->operation=="copy"||task->operation=="move")
        job=std::make_unique<Fm::FileTransferJob>(paths,fmPath(task->destination),task->operation=="copy"?Fm::FileTransferJob::Mode::COPY:Fm::FileTransferJob::Mode::MOVE);
    else if(task->operation=="rename")
        job=std::make_unique<Fm::FileTransferJob>(paths,Fm::FilePathList{fmPath(task->destination)},Fm::FileTransferJob::Mode::MOVE);
    else if(task->operation=="trash")job=std::make_unique<Fm::TrashJob>(paths);
    else if(task->operation=="delete")job=std::make_unique<Fm::DeleteJob>(paths);
    if(task->operation=="mkdir") {
        auto path=fmPath(task->destination);GError* error=nullptr;
        if(!g_file_make_directory(path.gfile().get(),nullptr,&error)) {
            std::lock_guard lock(task->mutex);task->error=QString::fromUtf8(error?error->message:"Cannot create folder");
        }
        if(error)g_error_free(error);
    } else if(job) {
        QObject::connect(job.get(),&Fm::Job::error,job.get(),[task](const Fm::GErrorPtr& error,Fm::Job::ErrorSeverity,Fm::Job::ErrorAction& response){
            std::lock_guard lock(task->mutex);task->error=QString::fromUtf8(error->message);response=Fm::Job::ErrorAction::ABORT;
        },Qt::DirectConnection);
        QObject::connect(job.get(),&Fm::FileOperationJob::fileExists,job.get(),[task](const Fm::FileInfo&,const Fm::FileInfo& destination,Fm::FileOperationJob::FileExistsAction& response,Fm::FilePath& renamed){
            std::unique_lock lock(task->mutex);
            task->question=destination.displayName();task->answer.clear();
            task->condition.wait(lock,[&]{return task->cancelled||!task->answer.isEmpty();});
            const auto answer=task->answer;task->question.clear();task->answer.clear();
            response=Fm::FileOperationJob::CANCEL;
            if(!task->cancelled && answer=="skip")response=Fm::FileOperationJob::SKIP;
            else if(!task->cancelled && answer=="replace")response=Fm::FileOperationJob::OVERWRITE;
            else if(!task->cancelled && answer=="keep") {
                const auto name=QString::fromUtf8(destination.name().c_str());
                const QFileInfo info(name);int n=2;
                do {
                    const auto candidate=info.completeBaseName()+QString(" (%1)").arg(n++)+(info.suffix().isEmpty()?QString():"."+info.suffix());
                    renamed=destination.path().parent().child(candidate.toUtf8().constData());
                } while(g_file_query_exists(renamed.gfile().get(),nullptr)&&n<10000);
                response=Fm::FileOperationJob::RENAME;
            }
        },Qt::DirectConnection);
        {std::lock_guard lock(task->mutex);task->job=job.get();if(task->cancelled)job->cancel();}
        job->run();
        {std::lock_guard lock(task->mutex);
            if(auto trash=dynamic_cast<Fm::TrashJob*>(job.get());trash&&!trash->unsupportedFiles().empty())task->error="This location does not support Trash. Files were not permanently deleted.";
            task->cancelled=task->cancelled||job->isCancelled();task->job=nullptr;
        }
    } else {std::lock_guard lock(task->mutex);task->error="Unsupported file operation";}
    {std::lock_guard lock(task->mutex);task->finished=true;task->progress=1;}
}
#endif
}
struct FilesService::Impl {
    QList<std::shared_ptr<Task>> jobs;
    QList<QFuture<QVariantMap>> lists;
    quint64 generation=0;
    QTimer timer;
};
FilesService::FilesService(QObject* parent):QObject(parent),impl_(std::make_unique<Impl>()) {
    impl_->timer.setInterval(250);
    connect(&impl_->timer,&QTimer::timeout,this,[this]{
        bool active=false;for(const auto& task:impl_->jobs) {std::lock_guard lock(task->mutex);active|=!task->finished;
#ifdef TRAINER_WITH_LIBFM
            if(auto op=dynamic_cast<Fm::FileOperationJob*>(task->job))task->progress=op->progress();
#endif
        }
        Q_EMIT changed();if(!active)impl_->timer.stop();
    });
}
FilesService::~FilesService(){disconnect(this,nullptr,nullptr,nullptr);cancel();for(auto& task:impl_->jobs)task->future.waitForFinished();for(auto& list:impl_->lists)list.waitForFinished();}
bool FilesService::available() const {
#ifdef TRAINER_WITH_LIBFM
    return true;
#else
    return false;
#endif
}
void FilesService::reset(){cancel();++impl_->generation;for(const auto& task:impl_->jobs){std::lock_guard lock(task->mutex);task->visible=false;}Q_EMIT changed();}
void FilesService::list(const QString& value) {
    const auto generation=++impl_->generation;
    auto* watcher=new QFutureWatcher<QVariantMap>(this);
    connect(watcher,&QFutureWatcher<QVariantMap>::finished,this,[this,watcher,generation,value]{
        const auto result=watcher->result();watcher->deleteLater();
        if(generation==impl_->generation)Q_EMIT listed(value,result["entries"].toList(),result["error"].toString());
    });
    auto future=QtConcurrent::run([value]{
        QVariantList entries;QString error;QMimeDatabase mime;
#ifdef TRAINER_WITH_LIBFM
        Fm::DirListJob job(fmPath(value), Fm::DirListJob::Flags(0));
        QObject::connect(&job,&Fm::Job::error,&job,[&](const Fm::GErrorPtr& err,Fm::Job::ErrorSeverity,Fm::Job::ErrorAction& response){error=QString::fromUtf8(err->message);response=Fm::Job::ErrorAction::ABORT;},Qt::DirectConnection);
        job.run();
        for(const auto& file:job.files()) {
            const auto url=QString::fromUtf8(file->path().uri().get());
            const auto type=file->isDir()?mime.mimeTypeForName("inode/directory"):mime.mimeTypeForFile(file->displayName(),QMimeDatabase::MatchExtension);
            entries<<QVariantMap{{"url",url},{"name",file->displayName()},{"directory",file->isDir()},{"hidden",file->isHidden()},
                {"size",QVariant::fromValue<qulonglong>(file->size())},{"modified",QDateTime::fromSecsSinceEpoch(file->mtime()).toString("dd MMM yyyy")},
                {"icon",type.iconName()},{"preview",type.name().startsWith("image/")&&QUrl(url).isLocalFile()?url:QString()}};
        }
#else
        // Read-only desktop development preview. Linux writes always use libfm.
        QDir dir(local(value));if(!dir.exists())error="Folder is not available";
        for(const auto& file:dir.entryInfoList(QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden)) {
            const auto type=mime.mimeTypeForFile(file,QMimeDatabase::MatchExtension);const auto url=QUrl::fromLocalFile(file.absoluteFilePath()).toString();
            entries<<QVariantMap{{"url",url},{"name",file.fileName()},{"directory",file.isDir()},{"hidden",file.isHidden()},
                {"size",file.size()},{"modified",file.lastModified().toString("dd MMM yyyy")},{"icon",file.isDir()?"folder":type.iconName()},{"preview",type.name().startsWith("image/")?url:QString()}};
        }
#endif
        return QVariantMap{{"entries",entries},{"error",error}};
    });
    impl_->lists.removeIf([](const auto& f){return f.isFinished();});impl_->lists<<future;watcher->setFuture(future);
}
QVariantList FilesService::places() const {
    QVariantList result;
    auto add=[&](QString name,QString path,QString icon){if(!path.isEmpty())result<<QVariantMap{{"name",name},{"url",path.contains("://")?path:QUrl::fromLocalFile(path).toString()},{"icon",icon}};};
    add("Home",QDir::homePath(),"user-home");add("Downloads",QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),"folder-download");
    add("Pictures",QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),"folder-pictures");
    add("Documents",QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),"folder-documents");
#ifdef TRAINER_WITH_LIBFM
    add("Trash","trash:///","user-trash");
    auto* monitor=g_volume_monitor_get();auto* mounts=g_volume_monitor_get_mounts(monitor);
    for(auto* item=mounts;item;item=item->next) {
        auto* mount=G_MOUNT(item->data);auto* root=g_mount_get_root(mount);auto* uri=g_file_get_uri(root);auto* name=g_mount_get_name(mount);
        add(QString::fromUtf8(name),QString::fromUtf8(uri),g_file_is_native(root)?"drive-removable-media":"network-server");
        g_free(name);g_free(uri);g_object_unref(root);g_object_unref(mount);
    }
    g_list_free(mounts);g_object_unref(monitor);
#endif
    for(const auto& volume:QStorageInfo::mountedVolumes()) {
        const auto path=volume.rootPath();if(!volume.isReady()||!volume.isValid()||path.startsWith("/proc")||path.startsWith("/sys")||path.startsWith("/dev")||path.startsWith("/run/user")||path.startsWith("/boot")||path=="/etc/hosts"||path=="/etc/hostname"||path=="/etc/resolv.conf")continue;
        const auto url=QUrl::fromLocalFile(path).toString();bool duplicate=false;for(const auto& entry:result)duplicate|=entry.toMap()["url"]==url;
        if(!duplicate)add(path=="/"?"Internal storage":volume.displayName().isEmpty()?path:volume.displayName(),path,"drive-harddisk");
    }
    return result;
}
QString FilesService::start(const QString& op,const QStringList& sources,const QString& destination) {
    if(!QStringList{"copy","move","rename","trash","delete","mkdir"}.contains(op))return {};
    auto task=std::make_shared<Task>();task->operation=op;task->sources=sources;task->destination=destination;impl_->jobs<<task;
#ifdef TRAINER_WITH_LIBFM
    task->future=QtConcurrent::run([task]{execute(task);});
#else
    task->finished=true;task->error="File operations require the Linux libfm-qt backend.";
#endif
    impl_->timer.start();Q_EMIT changed();return task->id;
}
void FilesService::cancel(const QString& id){for(const auto& task:impl_->jobs)if(id.isEmpty()||task->id==id){std::lock_guard lock(task->mutex);if(task->finished)continue;task->cancelled=true;task->question.clear();
#ifdef TRAINER_WITH_LIBFM
    if(task->job)task->job->cancel();
#endif
    task->condition.notify_all();}Q_EMIT changed();}
void FilesService::answer(const QString& id,const QString& answer){for(const auto& task:impl_->jobs)if(task->id==id){std::lock_guard lock(task->mutex);if(task->question.isEmpty()||!QStringList{"keep","replace","skip","cancel"}.contains(answer))return;task->answer=answer;task->question.clear();if(answer=="cancel")task->cancelled=true;task->condition.notify_all();}Q_EMIT changed();}
QVariantList FilesService::tasks() const {
    QVariantList result;for(const auto& task:impl_->jobs){std::lock_guard lock(task->mutex);if(!task->visible)continue;
        const auto state=task->finished?(!task->error.isEmpty()?"error":task->cancelled?"cancelled":"done"):task->question.isEmpty()?"running":"waiting";
        QVariantList actions;if(!task->finished)actions<<action("cancel","Cancel");
        if(!task->question.isEmpty())actions.prepend(action("resolve","Resolve conflict"));
        result<<QVariantMap{{"id",task->id},{"kind","file-operation"},{"title",task->operation.left(1).toUpper()+task->operation.mid(1)+" · "+(task->sources.isEmpty()?QUrl(task->destination).fileName():QString::number(task->sources.size())+" items")},
            {"subtitle",task->error.isEmpty()?task->destination:task->error},{"state",state},{"terminal",task->finished},{"progress",task->progress},{"actions",actions},{"question",task->question}};
    }return result;
}
}
