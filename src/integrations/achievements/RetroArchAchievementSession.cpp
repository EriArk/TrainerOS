#include "RetroArchAchievementSession.h"
#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QUuid>
#include <QRegularExpression>

namespace trainer {
namespace {
bool configPath(const QString& path) {
    return QDir::isAbsolutePath(path) && !path.contains('|') && !path.contains('\n')
        && !path.contains('\r') && !path.contains(QChar::Null);
}
}
void useRetroArchAchievementAccount(ProcessCommand& command, AchievementAccount account, bool livePresentation) {
    auto prepare = std::move(command.prepare);
    command.prepare = [prepare = std::move(prepare), account = std::move(account), livePresentation]
            (ProcessCommand& cmd, const std::atomic_bool& cancelled) -> QString {
          if (cancelled) return "Opening was cancelled.";
        if (prepare) {
            const auto error = prepare(cmd, cancelled);
            if (!error.isEmpty()) return error;
        }
        if (cancelled) return "Opening was cancelled.";
        // A synchronized guest must never award host progress to its own account.
        if(cmd.runtimeControls.contains("netplay"))return {};
        const bool connected = account.valid();
        if (!connected && (!account.username.isEmpty() || !account.token.isEmpty()))
            return "Sign in to RetroAchievements again in Settings.";
        const int baseIndex = cmd.arguments.indexOf("--config");
        const int appendIndex = cmd.arguments.indexOf("--appendconfig");
        if (baseIndex < 0 || baseIndex + 1 >= cmd.arguments.size()
                || appendIndex < 0 || appendIndex + 1 >= cmd.arguments.size()
                || cmd.arguments.count("--config") != 1 || cmd.arguments.count("--appendconfig") != 1)
            return "The game's launch settings changed.";
        const auto base = cmd.arguments[baseIndex + 1];
        const auto ordinary = cmd.arguments[appendIndex + 1];
        const auto layers=ordinary.split('|');
        if (!configPath(base) || QFileInfo(base).isSymLink() || layers.isEmpty() || layers.size()>4)
            return "Couldn't prepare the game's account settings.";
        for(const auto& layer:layers)
            if(!configPath(layer)||QFileInfo(layer).isSymLink()||!QFileInfo(layer).isFile())
                return "Couldn't prepare the game's account settings.";
        // Same directory as RetroArch's base config: available inside its Flatpak.
        // QTemporaryFile creates the file exclusively with owner-only permissions.
        auto file = std::make_shared<QTemporaryFile>(QFileInfo(base).dir().filePath(".traineros-ra-XXXXXX.cfg"));
        if (!file->open() || !file->setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner))
            return "Couldn't prepare the game's account settings. Check storage and try again.";
        QByteArray bytes = "# TrainerOS per-launch account\n"
            "config_save_on_exit = \"false\"\nauto_overrides_enable = \"false\"\n"
            "cheevos_enable = \"" + QByteArray(connected ? "true" : "false") + "\"\n"
            "cheevos_username = \"" + account.username.toUtf8() + "\"\n"
            "cheevos_token = \"" + account.token.toUtf8() + "\"\n"
            "cheevos_password = \"\"\ncheevos_custom_host = \"\"\n"
            "cheevos_hardcore_mode_enable = \"false\"\n";
        std::shared_ptr<QTemporaryDir> captures;
        bool supported=false;
        if(connected && livePresentation) {
            const int runtimeStart=cmd.arguments.indexOf("--fullscreen");
            if(runtimeStart>=0) {
                QProcess version;version.setProcessChannelMode(QProcess::MergedChannels);
                version.start(cmd.program,cmd.arguments.mid(0,runtimeStart)+QStringList{"--version"});
                if(version.waitForFinished(2500)) {
                    const auto text=QString::fromUtf8(version.read(4096));
                    supported=version.exitCode()==0 && text.startsWith("RetroArch")
                        && QRegularExpression("(?:Version: |RetroArch )1\\.22\\.2\\b").match(text).hasMatch();
                }
                else {version.kill();version.waitForFinished(300);}
            }
        }
        if (connected && livePresentation && supported && !cancelled) {
            captures = std::make_shared<QTemporaryDir>(QFileInfo(base).dir().filePath(".traineros-moments-XXXXXX"));
            if (!captures->isValid() || !configPath(captures->path()) || captures->path().contains('"'))
                return "Couldn't prepare achievement photos. Check storage and try again.";
            bytes += "cheevos_auto_screenshot = \"true\"\ncheevos_visibility_unlock = \"false\"\n"
                "cheevos_unlock_sound_enable = \"false\"\nlog_verbosity = \"true\"\nlog_to_file = \"false\"\n"
                "screenshot_directory = \"" + captures->path().toUtf8() + "\"\n";
            if (!cmd.arguments.contains("--verbose") && !cmd.arguments.contains("-v")) cmd.arguments.append("--verbose");
            cmd.runtimeControls["achievements"] = QVariantMap{{"directory",captures->path()},
                {"account",account.username},{"session",QUuid::createUuid().toString(QUuid::WithoutBraces)}};
        }
        if (file->write(bytes) != bytes.size() || !file->flush())
            return "Couldn't prepare the game's account settings. Check storage and try again.";
        const auto path = file->fileName();
        file->close();
        if (cancelled) return "Opening was cancelled.";
        // RetroArch accepts a | separated list in ONE --appendconfig argument.
        // Keep the owned/moved save configuration and append only account settings.
        cmd.arguments[appendIndex + 1] = ordinary + "|" + path;
        auto settled = std::move(cmd.settled);
        cmd.settled = [file, captures, settled = std::move(settled)](const ProcessOutcome& outcome) {
            file->remove();
            if (settled) settled(outcome);
        };
        return {};
    };
}
}
