#pragma once
#include <QSqlDatabase>
#include <QString>

namespace trainer {
// A durable owner exists even before the first profile is named. It is not a
// login session or permission to activate another Trainer.
QString migrateOwnership(QSqlDatabase&);
QString localOwner(QSqlDatabase&);
QString adoptInitialOwner(QSqlDatabase&, const QString& previous, const QString& profileId);
}
