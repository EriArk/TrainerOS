#include "PokedexRepository.h"
#include "core/input/TextEntryController.h"
#include <algorithm>

namespace trainer {
QString validatePokedexRecord(const QString& id, const PokedexProgress& record) {
    if (id.isEmpty() || id.size()>128 || record.revision<0) return "Choose a valid Pokédex entry.";
    if ((record.caught.value_or(false) && record.seen!=std::optional<bool>(true))
        || (record.seen==std::optional<bool>(false) && record.caught!=std::optional<bool>(false)))
        return "A caught Pokémon must be seen; an unseen Pokémon cannot be caught.";
    if (TextEntryController::characterCount(record.notes)>160 || std::any_of(record.notes.begin(),record.notes.end(),[](QChar c){
        return c.category()==QChar::Other_Control || c.category()==QChar::Separator_Line || c.category()==QChar::Separator_Paragraph;
    })) return "Keep your field note to one line of up to 160 characters.";
    return {};
}
void MockPokedexRepository::saveRecordAsync(const QString& id, const PokedexProgress& candidate, QObject*, std::function<void(PokedexWriteResult)> completed) {
    const auto error=validatePokedexRecord(id,candidate);
    if (!error.isEmpty()) { completed({false,error});return; }
    if (failWrite_) { failWrite_=false;completed({false,"Couldn't save your field note. Try again."});return; }
    if (std::none_of(catalog_.entries.begin(),catalog_.entries.end(),[&](const auto& e){return e.id==id;}) || progress(id).revision!=candidate.revision) {
        completed({false,"This record changed. Reopen it before editing."});return;
    }
    auto saved=candidate;saved.favorite=progress(id).favorite;++saved.revision;progress_[id]=saved;completed({true,{},saved.revision});
}
}
