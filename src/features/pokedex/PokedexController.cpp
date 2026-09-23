#include "PokedexController.h"
#include <algorithm>

namespace trainer {
namespace {
QString recorded(const std::optional<bool>& value) { return value ? (*value ? "Yes" : "No") : "Not recorded"; }
QString searchable(const QString& value) {
    QString result;
    for(const auto& c : value.normalized(QString::NormalizationForm_D)) if(c.isLetterOrNumber())result+=c.toLower();
    return result;
}
QString stateLabel(const PokedexProgress& p) {
    if (p.caught.value_or(false)) return "Caught";
    if (p.seen.value_or(false)) return "Seen";
    if (p.seen.has_value() && p.caught.has_value()) return "Not seen";
    return "Not recorded";
}
}
void PokedexController::configureArtwork(ClassicArt* art) {
    art_ = art;
    if (art_) connect(art_, &ClassicArt::changed, this, [this] { emit rowsChanged(); emit changed(); });
    emit rowsChanged(); emit changed();
}
void PokedexController::configureSprites(SpriteArt* sprites) { sprites_ = sprites; emit changed(); }
QVariantList PokedexController::spriteChoices() const { return sprites_ ? sprites_->choices(artTarget()) : QVariantList{}; }
QVariantMap PokedexController::spritePreview() const {
    for (const auto& row : spriteChoices()) if (row.toMap()["kind"] == "sprite" && row.toMap()["action"] == "Idle") return row.toMap();
    return {};
}
QVariantMap PokedexController::spriteClips() const {
    QVariantMap result;
    for (const auto& row : spriteChoices()) if (row.toMap()["kind"] == "sprite") result[row.toMap()["action"].toString()] = row;
    return result;
}
QString PokedexController::spriteStatus() const { return sprites_ ? sprites_->status() : "Sprites not installed"; }
void PokedexController::openSprites() {
    if (zone_ != "detail" || journal_.isOpen() || saving_) return;
    spriteFocus_ = 0; zone_ = "sprites"; emit changed();
}
QString PokedexController::artTarget() const {
    if (filtered_.isEmpty()) return {};
    const auto& entry = filtered_[entryIndex()];
    return entry.id + '/' + selectedForm(entry).id;
}
QString PokedexController::artCoverage() const { return art_ ? art_->coverage() : "Illustrations not installed"; }
QVariantList PokedexController::artChoices() const { return art_ ? art_->choices(artTarget()) : QVariantList{}; }
void PokedexController::openArtwork() {
    if (zone_ != "detail" || journal_.isOpen() || saving_) return;
    artFocus_ = 0;
    const auto choices = artChoices();
    for (int i = 0; i < choices.size(); ++i) if (choices[i].toMap()["current"].toBool()) artFocus_ = i;
    zone_ = "art"; emit changed();
}
PokedexController::PokedexController(PokedexReferenceProvider& reference, PokedexProgressRepository& progress, QObject* parent)
    : QObject(parent), reference_(reference), progress_(progress), journal_(progress,this) {
    connect(&journal_, &PokedexJournalEditor::changed, this, &PokedexController::changed);
    connect(&journal_, &PokedexJournalEditor::saved, this, &PokedexController::rebuild);
    connect(&journal_, &PokedexJournalEditor::messageRequested, this, &PokedexController::messageRequested);
    refresh();
}
int PokedexController::entryIndex() const {
    for (int i = 0; i < filtered_.size(); ++i) if (filtered_[i].id == selectedId_) return i;
    return 0;
}
int PokedexController::focusIndex() const {
    if (zone_ == "sprites") return spriteFocus_;
    if (zone_ == "art") return artFocus_;
    if (zone_ == "picker") return pickerFocus_;
    if (zone_ == "rail") return railFocus_;
    if (zone_ == "detail") return detailFocus_;
    return zone_ == "list" ? entryIndex() : 0;
}
PokedexForm PokedexController::selectedForm(const PokedexEntry& entry) const {
    if(entry.forms.isEmpty())return {{},"Standard",entry.types};
    if(entry.id==selectedId_)for(const auto& form:entry.forms)if(form.id==formId_)return form;
    for(const auto& form:entry.forms)if(!type_.isEmpty()&&form.types.contains(type_))return form;
    if(!query_.isEmpty()&&!searchable(entry.name).contains(searchable(query_)))
        for(const auto& form:entry.forms)if(searchable(form.name).contains(searchable(query_)))return form;
    return entry.forms.first();
}
QVariantMap PokedexController::present(const PokedexEntry& entry, bool detailed) const {
    const auto p = progress_.progress(entry.id);
    const auto form=selectedForm(entry);
    QStringList worlds;
    for (const auto& w : catalog_.collections) if (entry.collectionIds.contains(w.id)) worlds.append(w.name);
    QVariantMap result{{"id", entry.id}, {"number", QString("#%1").arg(entry.number, 3, 10, QChar('0'))},
        {"name", entry.name}, {"types", form.types.join(" / ")}, {"worlds", worlds.join(" · ")},
        {"status", stateLabel(p)}, {"seen", recorded(p.seen)}, {"caught", recorded(p.caught)},
        {"favorite", p.favorite}};
    const auto target = entry.id + '/' + form.id;
    result["formId"] = form.id;
    result["art"] = art_ ? art_->image(target, detailed ? "pokedexDetailArt" : "pokedexListArt") : QVariantMap{};
    if(detailed) {
        result["form"]=form.name;result["formCount"]=int(entry.forms.size());result["notes"]=p.notes;
        result["height"]=form.heightDm?QString::number(form.heightDm/10.0,'f',1)+" m":"—";
        result["weight"]=form.weightHg?QString::number(form.weightHg/10.0,'f',1)+" kg":"—";
        result["editable"]=progress_.recordsEditable();
        QStringList family;for(const auto& id:entry.familyIds)family.append(names_.value(id,id));result["family"]=family.join(" · ");
        const QStringList labels{"HP","Attack","Defense","Sp. Atk","Sp. Def","Speed"};QVariantList stats;
        for(int i=0;i<6;++i){const int value=i<form.stats.size()?form.stats[i]:0;stats.append(QVariantMap{{"label",labels[i]},{"value",value?QString::number(value):"—"},{"fraction",value/255.0}});}
        result["stats"]=stats;
    }
    return result;
}
QVariantList PokedexController::entries() const {
    QVariantList result;
    for (const auto& e : filtered_) result.append(present(e));
    return result;
}
QVariantMap PokedexController::detail() const {
    if (filtered_.isEmpty()) return {{"id", ""}, {"name", "A new discovery awaits"}, {"number", "—"},
        {"types", ""}, {"worlds", ""}, {"status", ""}, {"seen", "Not recorded"}, {"caught", "Not recorded"}, {"favorite", false},
        {"form",""},{"formCount",0},{"notes",""},{"height","—"},{"weight","—"},{"family",""},{"stats",QVariantList{}},{"editable",false}};
    return present(filtered_.at(entryIndex()),true);
}
QString PokedexController::selection(int index) const {
    if (index == 1) return world_;
    if (index == 2) return type_;
    if (index == 3) return status_;
    return sort_;
}
QList<PokedexController::Choice> PokedexController::options() const {
    if (pickerKind_ == 1) {
        QList<Choice> result{{"", "All Worlds"}};
        for (const auto& w : catalog_.collections) result.append({w.id, w.name});
        return result;
    }
    if (pickerKind_ == 2) {
        QList<Choice> result{{"", "All types"}};
        // The whole type vocabulary fits the fixed 5×4 chooser, including Cancel.
        for (const auto& type : QStringList{"Bug", "Dark", "Dragon", "Electric", "Fairy", "Fighting", "Fire", "Flying", "Ghost",
                                           "Grass", "Ground", "Ice", "Normal", "Poison", "Psychic", "Rock", "Steel", "Water"})
            result.append({type, type});
        return result;
    }
    if (pickerKind_ == 3) return {{"", "All records"}, {"seen", "Seen"}, {"caught", "Caught"},
        {"favorite", "Favorites"}, {"unseen", "Not seen"}, {"unknown", "Not recorded"}, {"uncaught", "Not caught"}};
    return {{"number", "Number ↑"}, {"name", "Name A–Z"}};
}
QString PokedexController::selectionLabel(int index) const {
    if (index == 1) {
        for (const auto& w : catalog_.collections) if (w.id == world_) return w.name;
        return "All Worlds";
    }
    if (index == 2) return type_.isEmpty() ? "All types" : type_;
    if (index == 3) {
        if (status_ == "seen") return "Seen";
        if (status_ == "caught") return "Caught";
        if (status_ == "favorite") return "Favorites";
        if (status_ == "unseen") return "Not seen";
        if (status_ == "unknown") return "Not recorded";
        if (status_ == "uncaught") return "Not caught";
        return "All records";
    }
    return sort_ == "name" ? "Name A–Z" : "Number ↑";
}
QVariantList PokedexController::rail() const {
    QVariantList result{QVariantMap{{"label", "Search"}, {"value", query_.isEmpty() ? "Name / number" : query_}}};
    const QStringList labels{"", "World", "Type", "Records", "Order"};
    for (int i = 1; i <= 4; ++i) result.append(QVariantMap{{"label", labels[i]}, {"value", selectionLabel(i)}});
    result.append(QVariantMap{{"label", "Reset"}, {"value", "Clear filters"}});
    return result;
}
QVariantList PokedexController::choices() const {
    QVariantList result;
    for (const auto& choice : options()) result.append(QVariantMap{{"id", choice.id}, {"label", choice.label},
        {"current", choice.id == selection(pickerKind_)}});
    result.append(QVariantMap{{"id", "cancel"}, {"label", "Cancel"}, {"current", false}});
    return result;
}
QString PokedexController::pickerTitle() const {
    return QStringList{"", "Choose a World collection", "Choose a type", "Choose your records", "Order the field guide"}.at(pickerKind_);
}
QString PokedexController::emptyMessage() const {
    if (!error_.isEmpty()) return error_;
    return catalog_.entries.isEmpty() ? "The field guide is empty. Reference entries will appear here when available."
        : "No entries match this combination. Change a filter or reset to explore again.";
}
QString PokedexController::recoveryLabel() const { return !error_.isEmpty() || catalog_.entries.isEmpty() ? "Retry field guide" : "Reset filters"; }
void PokedexController::rebuild() {
    filtered_.clear();
    QString numeric = query_.trimmed();
    if (numeric.startsWith('#')) numeric.remove(0, 1);
    bool isNumber = false;
    const int number = numeric.toInt(&isNumber);
    for (const auto& e : catalog_.entries) {
        const auto p = progress_.progress(e.id);
        if (!world_.isEmpty() && !e.collectionIds.contains(world_)) continue;
        if (!type_.isEmpty() && !e.types.contains(type_)
            && std::none_of(e.forms.begin(),e.forms.end(),[&](const auto& form){return form.types.contains(type_);})) continue;
        if (status_ == "seen" && !p.seen.value_or(false) && !p.caught.value_or(false)) continue;
        if (status_ == "caught" && !p.caught.value_or(false)) continue;
        if (status_ == "uncaught" && (!p.caught.has_value() || *p.caught)) continue;
        if (status_ == "favorite" && !p.favorite) continue;
        if (status_ == "unseen" && (!p.seen.has_value() || *p.seen || !p.caught.has_value() || *p.caught)) continue;
        if (status_ == "unknown" && (p.caught.value_or(false) || p.seen.value_or(false) || (p.seen.has_value() && p.caught.has_value()))) continue;
        if (!query_.isEmpty() && !(isNumber ? e.number == number : searchable(e.name).contains(searchable(query_))
            || std::any_of(e.forms.begin(),e.forms.end(),[&](const auto& form){return searchable(form.name).contains(searchable(query_));}))) continue;
        filtered_.append(e);
    }
    std::sort(filtered_.begin(), filtered_.end(), [this](const auto& a, const auto& b) {
        if (sort_ == "name") {
            const auto compare = QString::compare(a.name, b.name, Qt::CaseInsensitive);
            if (compare != 0) return compare < 0;
        }
        return a.number == b.number ? a.id < b.id : a.number < b.number;
    });
    const bool exists = std::any_of(filtered_.begin(), filtered_.end(), [&](const auto& e) { return e.id == selectedId_; });
    if (!exists) {
        selectedId_ = filtered_.isEmpty() ? QString() : filtered_.first().id;
        if (zone_ == "detail") zone_ = filtered_.isEmpty() ? "recovery" : "list";
    }
    if (filtered_.isEmpty() && zone_ == "list") zone_ = "recovery";
    emit rowsChanged();
    emit changed();
}
void PokedexController::refresh() {
    const auto loaded = reference_.load();
    error_ = loaded.success ? QString() : loaded.error;
    if (!loaded.success && error_.isEmpty()) error_ = "The field guide couldn't be loaded. Try again.";
    // Failed refresh keeps the last good reference snapshot available.
    if (loaded.success) { catalog_ = loaded;names_.clear();for(const auto& entry:catalog_.entries)names_.insert(entry.id,entry.name); }
    rebuild();
    if (!loaded.success && !catalog_.entries.isEmpty()) emit messageRequested(error_);
}
void PokedexController::reset() {
    query_.clear(); world_.clear(); type_.clear(); status_.clear(); sort_ = "number";
    zone_ = "list";
    rebuild();
}
QJsonObject PokedexController::navigationState() const {
    return {{"entry", selectedId_}, {"query", query_}, {"world", world_}, {"type", type_}, {"status", status_},
            {"sort", sort_}, {"zone", zone_ == "picker" ? "rail" : zone_}, {"rail", railFocus_}, {"detail", detailFocus_},{"form",formId_}};
}
void PokedexController::restoreNavigation(const QJsonObject& state) {
    selectedId_ = state["entry"].toString();
    formId_=state["form"].toString();
    query_ = state["query"].toString().left(128).trimmed();
    world_.clear();
    for (const auto& world : catalog_.collections) if (world.id == state["world"].toString()) world_ = world.id;
    type_.clear(); status_.clear();
    const auto oldKind = pickerKind_;
    pickerKind_ = 2;
    for (const auto& choice : options()) if (choice.id == state["type"].toString()) type_ = choice.id;
    pickerKind_ = 3;
    for (const auto& choice : options()) if (choice.id == state["status"].toString()) status_ = choice.id;
    pickerKind_ = oldKind;
    sort_ = state["sort"].toString() == "name" ? "name" : "number";
    const auto zone = state["zone"].toString();
    zone_ = QStringList{"list", "rail", "detail", "recovery"}.contains(zone) ? zone : "list";
    railFocus_ = std::clamp(state["rail"].toInt(), 0, 5);
    detailFocus_ = std::clamp(state["detail"].toInt(), 0, 1);
    rebuild();
    if (zone_ == "recovery" && !filtered_.isEmpty()) zone_ = "list";
    emit changed();
}
void PokedexController::openPicker(int index) {
    pickerKind_ = index;
    pickerFocus_ = 0;
    const auto values = options();
    for (int i = 0; i < values.size(); ++i) if (values[i].id == selection(index)) pickerFocus_ = i;
    zone_ = "picker";
    emit pickerChanged();
}
void PokedexController::cancelTransient() {
    journal_.cancel();
    if (zone_ == "art" || zone_ == "sprites") { zone_ = "detail"; emit changed(); }
    if (zone_ == "picker") { zone_ = "rail"; emit changed(); }
}
void PokedexController::applySearch(const QString& text) {
    query_ = text.trimmed();
    zone_ = "rail"; railFocus_ = 0;
    rebuild();
}
void PokedexController::activate(int index) {
    if(journal_.isOpen()){journal_.activate(index);return;}
    if (zone_ == "sprites") { zone_ = "detail"; emit changed(); return; }
    if (zone_ == "art") {
        const auto choices = artChoices();
        if (!choices.isEmpty() && index >= 0 && index < choices.size()) {
            const auto error = art_->select(artTarget(), choices[index].toMap()["id"].toString());
            if (!error.isEmpty()) { emit messageRequested(error); return; }
        }
        zone_ = "detail"; emit changed(); return;
    }
    if (zone_ == "picker") {
        const auto values = options();
        if (index < 0 || index > values.size()) return;
        if (index < values.size()) {
            const auto value = values[index].id;
            if (pickerKind_ == 1) world_ = value;
            else if (pickerKind_ == 2) type_ = value;
            else if (pickerKind_ == 3) status_ = value;
            else sort_ = value;
        }
        zone_ = "rail";
        rebuild();
    } else if (zone_ == "rail") {
        if (index < 0 || index > 5) return;
        railFocus_ = index;
        if (index == 0) emit searchRequested(query_);
        else if (index == 5) reset();
        else openPicker(index);
    } else if (zone_ == "list") {
        if (index < 0 || index >= filtered_.size()) return;
        selectedId_ = filtered_[index].id;
        zone_ = "detail"; detailFocus_ = 0;
    } else if (zone_ == "detail") {
        if (index == 1) zone_ = "list";
        else if (index == 0 && !filtered_.isEmpty() && !saving_) {
            saving_ = true;
            progress_.setFavoriteAsync(selectedId_, !progress_.progress(selectedId_).favorite, this,
                                      [this](const QString& error) {
                saving_ = false;
                if (!error.isEmpty()) emit messageRequested(error);
                else rebuild(); // Reconcile against the current filter, even after leaving this detail.
                emit changed();
            });
        }
    } else if (zone_ == "recovery" && index == 0) {
        if (recoveryLabel() == "Reset filters") reset();
        else { zone_ = "list"; refresh(); }
    }
    emit changed();
}
void PokedexController::activateControl(const QString& zone, int index) {
    const bool browsing = zone_ == "list" || zone_ == "rail" || zone_ == "recovery";
    if (zone != zone_ && !(browsing && (zone == "rail" || (zone == "list" && !filtered_.isEmpty())
                                      || (zone == "recovery" && filtered_.isEmpty())))) return;
    zone_ = zone;
    activate(index);
}
void PokedexController::dispatch(Action action) {
    if(journal_.isOpen()){journal_.dispatch(action);return;}
    if (zone_ == "sprites") {
        if (action == Action::Back || action == Action::Confirm) zone_ = "detail";
        else if (action == Action::Left) spriteFocus_ = std::max(0, spriteFocus_ - 1);
        else if (action == Action::Right) spriteFocus_ = std::min(std::max(0, int(spriteChoices().size()) - 1), spriteFocus_ + 1);
        emit changed(); return;
    }
    if (zone_ == "detail" && action == Action::Down) { openSprites(); return; }
    if (zone_ == "art") {
        if (action == Action::Back) zone_ = "detail";
        else if (action == Action::Confirm) { activate(artFocus_); return; }
        else if (action == Action::Left || action == Action::Up) artFocus_ = std::max(0, artFocus_ - 1);
        else if (action == Action::Right || action == Action::Down) artFocus_ = std::min(std::max(0, int(artChoices().size()) - 1), artFocus_ + 1);
        emit changed(); return;
    }
    if (zone_ == "detail" && action == Action::Up) { openArtwork(); return; }
    if(zone_=="detail" && action==Action::Secondary){cycleForm();return;}
    if(zone_=="detail" && action==Action::ToggleContinue){editJournal();return;}
    if((zone_=="list"||zone_=="rail"||zone_=="recovery") && action==Action::Secondary){railFocus_=0;zone_="rail";emit searchRequested(query_);emit changed();return;}
    if (action == Action::Confirm) { activate(focusIndex()); return; }
    if (action == Action::Back) {
        if (zone_ == "picker") zone_ = "rail";
        else if (zone_ == "detail") zone_ = "list";
    } else if (zone_ == "picker") {
        const int count = options().size() + 1;
        if (action == Action::Left && pickerFocus_ % 5 > 0) --pickerFocus_;
        if (action == Action::Right && pickerFocus_ % 5 < 4 && pickerFocus_ + 1 < count) ++pickerFocus_;
        if (action == Action::Up && pickerFocus_ >= 5) pickerFocus_ -= 5;
        if (action == Action::Down && pickerFocus_ + 5 < count) pickerFocus_ += 5;
    } else if (zone_ == "rail") {
        if (action == Action::Left) railFocus_ = std::max(0, railFocus_ - 1);
        if (action == Action::Right) railFocus_ = std::min(5, railFocus_ + 1);
        if (action == Action::Down) zone_ = filtered_.isEmpty() ? "recovery" : "list";
    } else if (zone_ == "list") {
        const int index = entryIndex();
        if (action == Action::Up) {
            if (index == 0) zone_ = "rail";
            else selectedId_ = filtered_[index - 1].id;
        }
        if (action == Action::Down && index + 1 < filtered_.size()) selectedId_ = filtered_[index + 1].id;
        if(action==Action::Left)selectedId_=filtered_[std::max(0,index-8)].id;
        if(action==Action::Right)selectedId_=filtered_[std::min(int(filtered_.size())-1,index+8)].id;
    } else if (zone_ == "detail") {
        if (action == Action::Left) detailFocus_ = 0;
        if (action == Action::Right) detailFocus_ = 1;
    } else if (zone_ == "recovery" && action == Action::Up) zone_ = "rail";
    emit changed();
}
void PokedexController::cycleForm() {
    if(zone_!="detail"||journal_.isOpen()||filtered_.isEmpty())return;
    const auto& entry=filtered_[entryIndex()];if(entry.forms.size()<2)return;
    const auto selected=selectedForm(entry);
    for(int i=0;i<entry.forms.size();++i)if(entry.forms[i].id==selected.id){formId_=entry.forms[(i+1)%entry.forms.size()].id;break;}
    emit changed();
}
void PokedexController::editJournal() {
    if(zone_!="detail"||filtered_.isEmpty()||saving_)return;
    const auto& entry=filtered_[entryIndex()];journal_.begin(entry.id,entry.name);
}
}
