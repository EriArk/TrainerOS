#pragma once
#include <QJsonObject>
#include <QVariantList>
namespace trainer {struct ProcessCommand;}
namespace trainer::pc {
// Discovery is read-only. Never execute an unknown runner to identify it.
QVariantList runtimes(const QString& state, const QString& home, const QString& architecture);
QString runtimeIssue(const QJsonObject& config, const QString& architecture);
bool nativeElf(const QString& path, const QString& architecture);
void captureFailure(ProcessCommand& command);
}
