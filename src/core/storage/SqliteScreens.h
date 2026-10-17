#pragma once
#include "core/model/ExitMedia.h"
#include <QSqlDatabase>
#include <QVariantList>

namespace trainer {
QString migrateScreens(QSqlDatabase&);
QString seedScreens(QSqlDatabase&,const QString& owner,const QString& directory,const QList<ExitMedia>&);
// Called within the caller's existing transaction/savepoint after clean exit.
QString appendScreen(QSqlDatabase&,const ExitMedia&,const QString& title,bool favorite=false,const QString& kind="exit");
QString pruneScreens(QSqlDatabase&,const QString& owner);
QString starScreen(QSqlDatabase&,const QString& owner,const QString& id,bool favorite);
QVariantList readScreens(QSqlDatabase&,const QString& owner);
QImage readScreenImage(const QString& database,const QString& owner,const QString& id);
}
