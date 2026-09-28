#include "LaunchPreparation.h"
#include <QFileInfo>

namespace trainer {
LaunchPreparation::~LaunchPreparation() { if(worker_){worker_->wait();delete worker_;} }
void LaunchPreparation::fail(const QString& error) {
    busy_=false;emit changed();emit messageRequested(error);
}
void LaunchPreparation::launch(const QString& id) {
    if(busy_)return;
    const auto record=library_.registration(id);
    if(!record || record->removed){fail("This game is not installed.");return;}
    auto prepared=*record;
    // Keep explicit/custom routes and all personal metadata intact.
    if(prepared.adventure.adapterId=="unconfigured")adapter_.prepareInstallation(prepared);
    busy_=true;emit changed();
    if(worker_){worker_->wait();delete worker_;}
    worker_=QThread::create([this,record=*record,prepared] {
        const QFileInfo file(record.contentPath);
        const bool available=file.isAbsolute() && file.isFile() && file.isReadable() && file.size()>0;
        const auto issue=available?adapter_.verifyInstallation(prepared):QString("The game file is missing or unreadable. Reconnect its storage.");
        QMetaObject::invokeMethod(this,[this,record,prepared,issue] {
            const auto current=library_.registration(record.adventure.id);
            if(!current || current->removed || current->revision!=record.revision || current->contentPath!=record.contentPath){
                fail("This game changed before it could open. Try again.");return;
            }
            if(!issue.isEmpty()){fail(issue);return;}
            const auto start=[this,prepared](const QString& error) {
                if(!error.isEmpty()){fail(error);return;}
                const auto current=library_.registration(prepared.adventure.id);
                if(!current || current->removed || current->contentPath!=prepared.contentPath
                        || current->adventure.adapterId!=prepared.adventure.adapterId
                        || current->integrationConfig!=prepared.integrationConfig){
                    fail("This game changed before it could open. Try again.");return;
                }
                if(!adapter_.capabilities(current->adventure).launch){
                    auto reason=adapter_.setupIssue(*current);
                    fail(reason.isEmpty()?QString("No emulator is configured for this platform."):reason);return;
                }
                // Drop the preparation lock before the existing launch lifecycle
                // takes ownership of input, checkpointing and window handoff.
                busy_=false;emit changed();
                const auto result=adapter_.launch(current->adventure);
                if(!result.inProgress && !result.message.isEmpty())emit messageRequested(result.message);
            };
            if(prepared.adventure.adapterId!=current->adventure.adapterId || prepared.integrationConfig!=current->integrationConfig)
                library_.saveAdventureAsync(prepared,this,[this,start](const LibraryWriteResult& result){
                    if(result.success)emit libraryChanged();
                    start(result.success?QString():result.error);
                });
            else start({});
        },Qt::QueuedConnection);
    });
    worker_->start();
}
}
