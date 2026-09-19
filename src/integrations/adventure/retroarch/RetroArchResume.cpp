#include "RetroArchResume.h"
#include "RetroArchConfiguration.h"
#include <QCryptographicHash>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>
#include <algorithm>

namespace trainer {
using namespace retroarch;
namespace {
constexpr qint64 StateLimit = 16 * 1024 * 1024;
QString digest(const QByteArray& bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
QString integrationRevision(const AdventureRegistration& r, const RetroArchInstallation& i,
                            const Settings& s, const std::atomic_bool& cancel) {
    if (r.adventure.adapterId != "retroarch" || r.integrationConfig.value("core").toString() != "mgba"
        || !safePath(i.resumeDirectory) || !safePath(i.runtimeFile) || s.isEmpty()) return {};
    QJsonArray parts{QString("mgba-entry-v1"), i.program, i.resumeDirectory,
        QJsonArray::fromStringList(i.prefixArguments), r.integrationConfig};
    const auto paths = contextFiles(r, i, s);
    for (int n = 0; n < paths.size(); ++n) {
        const auto& path = paths[n];
        if (!safePath(path)) return {};
        const bool exists = QFileInfo::exists(path);
        const auto hash = exists ? fileDigest(path, n == 3 ? 64 * 1024 * 1024 : 128 * 1024 * 1024, cancel) : QString("absent");
        if (hash.isEmpty() || (n < 4 && !exists)) return {};
        parts.append(path); parts.append(hash);
    }
    return digest(QJsonDocument(parts).toJson(QJsonDocument::Compact));
}
QString ownedRoot(const AdventureRegistration& r, const RetroArchInstallation& i) {
    return QDir(i.resumeDirectory).filePath(digest(r.adventure.id.toUtf8()));
}
QString nativeDirectory(const AdventureRegistration& r, const RetroArchInstallation& i, const Settings& s) {
    const QFileInfo content(r.contentPath);
    if (enabled(s, "savestates_in_content_dir")) return content.absolutePath();
    QString path = configuredPath(s, "savestate_directory");
    if (!safePath(path)) return {};
    if (enabled(s, "sort_savestates_by_content_enable")) path = QDir(path).filePath(content.dir().dirName());
    if (enabled(s, "sort_savestates_enable")) path = QDir(path).filePath("mGBA");
    Q_UNUSED(i);
    return path;
}
bool writeNew(const QString& path, const QByteArray& bytes) {
    QFile f(path);
    return f.open(QIODevice::WriteOnly | QIODevice::NewOnly) && f.write(bytes) == bytes.size() && f.flush();
}
QByteArray setting(const QString& key, const QString& value) { return (key + " = \"" + value + "\"\n").toUtf8(); }
QString makeSession(const AdventureRegistration& r, const RetroArchInstallation& i) {
    if (!safePath(i.resumeDirectory)) return {};
    const auto path = QDir(ownedRoot(r, i)).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces));
    return QDir().mkpath(path) ? path : QString();
}
QByteArray launchSettings(const QString& session, bool saveOnExit = true) {
    return setting("savestate_directory", session) + setting("sort_savestates_enable", "false")
        + setting("sort_savestates_by_content_enable", "false") + setting("savestates_in_content_dir", "false")
        + setting("savestate_auto_save", saveOnExit ? "true" : "false") + setting("savestate_auto_load", "false")
        + setting("savestate_thumbnail_enable", saveOnExit ? "true" : "false") + setting("config_save_on_exit", "false")
        + setting("auto_overrides_enable", "false");
}
QString prepareSession(ProcessCommand& command, const AdventureRegistration& r, const QString& session, bool saveOnExit = true) {
    const auto config = QDir(session).filePath("launch.cfg");
    if (!writeNew(config, launchSettings(session, saveOnExit))) return "Couldn't prepare the Adventure. Your existing saves are unchanged.";
    // Insert before the literal content argument, never concatenate shell text.
    command.arguments.removeLast();
    command.arguments << "--appendconfig" << config << r.contentPath;
    return {};
}
}

