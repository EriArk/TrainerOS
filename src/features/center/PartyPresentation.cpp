#include "PartyPresentation.h"
#include <algorithm>
#include <QJsonArray>

namespace trainer {
QString PartyPresentation::boxName() const {
    return !sample_ && snapshot_ && box_ < snapshot_->boxes.size() ? snapshot_->boxes[box_].name : QString("Box %1").arg(box_+1);
}
void PartyPresentation::setProgress(const QString& adventureId, const GameProgress& progress) {
    if (sample_) return;
    const bool matches = adventureId == id_ && !id_.isEmpty();
    const auto state = matches ? progress.availability : ProgressAvailability::Unsupported;
    const QString key = matches ? adventureId + progress.contextRevision + progress.contentRevision + progress.saveRevision + QString::number(int(state)) : QString();
    if (observationKey_ == key && availability_ == state) return;
    if(moveOpen() && !moving() && matches && !progress.contextRevision.isEmpty() && !sourceContext_.isEmpty() && progress.contextRevision!=sourceContext_)cancelMove();
    observationKey_ = key; availability_ = state;
    saveRevision_=matches?progress.saveRevision:QString();
    snapshot_ = matches && state == ProgressAvailability::Available ? progress.party : std::nullopt;
    detail_ = false; activitiesFocus_ = false; boxFocus_ = false;
    if (snapshot_ && sourceContext_ != progress.contextRevision) {
        sourceContext_ = progress.contextRevision; initialBoxSet_ = false; partyFocus_ = 0;
        std::fill(std::begin(storageFocus_),std::end(storageFocus_),0);
    }
    if (!initialBoxSet_ && snapshot_ && snapshot_->error.isEmpty()) { box_ = snapshot_->currentBox; initialBoxSet_ = true; }
    if (snapshot_ && box_ >= boxCount()) box_ = 0;
    syncActors();
    emit changed();
}
void PartyPresentation::syncActors() {
    QVariantList actors;
    if (sample_) {
        // Explicit development fixture; never fills missing production records.
        for (const auto& pair : {QPair<QString,QString>{"Bulbasaur","bulbasaur/1"}, {"Pikachu","pikachu/25"}})
            actors.append(withArt({{"name",pair.first},{"target",pair.second},{"kind","known"},{"level","12"},{"condition","Healthy"}}));
    } else if (available()) {
        for (int i=0;i<snapshot_->party.size() && i<6;++i)
            if (snapshot_->party[i].kind == PokemonSlotKind::Known) actors.append(present(snapshot_->party[i],i));
    }
    activities_.setParty(actors, id_ + observationKey_, status());
}
QVariantMap PartyPresentation::present(const PokemonRecord& p, int index) const {
    const auto kind = p.kind == PokemonSlotKind::Known ? "known" : p.kind == PokemonSlotKind::Egg ? "egg" : p.kind == PokemonSlotKind::Unreadable ? "unreadable" : "empty";
    QVariantMap row{{"index",index},{"kind",kind},{"name",p.kind == PokemonSlotKind::Egg ? "Egg" : p.kind == PokemonSlotKind::Empty ? "Empty slot" : "Unreadable slot"},
        {"condition",p.condition},{"level","—"},{"hp","—"}};
    if (p.kind != PokemonSlotKind::Known) return row;
    row["name"] = p.nickname.isEmpty() ? p.speciesName : p.nickname;
    row["species"] = p.speciesName; row["target"] = p.speciesId + '/' + p.formId;
    row["level"] = QString::number(p.level); row["types"] = p.types.join(" / ");
    row["ability"] = p.ability; row["nature"] = p.nature; row["item"] = p.item;
    row["shiny"] = p.shiny; row["itemId"]=p.itemId;
    if (p.hp) { row["hp"] = QString("%1 / %2").arg(*p.hp).arg(p.stats[0]); row["hpRatio"] = p.stats[0] ? double(*p.hp)/p.stats[0] : 0; }
    QVariantList stats; for (auto value : p.stats) stats.append(value); row["stats"] = stats;
    QStringList moves;
    for (const auto& move : p.moves) moves.append(move.name.isEmpty() ? QStringLiteral("—") : QString("%1 · %2 / %3 PP").arg(move.name).arg(move.pp).arg(move.maxPp));
    row["moves"] = moves.join('\n');
    return withArt(row);
}
void PartyPresentation::configureArtwork(ClassicArt* art, SpriteArt* sprites) {
    art_ = art; sprites_ = sprites;
    activities_.link()->setArtwork([this](QVariantMap row){return withArt(row);});
    if (art_) connect(art_, &ClassicArt::changed, this, [this] { syncActors(); emit changed(); });
    syncActors();
    emit changed();
}
void PartyPresentation::changeBox(int delta) {
    if (!available() || boxCount() == 0 || section_ != "storage" || detail_ || moveOpen()) return;
    box_ = (box_ + delta % boxCount() + boxCount()) % boxCount(); emit changed();
}
PartyPresentation::PartyPresentation(bool sample, QObject* parent) : QObject(parent), sample_(sample), activities_(sample, this) {
    connect(&activities_, &CenterActivities::changed, this, &PartyPresentation::changed);
    connect(&activities_, &CenterActivities::closeRequested, this, [this] {
        section_ = managementSection_; activitiesFocus_ = true; emit changed();
    });
    syncActors();
}
void PartyPresentation::openActivities() {
    if (section_ != "party" && section_ != "storage") return;
    managementSection_ = section_; section_ = "activities"; detail_ = false;
    activities_.reset(); emit changed();
}
QString PartyPresentation::status() const {
    if (sample_) return "Development sample · not your save · all records are read-only";
    if (availability_ == ProgressAvailability::Checking) return "Reading your team…";
    if (availability_ == ProgressAvailability::Missing) return "Save in the Adventure, then return here.";
    if (availability_ == ProgressAvailability::Unreadable) return "The save could not be read. Close the game and try again.";
    if (snapshot_) return snapshot_->error;
    if (!id_.isEmpty() && title_.isEmpty()) return "The selected Adventure is no longer linked.";
    return id_.isEmpty() ? "Choose an Adventure with a supported save to view its Party and Storage."
        : "Party and Storage reading is not available for this Adventure yet.";
}
void PartyPresentation::setAdventure(const QString& id, const QString& title) {
    if (id_ != id) {
        cancelMove();
        id_ = id; detail_ = false; partyFocus_ = 0; std::fill(std::begin(storageFocus_),std::end(storageFocus_),0); box_ = 0;
        snapshot_.reset(); observationKey_.clear(); sourceContext_.clear(); initialBoxSet_ = false; availability_ = ProgressAvailability::Unsupported;
        activitiesFocus_ = false; boxFocus_ = false; activities_.reset();
    }
    title_ = title; syncActors(); emit changed();
}
QVariantMap PartyPresentation::slot(int index) const {
    if (!sample_) {
        if (!available()) return {};
        const auto& members = section_ == "party" ? snapshot_->party : snapshot_->boxes[box_].members;
        return index >= 0 && index < members.size() ? present(members[index], index) : QVariantMap{};
    }
    QVariantMap row{{"index",index},{"name","Empty slot"},{"species",""},{"summary","No Pokémon"},
        {"hp","—"},{"level","—"},{"condition","Empty"},{"item","—"},{"moves","—"},{"kind","empty"}};
    const int record = section_ == "party" ? index : box_ == 0 ? (index < 4 ? index : index % 5 == 0 ? index % 2 : 6) : (index == 7 ? 3 : 6);
    if (record == 0) {
        row["name"]="Bulbasaur"; row["species"]="#001"; row["level"]="12"; row["hp"]="28 / 35";
        row["condition"]="Healthy"; row["item"]="None"; row["moves"]="Tackle · 31 / 35 PP\nGrowl · 40 / 40 PP\nVine Whip · PP unknown";
        row["kind"]="known"; row["summary"]="Lv. 12 · HP 28 / 35";
        row["target"]="bulbasaur/1"; row["types"]="Grass / Poison"; row["ability"]="Overgrow"; row["nature"]="Modest";
        row["hpRatio"]=28.0/35; row["stats"]=QVariantList{35,18,20,25,23,17};
    } else if (record == 1) {
        row["name"]="Pikachu"; row["species"]="#025"; row["level"]="15"; row["hp"]="0 / 38";
        row["condition"]="Fainted"; row["item"]="Unknown"; row["moves"]="Move data unavailable";
        row["kind"]="known"; row["summary"]="Lv. 15 · Fainted";
        row["target"]="pikachu/25"; row["types"]="Electric"; row["ability"]="Static"; row["nature"]="—";
        row["hpRatio"]=0.0; row["stats"]=QVariantList{38,24,17,22,20,35};
    } else if (record == 2) {
        row["name"]="Egg"; row["condition"]="Egg"; row["summary"]="Species and hatch progress unknown"; row["kind"]="egg";
    } else if (record == 3) {
        row["name"]="Unreadable slot"; row["condition"]="Unavailable"; row["summary"]="Record could not be read"; row["kind"]="unreadable";
    }
    return withArt(row);
}
QVariantMap PartyPresentation::withArt(QVariantMap row) const {
    if (row["kind"] == "known") {
        const auto target = row["target"].toString();
        row["art"] = art_ ? art_->image(target, "pokedexDetailArt") : QVariantMap{};
        QVariantMap clips, portraits;
        if (sprites_) for (const auto& asset : sprites_->choices(target))
            if (asset.toMap()["kind"] == "sprite") {
                clips[asset.toMap()["action"].toString()] = asset;
                if (asset.toMap()["action"] == "Idle") row["sprite"] = asset;
            } else if (asset.toMap()["kind"] == "portrait")
                portraits[asset.toMap()["action"].toString()] = asset;
        row["clips"] = clips;
        row["portraits"] = portraits;
    }
    return row;
}
QVariantList PartyPresentation::entries() const {
    QVariantList result;
    if (!available() || section_ == "saves" || section_ == "activities") return result;
    for (int i = 0; i < (section_ == "party" ? 6 : 30); ++i) result.append(slot(i));
    return result;
}
QVariantMap PartyPresentation::detail() const {
    return available() && (section_ == "party" || section_ == "storage") ? slot(section_ == "party" ? partyFocus_ : storageFocus_[box_]) : QVariantMap{};
}
void PartyPresentation::activate(int index) {
    if(moveOpen()){moveActivate(index);return;}
    if (section_ == "activities") { activities_.activate(index); return; }
    if (activitiesFocus_) { openActivities(); return; }
    if (boxFocus_) { changeBox(1); return; }
    if (detail_) { if(index==5){beginHeldItems();return;} if(index==4){beginRelease();return;} if(index==3){beginMove();return;} if (index == 2) {detail_=false;emit changed();emit healingRequested();} else if (index == 1) {detail_=false;emit backupsRequested();} else { detail_ = false; emit changed(); } return; }
    if (!available()) { emit backupsRequested(); return; }
    const int count = section_ == "party" ? 6 : 30;
    if (index < 0 || index >= count || section_ == "saves") return;
    (section_ == "party" ? partyFocus_ : storageFocus_[box_]) = index;
    detail_ = true; menuIndex_ = canMove()?3:0; emit changed();
}
void PartyPresentation::showSection(const QString& section) {
    section_=section;detail_=false;activitiesFocus_=false;boxFocus_=false;emit changed();
}
QJsonObject PartyPresentation::navigationState() const {
    QJsonArray positions;for(auto i:storageFocus_)positions.append(i);
    return {{"adventure",id_},{"context",sourceContext_},{"box",box_},{"party",partyFocus_},{"slots",positions}};
}
void PartyPresentation::restoreNavigation(const QJsonObject& state) {
    if(state["adventure"].toString()!=id_)return;
    sourceContext_=state["context"].toString();
    box_=std::clamp(state["box"].toInt(),0,13);partyFocus_=std::clamp(state["party"].toInt(),0,5);
    const auto positions=state["slots"].toArray();for(int i=0;i<14;++i)storageFocus_[i]=std::clamp(i<positions.size()?positions.at(i).toInt():0,0,29);
    initialBoxSet_=true;emit changed();
}
void PartyPresentation::openSaves() {
    if (section_ != "saves") previousSection_ = section_;
    section_ = "saves"; detail_ = false; emit changed();
}
void PartyPresentation::returnFromSaves() { section_ = previousSection_; emit changed(); }
void PartyPresentation::dispatch(Action action) {
    if(moveOpen()){dispatchMove(action);return;}
    if(action==Action::Secondary && !detail_ && canRenameBox()){beginBoxName();return;}
    if (section_ == "activities") { activities_.dispatch(action); return; }
    if (detail_) {
        if (action == Action::Back) { detail_ = false; emit changed(); }
        else if (action == Action::Confirm) activate(menuIndex_);
        else if (action == Action::Up || action == Action::Down) { QList<int> order{2,1,0};if(canRelease())order.prepend(4);if(canHoldItems())order.prepend(5);if(canMove())order.prepend(3); const int i=order.indexOf(menuIndex_); menuIndex_=order[(i+(action==Action::Up?order.size()-1:1))%order.size()]; emit changed(); }
        return;
    }
    if (action == Action::LocalAction) { emit backupsRequested(); return; }

    if (boxFocus_) {
        if (action == Action::Left || action == Action::Right || action == Action::Confirm) changeBox(action == Action::Left ? -1 : 1);
        else if (action == Action::Down || action == Action::Back) { boxFocus_ = false; emit changed(); }
        return;
    }
    if (action == Action::Confirm) { activate(focusIndex()); return; }
    if (activitiesFocus_) {
        if (action == Action::Up || action == Action::Back) { activitiesFocus_ = false; emit changed(); }
        return;
    }

    if (!available() || action == Action::Back) return;
    auto& focus = section_ == "party" ? partyFocus_ : storageFocus_[box_];
    const int columns = section_ == "party" ? 2 : 6;
    const int count = section_ == "party" ? 6 : 30;
    if (section_ == "storage" && action == Action::Up && focus < columns) boxFocus_ = true;
    else if (action == Action::Left && focus % columns > 0) --focus;
    else if (action == Action::Right && focus % columns < columns - 1) ++focus;
    else if (action == Action::Up) focus = std::max(0, focus - columns);
    else if (action == Action::Down) focus = std::min(count - 1, focus + columns);
    emit changed();
}
void PartyPresentation::configureMovement(SaveBackupService* service,LibraryRepository* library) {
    movementService_=service;movementLibrary_=library;
    if(service)connect(service,&SaveBackupService::policyChanged,this,[this]{if(!moving())cancelMove();emit changed();});
}
bool PartyPresentation::canRenameBox() const {
    return !sample_ && section_=="storage" && available() && snapshot_->boxNameLimit>0
        && box_>=0 && box_<snapshot_->boxes.size() && movementService_ && movementLibrary_
        && !movementService_->busy() && !movementService_->readOnly() && !saveRevision_.isEmpty();
}
void PartyPresentation::beginBoxName() {beginOperation(Operation::BoxName);}
void PartyPresentation::editBoxName() {
    moveStage_="name-edit";moveMessage_.clear();emit changed();
    emit boxNameRequested(boxNameDraft_,moveSnapshot_.boxNameLimit);
}
void PartyPresentation::cancelBoxName() {
    if(operation_==Operation::BoxName && moveStage_=="name-edit") {cancelMove();emit changed();}
}
void PartyPresentation::applyBoxName(const QString& text) {
    if(operation_!=Operation::BoxName || moveStage_!="name-edit")return;
    boxNameDraft_=text.trimmed();
    const bool valid=!boxNameDraft_.isEmpty() && boxNameDraft_.size()<=moveSnapshot_.boxNameLimit
        && std::all_of(boxNameDraft_.begin(),boxNameDraft_.end(),[this](QChar c){return moveSnapshot_.boxNameCharacters.contains(c);});
    moveStage_=valid?"name-confirm":"name-error";
    moveMessage_=valid?moveSnapshot_.boxes[moveRequest_.from.box].name+"  \u2192  "+boxNameDraft_
        :QString("Choose a name of 1-%1 characters. Use letters, numbers or the game's punctuation.").arg(moveSnapshot_.boxNameLimit);
    emit changed();
}
bool PartyPresentation::canMove() const {
    return !sample_ && available() && snapshot_->canManage && movementService_ && movementLibrary_
        && !movementService_->busy() && !movementService_->readOnly() && !saveRevision_.isEmpty()
        && detail().value("kind").toString()=="known";
}
bool PartyPresentation::canRelease() const {
    return !sample_ && available() && snapshot_->canRelease && movementService_ && movementLibrary_
        && !movementService_->busy() && !movementService_->readOnly() && !saveRevision_.isEmpty()
        && detail().value("kind").toString()=="known";
}
bool PartyPresentation::canHoldItems() const {
    return !sample_ && available() && snapshot_->canHoldItems && movementService_ && movementLibrary_
        && !movementService_->busy() && !movementService_->readOnly() && !saveRevision_.isEmpty()
        && detail().value("kind").toString()=="known";
}
void PartyPresentation::cancelMove() {if(moving())return;++moveGeneration_;moveStage_.clear();moveToken_.clear();emit changed();}
QString PartyPresentation::moveTitle() const {
    if(operation_==Operation::BoxName)return moveStage_=="name-confirm"?"Rename this box?":moveStage_=="writing"?"Saving box name":"Box name";
    if(moveStage_=="items")return "Held item \u00b7 " + moveName_;
    if(moveStage_=="item-confirm")return heldItemChoice_?"Give this item?":"Take held item?";
    if(moveStage_=="release-confirm")return "Release " + moveName_ + "?";
    if(moveStage_=="places")return "Move " + moveName_;
    if(moveStage_=="slots")return moveTargetBox_<0?"Choose a team position":moveSnapshot_.boxes[moveTargetBox_].name;
    if(moveStage_=="confirm")return !movePair().isEmpty()?"Swap places?":"Move " + moveName_ + "?";
    return moveStage_=="writing"?"Saving your team…":"Your Pokemon";
}
QVariantList PartyPresentation::movePair() const {
    if(moveStage_!="confirm")return {};
    const auto from=moveRequest_.from,to=moveRequest_.to;
    const auto& destination=to.box<0?moveSnapshot_.party[to.slot]:moveSnapshot_.boxes[to.box].members[to.slot];
    if(destination.kind==PokemonSlotKind::Empty)return {};
    QVariantList pair;
    for(const auto pos:{from,to}) {
        auto row=present(pos.box<0?moveSnapshot_.party[pos.slot]:moveSnapshot_.boxes[pos.box].members[pos.slot],pos.slot);
        row["place"]=(pos.box<0?QString("Party"):moveSnapshot_.boxes[pos.box].name)+" · "+QString::number(pos.slot+1);
        pair.append(row);
    }
    return pair;
}
QVariantList PartyPresentation::moveRows() const {
    QVariantList rows;
    if(moveStage_=="items") {
        if(releaseSubject_["itemId"].toInt())rows.append(QVariantMap{{"id",0},{"name","Take " + releaseSubject_["item"].toString()},{"pocket","Return to Bag"},{"quantity",0}});
        for(const auto& item:moveSnapshot_.bag.items) {
            if(item.id==releaseSubject_["itemId"].toInt())continue;
            rows.append(QVariantMap{{"id",item.id},{"name",item.name},{"pocket",item.pocket},{"quantity",item.quantity}});
        }
    } else if(moveStage_=="places") {
        rows.append(QVariantMap{{"name","Party"},{"kind","place"}});
        for(const auto& box:moveSnapshot_.boxes)rows.append(QVariantMap{{"name",box.name},{"kind","place"}});
    } else if(moveStage_=="slots") {
        const auto& members=moveTargetBox_<0?moveSnapshot_.party:moveSnapshot_.boxes[moveTargetBox_].members;
        for(int i=0;i<members.size();++i)rows.append(present(members[i],i));
    }
    return rows;
}
void PartyPresentation::beginMove() {beginOperation(Operation::Move);}
void PartyPresentation::beginRelease() {beginOperation(Operation::Release);}
void PartyPresentation::beginHeldItems() {beginOperation(Operation::HeldItem);}
void PartyPresentation::beginOperation(Operation operation) {
    if(moveOpen() || (operation==Operation::BoxName?!canRenameBox():operation==Operation::Release?!canRelease():operation==Operation::HeldItem?!canHoldItems():!canMove()))return;
    operation_=operation;releaseSubject_=detail();
    const auto record=movementLibrary_->registration(id_);if(!record)return;
    moveRegistration_=*record;moveSnapshot_=*snapshot_;moveName_=detail()["name"].toString();
    if(operation==Operation::BoxName)boxNameDraft_=boxName();
    moveRequest_={{section_=="party"?-1:box_,section_=="party"?partyFocus_:storageFocus_[box_]}, {},saveRevision_};
    moveIndex_=section_=="party"?0:box_+1;moveTargetBox_=section_=="party"?-1:box_;
    detail_=false;moveStage_="checking";moveMessage_="Checking your save…";const auto generation=++moveGeneration_;emit changed();
    movementService_->inspect(*record,this,[this,generation](const SaveBackupSnapshot& snapshot){
        if(generation!=moveGeneration_)return;
        moveToken_=snapshot.token;
        if(snapshot.token.isEmpty() || !snapshot.hasSave || !snapshot.error.isEmpty()) {moveStage_="result";moveMessage_=snapshot.error.isEmpty()?"Save inside your Adventure first.":snapshot.error;}
        else if(operation_==Operation::BoxName) {editBoxName();return;}
        else if(operation_==Operation::Release) {
            moveStage_="release-confirm";
            const auto item=releaseSubject_["item"].toString();
            moveMessage_="This Pokemon will leave your collection.";
            if(item!="None")moveMessage_+=" Its held item will leave too.";
            moveMessage_+=" A backup will be kept.";
        } else if(operation_==Operation::HeldItem) {
            moveStage_="items";moveIndex_=0;moveMessage_.clear();
            if(moveRows().isEmpty()){moveStage_="result";moveMessage_="There are no items to give in your Bag.";}
        } else {moveStage_="places";moveMessage_.clear();}
        emit changed();
    });
}
void PartyPresentation::moveActivate(int index) {
    if(moveStage_=="writing" || moveStage_=="checking")return;
    if(moveStage_=="result"){cancelMove();return;}
    if(moveStage_=="name-error"){editBoxName();return;}
    if(moveStage_=="name-confirm"){submitOperation();return;}
    if(moveStage_=="items") {
        const auto rows=moveRows();if(index<0 || index>=rows.size())return;
        moveIndex_=index;const auto row=rows[index].toMap();heldItemChoice_=row["id"].toInt();
        moveMessage_=heldItemChoice_?moveName_+" will hold "+row["name"].toString()+".":releaseSubject_["item"].toString()+" will return to your Bag.";
        if(heldItemChoice_ && releaseSubject_["itemId"].toInt())moveMessage_+="\n"+releaseSubject_["item"].toString()+" will return to your Bag.";
        moveStage_="item-confirm";emit changed();return;
    }
    if(moveStage_=="places") {
        if(index<0 || index>moveSnapshot_.boxes.size())return;
        moveTargetBox_=index-1;moveStage_="slots";moveIndex_=0;moveMessage_.clear();emit changed();return;
    }
    if(moveStage_=="slots") {
        const auto& members=moveTargetBox_<0?moveSnapshot_.party:moveSnapshot_.boxes[moveTargetBox_].members;
        if(index<0 || index>=members.size())return;
        moveIndex_=index;const auto kind=members[index].kind;
        const bool reorder=moveRequest_.from.box<0 && moveTargetBox_<0;
        if(moveRequest_.from.box==moveTargetBox_ && moveRequest_.from.slot==index) {moveMessage_="Choose another position.";emit changed();return;}
        const bool exchange=!reorder && moveSnapshot_.canSwapOccupied && kind==PokemonSlotKind::Known;
        if(reorder ? kind==PokemonSlotKind::Empty || kind==PokemonSlotKind::Unreadable : kind!=PokemonSlotKind::Empty && !exchange) {
            moveMessage_=reorder?"Choose another occupied position.":moveSnapshot_.canSwapOccupied?"Choose an empty slot or a hatched Pokemon.":"Choose an empty slot.";emit changed();return;
        }
        if(moveTargetBox_<0 && !reorder && !exchange) {
            int first=0;while(first<members.size() && members[first].kind!=PokemonSlotKind::Empty)++first;
            if(index!=first){moveMessage_="Choose the first empty team position.";emit changed();return;}
        }
        moveRequest_.to={moveTargetBox_,index};moveRequest_.exchangeOccupied=exchange;moveStage_="confirm";
        const QString destination=moveTargetBox_<0?"Party":moveSnapshot_.boxes[moveTargetBox_].name;
        moveMessage_=reorder || exchange?QString{}:destination+" · Slot "+QString::number(index+1);
        emit changed();return;
    }
    if(moveStage_!="confirm" && moveStage_!="item-confirm")return;
    submitOperation();
}
void PartyPresentation::confirmRelease() {
    if(moveStage_=="release-confirm" && operation_==Operation::Release)submitOperation();
}
void PartyPresentation::submitOperation() {
    if(!movementService_ || movementService_->busy())return;
    const auto record=movementLibrary_->registration(moveRegistration_.adventure.id);
    if(!record || record->revision!=moveRegistration_.revision || movementService_->readOnly()) {
        moveStage_="result";moveMessage_="The Adventure or save policy changed. Choose again.";emit changed();return;
    }
    moveStage_="writing";moveMessage_="Keeping a backup and saving…";const auto generation=moveGeneration_;emit changed();
    const auto completed=[this,generation](const SaveBackupResult& result){
        if(generation!=moveGeneration_)return;
        moveStage_="result";moveMessage_=result.message;emit changed();
    };
    if(operation_==Operation::BoxName)movementService_->renameBox(*record,moveToken_,{moveRequest_.from.box,boxNameDraft_,moveRequest_.saveRevision},this,completed);
    else if(operation_==Operation::HeldItem)movementService_->changeHeldItem(*record,moveToken_,{moveRequest_.from,heldItemChoice_,moveRequest_.saveRevision},this,completed);
    else if(operation_==Operation::Release)movementService_->releasePokemon(*record,moveToken_,{moveRequest_.from,moveRequest_.saveRevision},this,completed);
    else movementService_->movePokemon(*record,moveToken_,moveRequest_,this,completed);
}
void PartyPresentation::dispatchMove(Action action) {
    if(moving())return;
    if(action==Action::Back) {
        if(moveStage_=="name-confirm"){editBoxName();return;}
        if(moveStage_=="item-confirm"){moveStage_="items";moveMessage_.clear();}
        else if(moveStage_=="confirm"){moveStage_="slots";moveMessage_.clear();}
        else if(moveStage_=="slots"){moveStage_="places";moveIndex_=moveTargetBox_+1;moveMessage_.clear();}
        else cancelMove();
        emit changed();return;
    }
    if(action==Action::Secondary && moveStage_=="release-confirm"){confirmRelease();return;}
    if(action==Action::Confirm){moveActivate(moveIndex_);return;}
    const int count=moveRows().size();if(!count)return;
    const int columns=moveStage_=="items"?1:moveStage_=="places"?3:moveTargetBox_<0?2:6;
    if(action==Action::Left && moveIndex_%columns>0)--moveIndex_;
    else if(action==Action::Right && moveIndex_%columns<columns-1)moveIndex_=std::min(count-1,moveIndex_+1);
    else if(action==Action::Up)moveIndex_=std::max(0,moveIndex_-columns);
    else if(action==Action::Down)moveIndex_=std::min(count-1,moveIndex_+columns);
    emit changed();
}

}
