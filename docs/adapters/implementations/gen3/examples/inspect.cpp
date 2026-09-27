#include "integrations/progress/Gen3Progress.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

// Read-only example. Profile identity is verified against the actual full ROM.
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    QTextStream out(stdout), err(stderr);
    if (args.size() != 4) {
        err << "Usage: inspect_game profile.json game.gba ordinary-save.sav\n";
        return 2;
    }
    QFile profileFile(args[1]), rom(args[2]), save(args[3]);
    if (!profileFile.open(QIODevice::ReadOnly) || !rom.open(QIODevice::ReadOnly) || !save.open(QIODevice::ReadOnly)) {
        err << "Cannot read the supplied inputs.\n"; return 2;
    }
    const auto profile = QJsonDocument::fromJson(profileFile.readAll()).object();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&rom)) return 2;
    const QString digest = QString::fromLatin1(hash.result().toHex());
    const auto edition = trainer::gen3Edition(digest);
    const bool emerald = edition && *edition == trainer::Gen3Edition::Emerald;
    if (!edition || digest != profile.value("romSha256").toString()
        || rom.size() != profile.value("romBytes").toInteger()
        || save.size() != profile.value("saveBytes").toInteger()
        || profile.value("game").toString() != (emerald ? "emerald" : "firered")) {
        err << "Inputs do not match the exact game profile.\n"; return 3;
    }
    const auto result = trainer::readGen3Progress(save.readAll(), *edition);
    if (result.availability != trainer::ProgressAvailability::Available || !result.party) {
        err << "Save rejected: " << result.message << '\n'; return 4;
    }
    out << "Profile: " << profile.value("id").toString()
        << "\nParty: " << result.party->party.size() << '\n';
    return 0;
}