RetroArchResumeSnapshot scanRetroArchMoments(const QList<AdventureRegistration>& records,
    const RetroArchInstallation& installation, const QList<ResumePoint>& previous, const std::atomic_bool& cancel) {
    RetroArchResumeSnapshot result;
    QSet<QString> scanned;
    const auto settings = readSettings(installation.configFile);
    const auto observed = QDateTime::currentDateTimeUtc();
    for (const auto& r : records.mid(0, 16)) {
        if (cancel) break;
        const auto revision = integrationRevision(r, installation, settings, cancel);
        scanned.insert(r.adventure.id);
        const auto native = nativeDirectory(r, installation, settings);
        QStringList directories;
        if (!native.isEmpty()) directories << native;
        if (safePath(installation.resumeDirectory)) {
            // One bounded directory level, never a recursive content-tree walk.
            QDirIterator sessions(ownedRoot(r, installation), QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
            QList<QFileInfo> sessionFolders;
            int count = 0;
            while (sessions.hasNext() && count++ < 4096 && !cancel) { sessions.next(); sessionFolders.append(sessions.fileInfo()); }
            std::stable_sort(sessionFolders.begin(), sessionFolders.end(), [](const auto& a, const auto& b) { return a.lastModified() > b.lastModified(); });
            for (const auto& folder : sessionFolders.mid(0, 128))
                if (QFileInfo::exists(QDir(folder.absoluteFilePath()).filePath("launch.cfg"))) directories << folder.absoluteFilePath();
        }
        QList<QFileInfo> candidates;
        const auto stem = QFileInfo(r.contentPath).completeBaseName();
        const QRegularExpression stateName("^" + QRegularExpression::escape(stem) + "\\.state(?:[0-9]{1,3}|\\.auto)?$");
        for (const auto& directory : directories) {
            QDirIterator files(directory, {stem + ".state*"}, QDir::Files | QDir::NoSymLinks);
            int visited = 0;
            while (files.hasNext() && visited++ < 128 && !cancel) {
                files.next();
                if (stateName.match(files.fileName()).hasMatch()) candidates.append(files.fileInfo());
            }
        }
        std::stable_sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) { return a.lastModified() > b.lastModified(); });
        QSet<QString> found;
        for (const auto& file : candidates.mid(0, 12)) {
            const auto relative = file.absolutePath() == native ? "native/" + file.fileName()
                : QDir(ownedRoot(r, installation)).relativeFilePath(file.absoluteFilePath());
            ResumePoint point;
            point.id = "ra-" + digest((r.adventure.id + "/" + relative).toUtf8());
            point.adventureId = r.adventure.id; point.savedAt = file.lastModified().toUTC();
            point.observedAt = observed;
            point.summary = file.fileName().endsWith(".auto") ? "Moment saved on exit" : "Saved moment";
            const auto hash = fileDigest(file.absoluteFilePath(), StateLimit, cancel);
            point.source = {"retroarch", point.id, hash, revision};
            point.availability = hash.isEmpty() || revision.isEmpty() ? ResumeAvailability::Stale
                : supportedConfiguration(r, installation, settings) ? ResumeAvailability::Exact : ResumeAvailability::LaunchOnly;
            point.adapterPayload = {{"path", file.absoluteFilePath()}};
            // A thumbnail is an optional observation paired with this state's
            // revision. It never provides location, progress or resume evidence.
            const auto png = file.absoluteFilePath() + ".png";
            const auto pngHash = fileDigest(png, 2 * 1024 * 1024, cancel);
            if (!hash.isEmpty() && !pngHash.isEmpty()) {
                QImageReader reader(png);
                const auto size = reader.size();
                if (size.isValid() && size.width() <= 4096 && size.height() <= 4096) {
                    reader.setScaledSize(size.scaled(320, 240, Qt::KeepAspectRatio));
                    const auto image = reader.read();
                    if (!image.isNull() && fileDigest(file.absoluteFilePath(), StateLimit, cancel) == hash) {
                        point.previewKey = digest((point.id + hash + pngHash).toUtf8());
                        result.previews.insert(point.previewKey, image);
                    }
                }
            }
            found.insert(point.id); result.points.append(point);
        }
        for (auto old : previous) if (old.adventureId == r.adventure.id && !found.contains(old.id)) {
            old.observedAt = observed; old.previewKey.clear();
            old.availability = QFileInfo::exists(old.adapterPayload["path"].toString()) ? ResumeAvailability::Stale : ResumeAvailability::Missing;
            result.points.append(old);
        }
    }
    for (const auto& old : previous) if (!scanned.contains(old.adventureId)) result.points.append(old);
    std::stable_sort(result.points.begin(), result.points.end(), [](const auto& a, const auto& b) { return a.savedAt > b.savedAt; });
    result.points = result.points.mid(0, 192);
    return result;
}

