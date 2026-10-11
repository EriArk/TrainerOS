#pragma once
#include "platform/storage/RuntimeSaveStorage.h"
#include "platform/process/ProcessService.h"

namespace trainer::melonds {
// Called only by the admitted runtime launcher, on its preparation worker.
// directory is a private local staging folder, never a peer-supplied path.
QString prepareOwnedSession(ProcessCommand&,const AdventureRegistration&,const QString& backupRoot,
                            const SaveTargetResolver&,const QString& directory,int ownerSeat,
                            const QString& expectedContent);
}
