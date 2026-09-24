#include "GameProgressService.h"
#include "Gen3Progress.h"
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>

namespace trainer {
namespace {
GameProgress unavailable(ProgressAvailability state, const QString& message) {
    GameProgress result; result.availability = state; result.message = message; return result;
}
QByteArray readSave(const QString& path) {
    const QFileInfo info(path);
    if (!info.isFile() || info.isSymLink() || info.size() != 0x20000) return {};
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.read(0x20001) : QByteArray{};
}
}
GameProgress inspectGameProgress(const AdventureRegistration& record, const ProgressSaveResolver& resolve) {
    const auto unsupported = unavailable(ProgressAvailability::Unsupported, "Progress is not supported for this edition yet.");
    if (record.adventure.collectionOnly || record.adventure.kind == AdventureKind::RomHack
        || record.adventure.adapterId != "retroarch" || record.integrationConfig["core"].toString() != "mgba") return unsupported;
    const auto target = resolve(record);
    if (!target.error.isEmpty()) return unavailable(ProgressAvailability::Unreadable, "Save progress is unavailable. Close the Adventure and return here.");
    if (!target.supported) return unsupported;
    const auto edition = gen3Edition(target.contentRevision);
    if (!edition) return unsupported;
    if (!QFileInfo::exists(target.savePath)) return unavailable(ProgressAvailability::Missing, "Save in the Adventure, then return here to see your progress.");
    const auto bytes = readSave(target.savePath);
    auto result = readGen3Progress(bytes, *edition);
    // Reopen the current path so an atomic replacement during the first read is
    // detected too. Only immutable in-memory bytes reach the parser.
    if (readSave(target.savePath) != bytes)
        return unavailable(ProgressAvailability::Unreadable, "The in-game save changed while reading. Return here to refresh it.");
    const auto current = resolve(record);
    if (!current.supported || !current.error.isEmpty() || current.adventureId != record.adventure.id
        || current.savePath != target.savePath || current.contentRevision != target.contentRevision
        || current.contextRevision != target.contextRevision)
        return unavailable(ProgressAvailability::Unreadable, "The Adventure changed while checking its save. Return here to refresh it.");
    result.contentRevision = target.contentRevision;
    result.contextRevision = target.contextRevision;
    result.saveRevision = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    result.observedAt = QDateTime::currentDateTimeUtc();
    return result;
}

GameProgressService::GameProgressService(ProgressSaveResolver resolver, QObject* parent)
    : GameProgressProvider(parent), resolver_(std::move(resolver)), worker_(new QObject) {
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater); thread_.start();
}
GameProgressService::~GameProgressService() { thread_.quit(); thread_.wait(); }
void GameProgressService::invalidate() {
    ++generation_; pending_ = false; record_ = {}; snapshot_ = {}; emit changed();
}
void GameProgressService::refresh(const AdventureRegistration& record) {
    ++generation_; record_ = record; pending_ = true;
    snapshot_ = unavailable(ProgressAvailability::Checking, "Reading the last in-game save…"); emit changed();
    if (!reading_) startRead();
}
void GameProgressService::startRead() {
    reading_ = true; pending_ = false;
    const auto generation = generation_;
    QMetaObject::invokeMethod(worker_, [this, generation, record = record_] {
        auto result = inspectGameProgress(record, resolver_);
        QMetaObject::invokeMethod(this, [this, generation, result = std::move(result)] {
            reading_ = false;
            if (generation == generation_) { snapshot_ = result; emit changed(); }
            if (pending_) startRead();
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
}