QString prepareRetroArchResume(ProcessCommand& command, const AdventureRegistration& r,
    const ResumePoint& point, const RetroArchInstallation& installation, const std::atomic_bool& cancel) {
    const auto settings = readSettings(installation.configFile);
    const auto integration = integrationRevision(r, installation, settings, cancel);
    if (cancel) return "Opening cancelled.";
    if (point.id.isEmpty() && (integration.isEmpty() || !supportedConfiguration(r, installation, settings))) return {};
    if (integration.isEmpty() || !supportedConfiguration(r, installation, settings))
        return "This play setup can't restore a saved moment. Open the Adventure normally to choose a save.";
    if (!point.id.isEmpty() && (point.adventureId != r.adventure.id || point.source.adapterId != "retroarch"
        || point.source.integrationRevision != integration || !point.source.complete()))
        return "The saved moment's play setup changed. Select it again after checking.";
    const auto session = makeSession(r, installation);
    if (session.isEmpty()) return "The save storage isn't available. Check the card and try again.";
    if (point.id.isEmpty()) return prepareSession(command, r, session, false);
    const auto source = point.adapterPayload["path"].toString();
    QFile sourceFile(source);
    if (!sourceFile.open(QIODevice::ReadOnly) || sourceFile.size() <= 0 || sourceFile.size() > StateLimit)
        return "That saved moment changed or disappeared. Select an available moment again.";
    const auto stateBytes = sourceFile.read(StateLimit + 1); sourceFile.close();
    if (cancel || stateBytes.size() > StateLimit || digest(stateBytes) != point.source.revision)
        return "That saved moment changed or disappeared. Select an available moment again.";
    const auto entry = QDir(session).filePath(QFileInfo(r.contentPath).completeBaseName() + ".state0.entry");
    if (!writeNew(entry, stateBytes) || fileDigest(entry, StateLimit, cancel) != point.source.revision)
        return "Couldn't verify a copy of the saved moment. The Adventure wasn't started.";

    // RetroArch queues state loading and can exit successfully after a failed
    // deserialize. Validate the immutable entry copy with this exact core and
    // content in an isolated, frame-limited process before exposing gameplay.
    const auto proof = QDir(session).filePath("validation");
    if (!QDir().mkpath(QDir(proof).filePath("config/mGBA")) || !QDir().mkpath(QDir(proof).filePath("saves")))
        return "Couldn't prepare the saved-moment check.";
    const auto context = contextFiles(r, installation, settings);
    for (int n = 4; n < context.size(); ++n) {
        if (!QFileInfo::exists(context[n]) || (n != 4 && !context[n].endsWith(".opt"))) continue;
        const auto destination = n == 4 ? QDir(proof).filePath("global.opt")
            : QDir(proof).filePath("config/mGBA/" + QFileInfo(context[n]).fileName());
        if (!QFile::copy(context[n], destination)) return "Couldn't copy the play settings for validation.";
    }
    // RetroArch can retain the first duplicate key. Produce one value per
    // setting so the validation child can never save a new gameplay moment.
    QByteArray config = launchSettings(session, false);
    for (const auto& pair : QList<QPair<QString, QString>>{
        {"video_driver", "null"}, {"audio_driver", "null"}, {"input_driver", "null"},
        {"audio_enable", "false"}, {"microphone_enable", "false"}, {"desktop_menu_enable", "false"},
        {"ui_companion_enable", "false"}, {"ui_companion_start_on_boot", "false"},
        {"history_list_enable", "false"}, {"cheevos_enable", "false"},
        {"content_runtime_log", "false"}, {"content_runtime_log_aggregate", "false"}, {"user_language", "0"},
        {"rgui_config_directory", QDir(proof).filePath("config")},
        {"savefile_directory", QDir(proof).filePath("saves")},
        {"core_options_path", QDir(proof).filePath("global.opt")}}) config += setting(pair.first, pair.second);
    if (safePath(configuredPath(settings, "system_directory"))) config += setting("system_directory", configuredPath(settings, "system_directory"));
    config += setting("global_core_options", settings.value("global_core_options", "false"));
    const auto proofConfig = QDir(proof).filePath("check.cfg");
    if (!writeNew(proofConfig, config)) return "Couldn't write the saved-moment check settings.";
    QProcess check;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("QT_QPA_PLATFORM", "offscreen"); check.setProcessEnvironment(environment);
    check.setProcessChannelMode(QProcess::MergedChannels);
    check.start(installation.program, installation.prefixArguments + QStringList{"--verbose", "--config", proofConfig,
        "--libretro", installation.cores.value("mgba"), "--entryslot", "0", "--max-frames", "60", r.contentPath});
    QByteArray output;
    QElapsedTimer timeout; timeout.start();
    while (check.state() != QProcess::NotRunning && !cancel && timeout.elapsed() < 15000) {
        check.waitForFinished(50); output += check.readAll();
        if (output.size() > 1024 * 1024) break;
    }
    if (check.state() != QProcess::NotRunning) { check.kill(); check.waitForFinished(2000); return "The saved-moment check couldn't finish. The Adventure wasn't started."; }
    output += check.readAll();
    if (cancel || check.exitStatus() != QProcess::NormalExit || check.exitCode() != 0
        || !output.contains("[State] Loading state \"" + entry.toUtf8() + "\"")
        || !output.contains("[State] Saving state \"RAM\"") || output.contains("[ERROR] [State]"))
        return "This saved moment couldn't be restored. Your original saves are unchanged; choose another moment or open the Adventure normally.";
    if (fileDigest(entry, StateLimit, cancel) != point.source.revision
        || integrationRevision(r, installation, readSettings(installation.configFile), cancel) != integration)
        return "The saved moment or play setup changed while checking. Please select it again.";
    const auto error = prepareSession(command, r, session);
    if (!error.isEmpty()) return error;
    command.arguments.removeLast(); command.arguments << "--verbose" << "--entryslot" << "0" << r.contentPath;
    auto pending = std::make_shared<QByteArray>();
    command.inspectOutput = [pending, entryBytes = entry.toUtf8()](const QByteArray& bytes) -> QString {
        *pending += bytes;
        int end;
        while ((end = pending->indexOf('\n')) >= 0) {
            const auto line = pending->left(end); pending->remove(0, end + 1);
            if (line.contains("[ERROR] [State]") && line.contains(entryBytes))
                return "The saved moment couldn't be restored. You're back in TrainerOS; your original state is intact.";
        }
        if (pending->size() > 32768) pending->remove(0, pending->size() - 32768);
        return {};
    };
    return {};
}

