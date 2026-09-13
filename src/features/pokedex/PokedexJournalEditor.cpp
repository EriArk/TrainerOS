#include "PokedexJournalEditor.h"
#include <algorithm>

namespace trainer {
namespace {
QString label(const std::optional<bool>& value){return !value?"Not recorded":*value?"Yes":"No";}
void cycle(std::optional<bool>& value){if(!value)value=true;else if(*value)value=false;else value.reset();}
}
QVariantList PokedexJournalEditor::fields() const {
    return {QVariantMap{{"label","SEEN"},{"value",label(draft_.seen)}},
        QVariantMap{{"label","CAUGHT"},{"value",label(draft_.caught)}},
        QVariantMap{{"label","FIELD NOTE"},{"value",draft_.notes.isEmpty()?"Add your own note…":draft_.notes}}};
}
void PokedexJournalEditor::begin(const QString& id,const QString& name) {
    if(open_||saving_||!repository_.recordsEditable()||id.isEmpty())return;
    id_=id;name_=name;draft_=repository_.progress(id);focus_=0;error_.clear();open_=true;emit changed();
}
void PokedexJournalEditor::cancel(){open_=false;error_.clear();draft_={};emit changed();}
void PokedexJournalEditor::applyNote(const QString& text){if(!open_||saving_)return;draft_.notes=text.trimmed();error_.clear();emit changed();}
void PokedexJournalEditor::activate(int index) {
    if(!open_||saving_||index<0||index>2)return;
    focus_=index;error_.clear();
    if(index==0){cycle(draft_.seen);if(draft_.seen==std::optional<bool>(false))draft_.caught=false;else if(!draft_.seen&&draft_.caught.value_or(false))draft_.caught.reset();}
    else if(index==1){cycle(draft_.caught);if(draft_.caught.value_or(false))draft_.seen=true;else if(!draft_.caught&&draft_.seen==std::optional<bool>(false))draft_.seen.reset();}
    else emit noteRequested(draft_.notes);
    emit changed();
}
void PokedexJournalEditor::submit() {
    if(!open_||saving_)return;
    error_=validatePokedexRecord(id_,draft_);if(!error_.isEmpty()){emit changed();return;}
    saving_=true;emit changed();
    repository_.saveRecordAsync(id_,draft_,this,[this](const PokedexWriteResult& result){
        saving_=false;
        if(result.success){cancel();emit saved();}
        else {error_=result.error.isEmpty()?"Couldn't save this record. Try again.":result.error;if(!open_)emit messageRequested(error_);}
        emit changed();
    });
}
void PokedexJournalEditor::dispatch(Action action) {
    if(!open_||saving_)return;
    if(action==Action::Back){cancel();return;}
    if(action==Action::Confirm){activate(focus_);return;}
    if(action==Action::ToggleContinue){submit();return;}
    if(action==Action::Left&&focus_==1)focus_=0;
    if(action==Action::Right&&focus_==0)focus_=1;
    if(action==Action::Down)focus_=2;
    if(action==Action::Up)focus_=0;
    emit changed();
}
}
