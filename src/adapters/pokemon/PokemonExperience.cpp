#include "PokemonExperience.h"
#include <QSignalBlocker>
#include "core/repository/CollectionRepository.h"
#include <QFile>
#include <QFileInfo>
namespace trainer {
ExperienceManifest PokemonExperience::manifest() const {
    static const auto cached=[] {
    ExperienceManifest result{"pokemon",1,{{"legacy-context",{},{{"legacy.domain","pokemon"}}}},
        {{"art","pokemon.classic-art",1,{}},{"sprites","pokemon.sprite-detail",1,{}}}};
    for(const auto& game:collectionCatalogue(true))if(game.domain=="pokemon" && !game.catalogueId.isEmpty())
        result.alternatives.append({"catalog-"+game.catalogueId,game.platformId,{{"catalog.traineros",game.catalogueId}}});
    // Provider identities verified against ScreenScraper gameinfos, 2026-10-09;
    // see this adapter's README. They select the family, never an exact save format.
    for(const auto& id:{"84406","84408","84409"})
        result.alternatives.append({"screenscraper-"+QString(id),"gba",{{"catalog.screenscraper",id}}});
    for(const auto& code:{"BPEE","BPRE","BPGE","AXVE","AXPE"})
        result.alternatives.append({"gba-"+QString(code),"gba",{{"rom.gba.code",code}}});
    return result;
    }();
    return cached;
}
QVariantMap observePokemonIdentity(const QString& platform,const QString& path) {
    // Bounded worker probe independent of scraping, names, or save mutation.
    // Header identity is presentation evidence only; hacks can retain these codes.
    if(platform!="gba" || QFileInfo(path).suffix().compare("gba",Qt::CaseInsensitive)!=0)return {};
    const QFileInfo before(path);QFile file(path);if(!file.open(QIODevice::ReadOnly))return {};
    const auto header=file.read(0xc0);const QFileInfo after(path);
    if(header.size()!=0xc0 || before.size()!=after.size() || before.lastModified()!=after.lastModified() || quint8(header[0xb2])!=0x96)return {};
    quint8 check=0x19;for(int i=0xa0;i<=0xbd;++i)check+=quint8(header[i]);
    if(check!=0)return {};
    return {{"rom.gba.code",QString::fromLatin1(header.mid(0xac,4))}};
}
PokemonExperience::PokemonExperience(ExperienceServices services,PokedexReferenceProvider& reference,PokedexProgressRepository& progress)
    :services_(services),persona_(services.profiles),dex_(reference,progress,this),center_(services.library,this),party_(!services.library.editable(),this) {
    persona_.configure(&services.library,&reference,&progress,&services.archive);
    connect(&services.achievements,&HallOfFameController::changed,this,[this]{
        if(slot_!=1 || view_=="profile")return;
        const auto face=QStringList{"journey","hall","ra"}.value(services_.achievements.faceIndex());
        if(face!=view_)emit faceRequested(1,face,false,context_.generation);
    });
    connect(&persona_,&TrainerController::changed,this,[this]{
        // Refresh the complete stored record, including opaque legacy persona
        // fields, so a later universal name edit cannot overwrite a new favorite.
        if(!persona_.editing() && !persona_.saving())services_.profile.reload();
        emit changed();
    });
    connect(&persona_,&TrainerController::messageRequested,this,&ExperienceModule::notice);
    connect(&persona_,&TrainerController::nameRequested,this,[this](QString initial){textApply_=[this](QString s){persona_.setDraftName(s);};emit textRequested("Trainer name",initial,TrainerController::NameLimit,context_.generation);});
    connect(&dex_,&PokedexController::changed,this,&ExperienceModule::changed);
    connect(&center_,&SaveCenterController::changed,this,[this]{if(center_.confirming())party_.openSaves();emit this->changed();});
    connect(&party_,&PartyPresentation::changed,this,[this]{
        if(boxName_ && party_.moveStage()!="name-edit"){boxName_=false;cancelText();emit textCancelled();}
        emit changed();
    });
    connect(&dex_,&PokedexController::messageRequested,this,&ExperienceModule::notice);
    connect(&center_,&SaveCenterController::messageRequested,this,&ExperienceModule::notice);
    connect(&center_,&SaveCenterController::restored,this,[this](const QString&){emit hostActionRequested("clear-resume",context_.generation);});
    connect(&center_,&SaveCenterController::closeRequested,this,[this]{emit hostActionRequested("close-service",context_.generation);});
    connect(&dex_,&PokedexController::searchRequested,this,[this](QString initial){textApply_=[this](QString s){dex_.applySearch(s);};emit textRequested("Field Guide · name or number",initial,32,context_.generation);});
    connect(&center_,&SaveCenterController::searchRequested,this,[this](QString initial){textApply_=[this](QString s){center_.applySearch(s);};emit textRequested("Find an Adventure",initial,64,context_.generation);});
    connect(&center_,&SaveCenterController::shopSearchRequested,this,[this](QString initial){textApply_=[this](QString s){center_.applyShopSearch(s);};emit textRequested("Find goods or shops",initial,64,context_.generation);});
    connect(&party_,&PartyPresentation::boxNameRequested,this,[this](QString initial,int limit){boxName_=true;textApply_=[this](QString s){boxName_=false;party_.applyBoxName(s);};emit textRequested("Box name",initial,limit,context_.generation);});
    connect(&party_,&PartyPresentation::healingRequested,this,[this]{controller_.setCenterRoute("clinic");emit faceRequested(0,"center",false,context_.generation);});
    connect(&party_,&PartyPresentation::backupsRequested,this,[this]{controller_.setCenterRoute("backups");emit faceRequested(0,"center",false,context_.generation);});
    connect(party_.activities(),&CenterActivities::shopsRequested,this,[this]{emit faceRequested(0,"shops",false,context_.generation);});
    connect(party_.activities()->link(),&LinkController::closeRequested,this,[this]{controller_.setCenterRoute("clinic");emit faceRequested(0,"center",false,context_.generation);});
    connect(party_.activities()->link(),&LinkController::workspaceRequested,this,[this]{controller_.setCenterRoute("link");emit faceRequested(0,"center",true,context_.generation);});
    connect(persona_.picker(),&SpeciesPicker::searchRequested,this,[this](QString initial){textApply_=[this](QString s){persona_.picker()->applySearch(s);};emit textRequested("Find your favorite · name / number",initial,48,context_.generation);});
}
void PokemonExperience::refresh(const ExperienceContext& context) {
    context_=context;
    const auto progress=context.progress?context.progress->snapshot():GameProgress{};
    const auto source=context.progress?context.progress->adventureId():QString();
    const auto title=context.game?context.game->title:QString();
    progressConfigured_ |= bool(context.progress);
    if(progressConfigured_ || services_.library.editable())dex_.setSaveProgress(context.adventure,title,source,progress);
    bool changed=false;
    {
        const QSignalBlocker batch(&party_);
        changed=party_.setAdventure(context.adventure,title);
        changed|=party_.setProgress(source,progress);
        const auto observed=source==context.adventure?progress:GameProgress{};
        const auto stamp=PracticeSource{context.owner,context.adventure,observed.contextRevision,observed.contentRevision,observed.saveRevision};
        party_.activities()->practice()->setObservation(stamp,observed,party_.activities()->actors());
        party_.activities()->link()->setTrainerName(context.profile["name"].toString());
        party_.activities()->link()->setObservation(stamp,observed,party_.activities()->actors());
    }
    if(changed)emit party_.changed();
}
QVariantMap PokemonExperience::homeProgress() const {return controller_.homeProgress(context_.game,context_.home,context_.progress);}
void PokemonExperience::show(int slot,const QString& face) {
    slot_=slot;view_=face;
    if(slot==0)controller_.show(face,context_.adventure);
    else if(face=="profile"){persona_.reload();persona_.refreshOverview();}
    else services_.achievements.showFace(face=="journey"?0:face=="hall"?1:2);
    emit changed();
}
void PokemonExperience::leave(bool primaryChange) {
    if(primaryChange){party_.activities()->practice()->leave();party_.activities()->link()->leave();}
    dex_.cancelTransient();center_.leaveClinic();center_.leaveShops();persona_.cancel();slot_=-1;
    emit changed();
}
bool PokemonExperience::dispatch(Action action) {
    if(persona_.editing()){persona_.dispatch(action);return true;}
    if(slot_==0){controller_.dispatch(action,context_.adventure);emit changed();return true;}
    if(historyFace()){services_.achievements.dispatch(action==Action::LocalAction && !modalOpen()?Action::ToggleContinue:action);return true;}
    if(slot_==1 && action==Action::Confirm){persona_.beginEdit();return true;}
    return false;
}
bool PokemonExperience::activate(int index,const QString& area) {
    if(persona_.editing()){persona_.activate(index);return true;}
    if(slot_==0){controller_.activate(index,area,context_.adventure);emit faceRequested(0,controller_.face(),false,context_.generation);return true;}
    if(historyFace()){if(area.isEmpty())services_.achievements.activate(index);else services_.achievements.activateControl(area,index);return true;}
    return dispatch(Action::Confirm);
}
int PokemonExperience::focusIndex() const {return persona_.editing()?persona_.focusIndex():slot_==0?controller_.focusIndex():historyFace()?services_.achievements.focusIndex():0;}
bool PokemonExperience::modalOpen() const {
    if(persona_.editing())return true;
    if(slot_==0)return centerFace()?center_.confirming() || center_.writing() || (center_.shopsOpen() && center_.shopModal()) || party_.detailOpen() || party_.moveOpen():dex_.zone()=="picker" || dex_.zone()=="art" || dex_.saving();
    return historyFace() && (services_.achievements.editor()->isOpen() || services_.achievements.account()->isOpen());
}
bool PokemonExperience::navigationBlocked(bool recovery) const {
    const auto* link=party_.activities()->link();
    return (link->navigationBlocked() && !(recovery && link->canBrowseForRecovery())) || party_.moveOpen() || center_.writing() || center_.confirming() || (center_.shopsOpen() && center_.shopModal());
}
bool PokemonExperience::intercept(Action action) {auto* link=party_.activities()->link();if(!link->invitationOpen())return false;link->dispatch(action);return true;}
QVariantMap PokemonExperience::invitation() const {
    auto* link=party_.activities()->link();
    return {{"open",link->invitationOpen()},{"incoming",link->invitationIncoming()},{"controller",QVariant::fromValue(static_cast<QObject*>(const_cast<LinkController*>(link)))}};
}
QJsonObject PokemonExperience::legacyState() const {return {{"pokedexFace",face()=="dex"?"pokedex":"center"},{"pokemonFace",face()},{"centerRoute",controller_.centerRoute()},{"party",party_.navigationState()},{"pokedex",dex_.navigationState()}};}
QStringList PokemonExperience::restoreLegacy(const QJsonObject& state) {
    controller_.restoreFace(state["pokemonFace"].toString(state["pokedexFace"].toString()=="center"?"party":"dex"));
    controller_.setCenterRoute(state["centerRoute"].toString()=="backups"?"backups":"clinic");
    party_.restoreNavigation(state["party"].toObject());dex_.restoreNavigation(state["pokedex"].toObject());
    const bool legacyHall=state["version"].toInt()==1 && (state["page"].toString()=="hall" || state["page"].toInt(-1)==4);
    return {face(),legacyHall?QStringList{"journey","hall","ra"}.value(services_.achievements.faceIndex()):state["trainerFace"].toString("profile")};
}
void PokemonExperience::cancelText() {ExperienceModule::cancelText();if(boxName_){boxName_=false;party_.cancelBoxName();}}
}