RetroArchResumeProvider::RetroArchResumeProvider(LibraryRepository& repository, RetroArchInstallation installation)
    : repository_(repository), installation_(std::move(installation)), worker_(new QObject),
      cancelled_(std::make_shared<std::atomic_bool>(false)) {
    worker_->moveToThread(&thread_); connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater); thread_.start();
}
RetroArchResumeProvider::~RetroArchResumeProvider() { *cancelled_ = true; thread_.quit(); thread_.wait(); }
void RetroArchResumeProvider::refresh(const QString& id) {
    if (installation_.resumeDirectory.isEmpty()) return;
    if (!id.isEmpty()) { requested_.removeAll(id); requested_.prepend(id); requested_ = requested_.mid(0, 16); }
    ++generation_; pending_ = true;
    if (!scanning_) startScan();
}
void RetroArchResumeProvider::startScan() {
    scanning_ = true; pending_ = false;
    auto ids = requested_;
    for (const auto& session : repository_.recentSessions()) if (!ids.contains(session.adventureId) && ids.size() < 12) ids.append(session.adventureId);
    QList<AdventureRegistration> records;
    for (const auto& id : ids) if (const auto r = repository_.registration(id); r && r->integrationConfig["core"].toString() == "mgba") records.append(*r);
    const auto generation = generation_;
    QMetaObject::invokeMethod(worker_, [this, records, generation, previous = points_] {
        auto snapshot = scanRetroArchMoments(records, installation_, previous, *cancelled_);
        QMetaObject::invokeMethod(this, [this, generation, snapshot = std::move(snapshot)] {
            scanning_ = false;
            if (generation == generation_) {
                requested_.clear();
                points_ = snapshot.points;
                for (auto it = snapshot.previews.cbegin(); it != snapshot.previews.cend(); ++it) previews_.insert(it.key(), it.value());
                QSet<QString> keys; for (const auto& point : points_) keys.insert(point.previewKey);
                for (auto it = previews_.begin(); it != previews_.end();) it = keys.contains(it.key()) ? ++it : previews_.erase(it);
                emit updated(points_);
            }
            if (pending_) startScan();
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
}
