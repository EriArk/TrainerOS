#include "LibraryStorageController.h"
#include <algorithm>

namespace trainer {
LibraryStorageController::~LibraryStorageController() { if(worker_){worker_->wait();delete worker_;} }
void LibraryStorageController::begin() {
    if(busy_ || !apply)return;
    locations_=locations(root_);focus_=0;error_.clear();open_=true;emit changed();
}
void LibraryStorageController::close() { if(!busy_){open_=false;emit changed();} }
QVariantList LibraryStorageController::rows() const {
    QVariantList rows;
    for(const auto& location:locations_)rows.append(libraryLocationRow(location,root_));
    rows.append(QVariantMap{{"label","Refresh"},{"detail","Find connected storage"},{"kind","refresh"}});
    return rows;
}
void LibraryStorageController::activate(int index) {
    if(!open_ || busy_ || index<0 || index>locations_.size())return;
    if(index==locations_.size()){begin();return;}
    const auto location=locations_[index];focus_=index;busy_=true;error_.clear();emit changed();
    if(worker_){worker_->wait();delete worker_;}
    worker_=QThread::create([this,location,prepare=prepare]{
        const auto failure=prepare(location);
        QMetaObject::invokeMethod(this,[this,location,failure]{
            // Applying is synchronous on the UI thread: validate availability,
            // commit the path and update the scanner without an event-loop gap.
            error_=failure.isEmpty()?apply(location.path):failure;
            busy_=false;
            if(error_.isEmpty()){root_=location.path;open_=false;}
            emit changed();
        },Qt::QueuedConnection);
    });
    worker_->start();
}
void LibraryStorageController::dispatch(Action action) {
    if(!open_ || busy_)return;
    if(action==Action::Back)close();
    else if(action==Action::Confirm)activate(focus_);
    else if(action==Action::Up){focus_=std::max(0,focus_-1);emit changed();}
    else if(action==Action::Down){focus_=std::min(int(locations_.size()),focus_+1);emit changed();}
}
}
