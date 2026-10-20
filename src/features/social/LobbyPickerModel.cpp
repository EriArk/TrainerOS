#include "LobbyPickerModel.h"

namespace trainer {
QVariant LobbyPickerModel::data(const QModelIndex& index,int role) const {
    return index.isValid()&&role==Qt::UserRole?rows_.value(index.row()):QVariant{};
}
int LobbyPickerModel::indexOf(const QString& key) const {
    for(int i=0;i<rows_.size();++i)if(rows_[i].toMap()["gameKey"]==key)return i;
    return -1;
}
void LobbyPickerModel::update(QVariantList rows) {
    if(rows_==rows)return;
    emit aboutToRefresh();
    for(int i=0;i<rows.size();++i) {
        const auto key=rows[i].toMap()["gameKey"].toString();
        const auto old=indexOf(key);
        if(old<0) {
            beginInsertRows({},i,i);rows_.insert(i,rows[i]);endInsertRows();
        } else {
            if(old!=i) {
                beginMoveRows({},old,old,{},old<i?i+1:i);
                rows_.move(old,i);endMoveRows();
            }
            if(rows_[i]!=rows[i]) {
                rows_[i]=rows[i];emit dataChanged(index(i),index(i),{Qt::UserRole});
            }
        }
    }
    if(rows_.size()>rows.size()) {
        beginRemoveRows({},rows.size(),rows_.size()-1);
        rows_.erase(rows_.begin()+rows.size(),rows_.end());endRemoveRows();
    }
    emit refreshed();
}
}
