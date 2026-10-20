#pragma once
#include <QAbstractListModel>
#include <QVariantMap>

namespace trainer {
// A persistent model: provider heartbeats must not replace the QML list.
// The local adventure key groups modes, while the network game id + mode
// still identifies each exact vote candidate.
class LobbyPickerModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY refreshed)
public:
    explicit LobbyPickerModel(QObject* parent=nullptr):QAbstractListModel(parent){}
    int rowCount(const QModelIndex& parent={}) const override {return parent.isValid()?0:rows_.size();}
    QVariant data(const QModelIndex& index,int role) const override;
    QHash<int,QByteArray> roleNames() const override {return {{Qt::UserRole,"modelData"}};}
    Q_INVOKABLE QVariantMap get(int index) const {return rows_.value(index).toMap();}
    Q_INVOKABLE int indexOf(const QString& key) const;
    void update(QVariantList rows);
signals:
    void aboutToRefresh();
    void refreshed();
private:
    QVariantList rows_;
};
}
