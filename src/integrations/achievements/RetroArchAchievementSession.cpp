#include "RetroArchAchievementSession.h"
#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>

namespace trainer {
namespace {
bool configPath(const QString& path) {
    return QDir::isAbsolutePath(path) && !path.contains('|') && !path.contains('\n')
        && !path.contains('\r') && !path.contains(QChar::Null);
}
}
void useRetroArchAchievementAccount(ProcessCommand& command, AchievementAccount account) {
    auto prepare = std::move(command.prepare);
    command.prepare = [prepare = std::move(prepare), account = std::move(account)]
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
        const QByteArray bytes = "# TrainerOS per-launch account\n"
            "config_save_on_exit = \"false\"\nauto_overrides_enable = \"false\"\n"
            "cheevos_enable = \"" + QByteArray(connected ? "true" : "false") + "\"\n"
            "cheevos_username = \"" + account.username.toUtf8() + "\"\n"
            "cheevos_token = \"" + account.token.toUtf8() + "\"\n"
            "cheevos_password = \"\"\ncheevos_custom_host = \"\"\n"
            "cheevos_hardcore_mode_enable = \"false\"\n";
        if (file->write(bytes) != bytes.size() || !file->flush())
            return "Couldn't prepare the game's account settings. Check storage and try again.";
        const auto path = file->fileName();
        file->close();
        if (cancelled) return "Opening was cancelled.";
        // RetroArch accepts a | separated list in ONE --appendconfig argument.
        // Keep the owned/moved save configuration and append only account settings.
        cmd.arguments[appendIndex + 1] = ordinary + "|" + path;
        auto settled = std::move(cmd.settled);
        cmd.settled = [file, settled = std::move(settled)](const ProcessOutcome& outcome) {
            file->remove();
            if (settled) settled(outcome);
        };
        return {};
    };
}
}
