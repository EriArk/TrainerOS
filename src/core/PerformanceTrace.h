#pragma once
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>

namespace trainer {
// Explicit, bounded developer capture; no titles, paths, identities or save data.
// Create ~/.local/state/traineros/performance.enabled before starting the shell.
class PerformanceTrace {
    struct Metric { int count=0; double total=0, maximum=0; };
    static auto& metrics() { static QHash<QString,Metric> value; return value; }
    static auto& mutex() { static QMutex value; return value; }
public:
    static bool enabled() {
        static const bool value=QFileInfo::exists(QDir::homePath()+"/.local/state/traineros/performance.enabled");
        return value;
    }
    static void sample(const QString& name,double milliseconds) {
        if(!enabled())return;
        QMutexLocker guard(&mutex());auto& metric=metrics()[name];++metric.count;
        metric.total+=milliseconds;metric.maximum=qMax(metric.maximum,milliseconds);
    }
    static void flush(int page) {
        if(!enabled())return;
        QJsonObject data;
        { QMutexLocker guard(&mutex());for(auto it=metrics().begin();it!=metrics().end();++it)
            data[it.key()]=QJsonObject{{"n",it->count},{"totalMs",it->total},{"maxMs",it->maximum}};
          metrics().clear(); }
        QFile file(QDir::homePath()+"/.local/state/traineros/performance.jsonl");
        if(file.size()>8*1024*1024 || !file.open(QIODevice::WriteOnly|QIODevice::Append))return;
        file.write(QJsonDocument(QJsonObject{{"page",page},{"metrics",data}}).toJson(QJsonDocument::Compact)+'\n');
    }
    class Scope {
        const char* name_;QElapsedTimer timer_;
    public:
        explicit Scope(const char* name):name_(name){if(enabled())timer_.start();}
        ~Scope(){if(timer_.isValid())sample(QString::fromLatin1(name_),timer_.nsecsElapsed()/1e6);}
    };
};
}
