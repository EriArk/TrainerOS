#include "HallOfFameRepository.h"
#include "core/input/TextEntryController.h"
#include <algorithm>

namespace trainer {
QString validateArchiveEntry(const HallOfFameEntry& entry) {
    const auto clean = [](const QString& value, int limit) {
        return TextEntryController::characterCount(value) <= limit
            && std::none_of(value.begin(), value.end(), [](QChar c) {
                return c.category() == QChar::Other_Control || c.category() == QChar::Separator_Line || c.category() == QChar::Separator_Paragraph;
            });
    };
    if (entry.id.isEmpty() || entry.id.size() > 128 || entry.adventureId.isEmpty() || entry.adventureId.size() > 256 || entry.revision < 0)
        return "Choose an Adventure for this memory.";
    if (!clean(entry.notes, 160)) return "Keep your note to one line of up to 160 characters.";
    if (entry.playtimeMinutes && (*entry.playtimeMinutes < 0 || *entry.playtimeMinutes > 999999))
        return "Enter playtime in minutes, from 0 to 999999, or leave it blank.";
    if (entry.team.size() > 6) return "A team can contain up to six members.";
    for (const auto& member : entry.team) {
        if (!clean(member.name, 24)) return "Use up to 24 characters for each team member.";
        if (member.level && (member.name.trimmed().isEmpty() || *member.level < 1 || *member.level > 100))
            return "A level needs a team member and must be between 1 and 100.";
    }
    return {};
}
}
