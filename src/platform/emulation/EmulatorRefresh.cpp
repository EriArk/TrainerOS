#include "EmulatorRefresh.h"

namespace trainer {
EmulatorRefresh::EmulatorRefresh(Reader read, QObject* parent)
    : QObject(parent), read_(std::move(read)), worker_(new QObject) {
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
    retry_.setInterval(250);
    connect(&retry_, &QTimer::timeout, this, &EmulatorRefresh::advance);
    thread_.start();
}
EmulatorRefresh::~EmulatorRefresh() { thread_.quit(); thread_.wait(); }
void EmulatorRefresh::request(const QString& root, bool scanLibrary) {
    if (root.isEmpty()) return;
    // Coalesce repeated requests; a different storage root invalidates the
    // outstanding result rather than importing from a stale root.
    if (root_ != root) { root_ = root; ++revision_; ready_.reset(); requested_ = true; }
    else if (!reading_ && !ready_) requested_ = true;
    scan_ |= scanLibrary;
    retry_.start(); advance();
}
void EmulatorRefresh::advance() {
    if (reading_) return;
    if (!idle || !idle()) {
        // An update may finish during a long Adventure. Inventory obtained
        // before it is not authoritative when the device becomes idle again.
        if (ready_) { ready_.reset(); requested_ = true; }
        return;
    }
    if (ready_) {
        auto result = std::move(*ready_); ready_.reset();
        const bool scan = scan_; scan_ = false;
        retry_.stop();
        if (apply) apply(result, scan);
        return;
    }
    if (!requested_) { retry_.stop(); return; }
    requested_ = false; reading_ = true;
    const auto root = root_; const auto revision = revision_;
    QMetaObject::invokeMethod(worker_, [this, root, revision] {
        auto result = read_(root);
        QMetaObject::invokeMethod(this, [this, revision, result = std::move(result)]() mutable {
            reading_ = false;
            if (revision == revision_) ready_ = std::move(result);
            advance();
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
}
