#include "FirstRunController.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>

namespace trainer {
namespace {
const QStringList stages{"welcome", "controls", "network", "storage", "trainer", "ready", "complete"};
QString stateFile(const QString& directory) { return QDir(directory).filePath("first-run.json"); }
}
FirstRunController::FirstRunController(QObject* parent) : QObject(parent) {}
FirstRunController::~FirstRunController() { if (worker_) { worker_->wait(); delete worker_; } }
QString FirstRunController::libraryRoot(const QString& directory, const QString& fallback) {
    if(directory.isEmpty())return fallback;
    QFile file(stateFile(directory));
    if (!file.open(QIODevice::ReadOnly) || file.size() > 32768) return readLibraryRoot(directory,fallback);
    const auto value = QJsonDocument::fromJson(file.readAll()).object();
    const auto path = value["libraryRoot"].toString();
    return readLibraryRoot(directory,value["version"].toInt() == 1 && QDir::isAbsolutePath(path) ? path : fallback);
}
void FirstRunController::configure(const QString& directory, const QString& root) {
    directory_ = directory; root_ = root; configured_ = !directory.isEmpty();
    state_ = {}; error_.clear(); loadFailed_ = false;
    QFile file(stateFile(directory));
    if (!file.exists()) return;
    if (file.open(QIODevice::ReadOnly) && file.size() <= 32768) {
        const auto doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isObject() && doc.object()["version"].toInt() == 1
            && stages.contains(doc.object()["stage"].toString())) {
            state_ = doc.object(); return;
        }
    }
    loadFailed_ = true;
    error_ = "Couldn't read your setup progress. Reconnect your storage and retry.";
}
bool FirstRunController::persist(const QJsonObject& next) {
    QSaveFile file(stateFile(directory_));
    const auto bytes = QJsonDocument(next).toJson(QJsonDocument::Compact);
    if (!QDir().mkpath(directory_) || !file.open(QIODevice::WriteOnly)
        || file.write(bytes) != bytes.size() || !file.commit()) {
        error_ = "Couldn't save your setup. Free some space and try again.";
        emit changed(); return false;
    }
    state_ = next; error_.clear(); return true;
}
bool FirstRunController::move(const QString& next) {
    auto value = state_; value["version"] = 1; value["stage"] = next;
    if (!persist(value)) return false;
    focus_ = 0; connections_ = false;
    if (next == "storage") loadLocations();
    emit changed();
    if (next == "trainer") emit trainerRequested();
    return true;
}
void FirstRunController::begin(bool hasTrainers) {
    hasTrainers_=hasTrainers;
    if (!configured_ || (!loadFailed_ && (stage() == "complete" || (state_.isEmpty() && hasTrainers)))) return;
    active_ = true;
    if (loadFailed_) { emit changed(); return; }
    // Profile creation may have committed just before the process stopped.
    // Resume at completion instead of creating a duplicate Trainer.
    if (hasTrainers && stage() == "trainer") move("ready");
    else if (stage() == "ready" && !hasTrainers) move("trainer");
    else if (state_.isEmpty()) move("welcome");
    else {
        if (stage() == "storage") loadLocations();
        emit changed();
        if (stage() == "trainer") emit trainerRequested();
    }
}
void FirstRunController::loadLocations() { locations_ = locations(root_); focus_ = 0; }
void FirstRunController::trainerOpened() {
    if (active_ && stage() == "trainer") move("ready");
}
void FirstRunController::saveTrainerDraft(const QJsonObject& draft) {
    if (!active_ || stage() != "trainer" || state_["trainerDraft"].toObject() == draft) return;
    auto next = state_; next["trainerDraft"] = draft; persist(next);
}
void FirstRunController::backFromTrainer() { if (active_ && stage() == "trainer") move("storage"); }
void FirstRunController::closeConnections() { connections_ = false; emit changed(); }
int FirstRunController::step() const { return std::max(0, int(stages.indexOf(stage()))); }
QString FirstRunController::title() const {
    if (stage() == "welcome") return "Hello, Trainer.";
    if (stage() == "controls") return "Get a feel for it.";
    if (stage() == "network") return "Stay connected.";
    if (stage() == "storage") return "Room for adventure.";
    if (stage() == "trainer") return "Meet your Trainer";
    return "Make it your journey.";
}
QString FirstRunController::description() const {
    if (stage() == "welcome") return "A few little things, and your next adventure is ready to begin.";
    if (stage() == "controls") return "Try each direction, then the two buttons.";
    if (stage() == "network") return "Connect now, or enjoy your games offline.";
    if (stage() == "storage") return "Choose a home for your games.";
    if (stage() == "ready") return "Pick an Adventure in Worlds. Your journey starts on Home.";
    return {};
}
QVariantList FirstRunController::rows() const {
    QVariantList result;
    const auto add = [&](const QString& label, const QString& detail = QString()) {
        result.append(QVariantMap{{"label", label}, {"detail", detail}});
    };
    if (loadFailed_) { add("Retry"); return result; }
    if (stage() == "welcome") add("Let's begin");
    else if (stage() == "network") { add("Wi-Fi", "Find a network"); add("Continue", "Wi-Fi can be set up later"); }
    else if (stage() == "storage") {
        for (const auto& entry : locations_)result.append(libraryLocationRow(entry,root_));
        add("Refresh", "Find connected storage");
    } else if (stage() == "ready") { add("Let's go", "Open Home"); add("Trainer PIN", "Optional"); }
    return result;
}
void FirstRunController::activate(int index) {
    if (!active_ || busy_ || connections_ || index < 0 || index >= rows().size()) return;
    if (loadFailed_) { const bool hasTrainers=hasTrainers_; configure(directory_, root_); begin(hasTrainers); return; }
    focus_ = index;
    if (stage() == "welcome") { checked_ = 0; move("controls"); }
    else if (stage() == "network") {
        if (index == 0) { connections_ = true; emit changed(); }
        else move("storage");
    } else if (stage() == "storage") {
        if (index == locations_.size()) { loadLocations(); emit changed(); return; }
        const auto location = locations_[index];
        busy_ = true; error_.clear(); emit changed();
        if (worker_) { worker_->wait(); delete worker_; }
        worker_ = QThread::create([this, location, prepare = prepare] {
            const auto error = prepare(location);
            QMetaObject::invokeMethod(this, [this, location, error] {
                busy_ = false;
                if (!error.isEmpty()) { error_ = error; emit changed(); return; }
                auto next = state_; next["libraryRoot"] = location.path;
                // Commit the selected root before changing the live scanner.
                if (!persist(next)) return;
                if (useLibraryRoot) {
                    const auto failure = useLibraryRoot(location.path);
                    if (!failure.isEmpty()) { error_ = failure; emit changed(); return; }
                }
                root_ = location.path;
                move("trainer");
            }, Qt::QueuedConnection);
        });
        worker_->start();
    } else if (stage() == "ready") {
        if(index==1) { emit protectTrainerRequested(); return; }
        auto next = state_; next.remove("trainerDraft"); next["stage"] = "complete";
        if (!persist(next)) return;
        active_ = false; emit changed(); emit finished();
    }
}
void FirstRunController::dispatch(Action action) {
    if (!active_ || busy_ || connections_) return;
    if (loadFailed_) { if (action == Action::Confirm) activate(0); return; }
    if (stage() == "controls") {
        const auto bit = action == Action::Up ? 1 : action == Action::Down ? 2 : action == Action::Left ? 4
            : action == Action::Right ? 8 : action == Action::Confirm ? 16 : action == Action::Back ? 32 : 0;
        checked_ |= bit;
        if (checked_ == 63) move("network");
        emit changed(); return;
    }
    if (action == Action::Back) {
        if (stage() == "network") { checked_ = 0; move("controls"); }
        else if (stage() == "storage") move("network");
    } else if (action == Action::Confirm) activate(focus_);
    else if (action == Action::Up) { focus_ = std::max(0, focus_ - 1); emit changed(); }
    else if (action == Action::Down) { focus_ = std::min(std::max(0, int(rows().size()) - 1), focus_ + 1); emit changed(); }
}
}
