#include "SettingsController.h"
#include "DisplayScale.h"
#include "features/library/ScrapeController.h"
#include "core/experience/ExperienceProviders.h"
#include <algorithm>
#include <QSettings>
#include <QCryptographicHash>

namespace trainer {
namespace {
const QStringList pageIds{"appearance","audio","media","feedback","trainer","system","credits","controller","library","saves","connections","clock","nearby","communication","scraper","software","game-input"};
const QList<QList<int>> sectionPages{{0,2},{1,7,16,10,3},{4,13,12},{8,9,14},{15},{5,11,6}};
}
QVariantList SettingsController::sections() const {
    return {QVariantMap{{"id","personalization"},{"title","Personalization"}},QVariantMap{{"id","device"},{"title","Device"}},
        QVariantMap{{"id","accounts"},{"title","Accounts & communication"}},QVariantMap{{"id","games"},{"title","Games & library"}},
        QVariantMap{{"id","software"},{"title","Installed software"}},QVariantMap{{"id","system"},{"title","System"}}};
}
int SettingsController::sectionIndex() const {
    for(int i=0;i<sectionPages.size();++i)if(sectionPages[i].contains(category_))return i;
    return 4;
}
QString SettingsController::categoryId() const {return pageIds.value(category_,"software");}
QVariantList SettingsController::pages() const {
    QVariantList result;
    for(const int page:sectionPages[sectionIndex()])result.append(QVariantMap{{"id",pageIds[page]},{"title",page==16?QString("Game input"):page==15?QString("Software"):categories()[page]}});
    return result;
}
void SettingsController::selectSection(int index) {
    index=std::clamp(index,0,int(sectionPages.size())-1);
    if(index==4) {if(storage_.busy()||clock_.busy())return;clock_.leave();communication_.leave();storage_.close();category_=15;row_=0;pane_=false;emit changed();}
    else selectCategory(sectionPages[index][0],false);
}
void SettingsController::selectPage(const QString& id,bool enter) {const int index=pageIds.indexOf(id);if(index>=0)selectCategory(index,enter);}
QString SettingsController::displayProfile() const {
    const auto value = QSettings().value("display/profile", "handheld").toString();
    return QStringList{"auto", "handheld", "tv"}.contains(value) ? value : "handheld";
}
void SettingsController::setMediaOwner(const QString& owner){mediaPrefix_="media/"+QString::fromLatin1(QCryptographicHash::hash(owner.toUtf8(),QCryptographicHash::Sha256).toHex())+"/";emit changed();}
bool SettingsController::previewSound() const{return QSettings().value(mediaPrefix_+"sound",false).toBool();}
int SettingsController::previewDelay() const{return std::clamp(QSettings().value(mediaPrefix_+"delay",1800).toInt(),1500,3000);}
int SettingsController::interfaceScale() const {
    return std::clamp(QSettings().value("display/interfaceScale", 100).toInt(), 75, 150);
}
double SettingsController::displayDensity(double sw, double sh, double ww, double wh,
        const QString& profile, int percent) const {
    return trainer::displayDensity(sw, sh, ww, wh, profile, percent);
}
void SettingsController::adjustDisplay(int row, int direction) {
    if(category_==7 && input_ && direction && row>=1 && row<=5) {
        auto p=input_->profile();
        if(row==1) {const auto values=ControllerProfile::families();p["family"]=values[(values.indexOf(p["family"].toString())+values.size()+(direction<0?-1:1))%values.size()];}
        if(row==2) {const QStringList values{"auto","east","south"};p["confirm"]=values[(values.indexOf(p["confirm"].toString())+values.size()+(direction<0?-1:1))%values.size()];}
        if(row==3)p["deadzone"]=std::clamp(p["deadzone"].toInt()+(direction<0?-5:5),5,40);
        if(row==4)p["invertX"]=!p["invertX"].toBool();
        if(row==5)p["invertY"]=!p["invertY"].toBool();
        input_->setProfile(p);return;
    }
    if(!saving_ && category_==1 && row==1 && direction) {
        QSettings settings;const int volume=std::clamp(settings.value("achievements/volume",65).toInt(),0,100);
        settings.setValue("achievements/volume",std::clamp(volume+(direction<0?-5:5),0,100));settings.sync();
        error_=settings.status()==QSettings::NoError?QString():QString("Could not save sound settings");
        emit changed();return;
    }
    if (saving_ || category_ != 0 || (row != 3 && row != 4) || !direction) return;
    pane_ = true; row_ = row;
    QSettings settings;
    if (row == 3) {
        const QStringList profiles{"auto", "handheld", "tv"};
        settings.setValue("display/profile", profiles[(profiles.indexOf(displayProfile()) + (direction < 0 ? 2 : 1)) % 3]);
    } else settings.setValue("display/interfaceScale", std::clamp(interfaceScale() + (direction < 0 ? -5 : 5), 75, 150));
    settings.sync();
    error_ = settings.status() == QSettings::NoError ? QString() : QStringLiteral("Could not save display settings");
    emit changed();
}
void SettingsController::resetDisplayScale() {
    QSettings settings; settings.setValue("display/interfaceScale", 100); settings.sync();
    error_ = settings.status() == QSettings::NoError ? QString() : QStringLiteral("Could not save display settings");
    emit changed();
}
void SettingsController::configureNearby(NativeActivityProvider* value) {
    if(nearby_==value)return;
    if(nearby_)disconnect(nearby_,nullptr,this,nullptr);
    nearby_=value;if(value)connect(value,&NativeActivityProvider::changed,this,&SettingsController::changed);
    emit changed();
}
void SettingsController::setLibraryScanState(bool available,bool busy,const QString& result) {
    libraryAvailable_=available;libraryScanning_=busy;
    if(busy)libraryStatus_.clear();
    else if(!result.isEmpty())libraryStatus_=result;
    emit changed();
}
void SettingsController::reload() { if (repository_ && !saving_) { value_ = repository_->preferences(); emit changed(); } }
void SettingsController::activate(int index) {
    if(index<0 || index>3 || saving_) return;
    auto candidate = value_;
    if (index == 0) {
        const QStringList themes{"light", "dark"};
        candidate.theme = themes[(themes.indexOf(theme()) + 1) % themes.size()];
    } else if(index==3) candidate.videoPreviews=!candidate.videoPreviews;
    else if(index==2) candidate.worldEditing=!candidate.worldEditing;
    else candidate.reducedMotion = !candidate.reducedMotion;
    error_.clear(); saving_ = true; emit changed();
    const auto completed = [this, candidate](const QString& error) {
        saving_ = false; error_ = error;
        if (error.isEmpty()) value_ = candidate;
        else emit messageRequested(error);
        emit changed();
    };
    if (repository_) repository_->savePreferences(candidate, this, completed); else completed({});
}
QVariantList SettingsController::controls() const {
    const auto row = [](const char* title, const char* kind, const QString& detail) {
        return QVariantMap{{"title",title},{"kind",kind},{"detail",detail}};
    };
    switch(category_) {
    case 0: return {row("Appearance","theme",theme()=="dark"?"Dark":"Light"),row("Reduced motion","toggle",reducedMotion()?"On":"Off"),row("Brightness","brightness",""),row("Display profile","stepper",displayProfile()=="auto"?"Auto · system density":displayProfile()=="tv"?"TV · larger controls":"Handheld"),row("Interface size","stepper",QString::number(interfaceScale())+"%"),row("Reset interface size","action","Restore 100%")};
    case 1: { const int volume=std::clamp(QSettings().value("achievements/volume",65).toInt(),0,100);
        return {row("Volume","volume",""),row("Achievement jingle","stepper",volume?QString::number(volume)+"%":"Off"),row("Background music","unavailable","Music playback is not available yet")}; }
    case 2: return {row("Video previews","toggle",videoPreviews()?"On · local files only":"Off"),row("Preview sound","action",previewSound()?"On":"Muted"),row("Preview delay","action",QString::number(previewDelay()/1000.0,'f',1)+" seconds"),row("Adventure pictures","status","Clean exit pictures on Home and in Choose Adventure"),row("Field Guide illustrations","status","Illustrations and animated companions")};
    case 3: return {row("Charger vibration","unavailable","Patterns have not been verified on this handheld"),row("Device lighting","unavailable","Lighting support has not been verified")};
    case 4: return {row("Your profile","action","Name and emblem"),row("RetroAchievements","action","Manage your account"), trainersAvailable_ ? row("Trainers","action","Choose a player or create a Trainer") : row("Separate Trainers & PIN","unavailable","Not available yet"),row("Trainer PIN",trainersAvailable_?"action":"unavailable","Set, change or remove your PIN"),row("Family code",trainersAvailable_?"action":"unavailable","A parent can reset forgotten PINs")};
    case 5: return {row("Refresh status","action",""),row("Restart","action",""),row("Power off","action",""),row("System health & game setup","action","Runtime, BIOS and a private support report")};
    case 7: {
        const auto p=input_?input_->profile():ControllerProfile{}.json();
        const char* kind=input_&&input_->connected()?"stepper":"unavailable";
        return {row("Check controller","action",input_&&!input_->deviceName().isEmpty()?input_->deviceName():"Connect a controller · touch remains available"),
            row("Controller icons",kind,p["family"].toString()+(p["family"]=="auto"&&input_?" · "+input_->family():QString())),
            row("Confirm button",kind,p["confirm"].toString()+" · "+(input_?input_->glyph("A"):QString("A"))),
            row("Stick dead zone",kind,QString::number(p["deadzone"].toInt())+"%"),
            row("Invert left stick X",kind,p["invertX"].toBool()?"On":"Off"),
            row("Invert left stick Y",kind,p["invertY"].toBool()?"On":"Off"),
            row("Reset this controller",input_&&input_->connected()?"action":"unavailable","Restore automatic layout and default calibration"),
            row("Recovery","status",input_&&!input_->profileError().isEmpty()?input_->profileError():QString("Home: controls · Home + %1: keyboard. Touch remains available.").arg(input_?input_->glyph(input_->family()=="nintendo"?"north":"west"):QString("X")))};
    }
    case 8: {
        QVariantList result{row("Collections","action","Create and edit your game collections")};
        result.append(row(libraryScanning_?"Refreshing library…":"Refresh library",
            libraryAvailable_ && !libraryScanning_?"action":"unavailable",
            libraryScanning_?"Looking for games and updated artwork":libraryAvailable_?"Find copied games and reload artwork":"Connect your game library first"));
        result.append(row("Game storage",storage_.apply?"action":"unavailable",storage_.root()));
        result.append(row("ScreenScraper","action","Account, downloads and artwork display"));
        if(legacyTrash_)result.append(row("Previous game trash","action","Restore games removed by an earlier version"));
        return result;
    }
    case 9: return {row("Read-only saves",savePolicy_?"toggle":"unavailable",readOnlySaves()?"On - reading and backups only":"Off - allow confirmed save changes")};
    case 10: return {}; // NetworkController owns the inline connections pane.
    case 12: return {row("Visible to nearby Trainers",nearby_?"toggle":"unavailable",nearby_ && nearby_->visibleNearby()?"On":"Off")};
    case 15: return {row("Apps","action","Open installed applications"),row("Crates","action","Manage installed Crates and their data")};
    case 16: return gameInput_.rows();
    default: return {};
    }
}
void SettingsController::selectCategory(int index, bool enter) {
    if(storage_.busy() || clock_.busy())return;
    clock_.leave();
    if(index!=13)communication_.leave();
    storage_.close();
    category_ = std::clamp(index,0,int(pageIds.size())-1); row_=0; pane_=enter;
    if(category_==16 && !gameInput_.editing())gameInput_.begin();
    if(category_==11)clock_.begin();
    if(category_==13)communication_.begin();
    emit changed();
}
void SettingsController::activateRow(int index) {
    if(category_==16){gameInput_.activate(index);return;}
    if(category_==14&&scraper_){scraper_->activateSetting(index);return;}
    if(category_==13){communication_.activate(index);return;}
    if(category_==11){clock_.activate(index);return;}
    if(storage_.isOpen()){storage_.activate(index);return;}
    if(saving_)return;
    pane_=true; row_=std::clamp(index,0,std::max(0,int(controls().size())-1));
    if(category_==0 && row_<2) activate(row_);
    else if(category_==0 && row_==2) emit quickAdjustment(1,Action::Confirm);
    else if(category_==0 && (row_==3 || row_==4)) { adjustDisplay(row_,1); return; }
    else if(category_==0 && row_==5) resetDisplayScale();
    else if(category_==1 && row_==0) emit quickAdjustment(0,Action::Confirm);
    else if(category_==1 && row_==1) {adjustDisplay(1,1);return;}
    else if(category_==2 && row_==0) activate(3);
    else if(category_==2 && (row_==1||row_==2)){QSettings s;if(row_==1)s.setValue(mediaPrefix_+"sound",!previewSound());else s.setValue(mediaPrefix_+"delay",previewDelay()>=3000?1500:std::min(3000,previewDelay()+500));s.sync();if(s.status()!=QSettings::NoError)error_="Could not save preview preferences.";}
    else if(category_==4) emit trainerRequested(row_);
    else if(category_==5 && row_==3) emit healthRequested();
    else if(category_==5) emit deviceRequested(row_);
    else if(category_==7 && row_==0 && input_) input_->beginCapture("center");
    else if(category_==7 && row_>=1 && row_<=5) adjustDisplay(row_,1);
    else if(category_==7 && row_==6 && input_) input_->resetProfile();
    else if(category_==8 && row_==0) emit collectionsRequested();
    else if(category_==8 && row_==1 && libraryAvailable_ && !libraryScanning_) emit libraryRefreshRequested();
    else if(category_==8 && row_==2) storage_.begin();
    else if(category_==8 && row_==3) selectCategory(14);
    else if(category_==8 && row_==4 && legacyTrash_) emit trashRequested();
    else if(category_==9 && savePolicy_) {
        saving_=true;error_.clear();
        savePolicy_->setReadOnly(!readOnlySaves(),this,[this](const QString& error){saving_=false;error_=error;emit changed();});
    }
    else if(category_==12 && nearby_)nearby_->setVisibleNearby(!nearby_->visibleNearby());
    else if(category_==15)emit softwareRequested(row_==0?"apps":"crates");
    emit changed();
}
void SettingsController::cycleTheme(int direction) {
    if(saving_) return;
    const QStringList themes{"light","dark"};
    auto candidate=value_;
    candidate.theme=themes[(themes.indexOf(theme())+1)%2];
    error_.clear();saving_=true;emit changed();
    const auto completed=[this,candidate](const QString& error){saving_=false;error_=error;if(error.isEmpty())value_=candidate;else emit messageRequested(error);emit changed();};
    if(repository_) repository_->savePreferences(candidate,this,completed); else completed({});
}
void SettingsController::dispatch(Action action) {
    if(category_==16 && pane_ && (gameInput_.editing() || (action!=Action::PreviousFace && action!=Action::NextFace))){gameInput_.dispatch(action);return;}
    if(action==Action::PreviousFace || action==Action::NextFace) {
        const auto pages=sectionPages[sectionIndex()];
        if(!pages.isEmpty())selectCategory(pages[(pages.indexOf(category_)+pages.size()+(action==Action::PreviousFace?-1:1))%pages.size()],pane_);
        return;
    }
    if(category_==14&&pane_&&scraper_){scraper_->dispatchSettings(action);return;}
    if(category_==13 && pane_){communication_.dispatch(action);return;}
    if(clock_.busy())return;
    if(category_==11 && pane_){clock_.dispatch(action);return;}
    if(storage_.isOpen()){storage_.dispatch(action);return;}
    if(action==Action::Back) {
        if(pane_) {pane_=false;emit changed();} else emit closeRequested();
        return;
    }
    if(!pane_) {
        if(action==Action::Up) selectSection(sectionIndex()-1);
        else if(action==Action::Down) selectSection(sectionIndex()+1);
        else if(action==Action::Confirm || action==Action::Right) {pane_=true;emit changed();}
        return;
    }
    if(category_==6) {if(action==Action::Confirm){pane_=false;emit changed();}return;}
    if(action==Action::Up) row_=std::max(0,row_-1);
    if(action==Action::Down) row_=std::min(int(controls().size())-1,row_+1);
    if(action==Action::Confirm) {activateRow(row_);return;}
    if(action==Action::Left || action==Action::Right) {
        if(category_==7) {adjustDisplay(row_,action==Action::Left?-1:1);return;}
        if(category_==0 && row_==0) cycleTheme(action==Action::Left?-1:1);
        else if(category_==0 && row_==1 && reducedMotion()!=(action==Action::Right)) activate(1);
        else if(category_==0 && row_==2) emit quickAdjustment(1,action);
        else if(category_==0 && (row_==3 || row_==4)) adjustDisplay(row_,action==Action::Left?-1:1);
        else if(category_==1 && row_==0) emit quickAdjustment(0,action);
        else if(category_==1 && row_==1) adjustDisplay(1,action==Action::Left?-1:1);
        else if(category_==2 && row_==0 && videoPreviews()!=(action==Action::Right)) activate(3);
        else if(category_==9 && readOnlySaves()!=(action==Action::Right)) activateRow(0);

    }
    emit changed();
}
}
