#include "ArchiveEditor.h"
#include <QUuid>
#include <QTimeZone>
#include <algorithm>

namespace trainer {
ArchiveEditor::ArchiveEditor(HallOfFameRepository& repository, QObject* parent) : QObject(parent), repository_(repository) {}
QList<Adventure> ArchiveEditor::adventures() const {
    QList<Adventure> values;
    if (!library_) return values;
    for (const auto& adventure : library_->adventures())
        if (adventure.domain == "pokemon" && !adventure.collectionOnly && (adventure.title + " " + adventure.variant).contains(query_, Qt::CaseInsensitive)) values.append(adventure);
    std::sort(values.begin(), values.end(), [](const auto& a, const auto& b) {
        const auto compare=QString::compare(a.title,b.title,Qt::CaseInsensitive);
        return compare==0 ? a.id<b.id : compare<0;
    });
    return values;
}
void ArchiveEditor::choose(const Adventure& adventure) {
    draft_.adventureId=adventure.id; draft_.adventureTitle=adventure.title;
    draft_.world=adventure.worldId;
    if (library_) for (const auto& world : library_->worlds()) if (world.id==adventure.worldId) draft_.world=world.name;
}
void ArchiveEditor::begin(const std::optional<HallOfFameEntry>& entry) {
    if (isOpen() || saving_ || !repository_.archiveEditable()) return;
    draft_=entry.value_or(HallOfFameEntry{});
    query_.clear();error_.clear();textTarget_.clear();focus_=0;route_="form";
    if (!entry) {
        draft_.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
        // Completion date/time are manual facts, never inferred from a launch.
    }
    while (draft_.team.size()<6) draft_.team.append(HallOfFameMember{{}, {}});
    emit changed();
}
void ArchiveEditor::cancel() {
    route_.clear();error_.clear();textTarget_.clear();draft_={};focus_=0;
    emit changed();
}
QVariantList ArchiveEditor::fields() const {
    QVariantList result;
    if (route_=="team") {
        for (int i=0;i<draft_.team.size();++i) {
            const auto& member=draft_.team[i];
            result.append(QVariantMap{{"label",QString("TEAM MEMBER %1").arg(i+1)},
                {"value",member.name.isEmpty()?"Not recorded":member.name},
                {"subtitle",member.level?QString("Lv %1").arg(*member.level):"Level not recorded"}});
        }
        return result;
    }
    int members=0; for (const auto& member : draft_.team) if (!member.name.isEmpty()) ++members;
    return {QVariantMap{{"label","ADVENTURE"},{"value",draft_.adventureTitle.isEmpty()?"Choose from your library":draft_.adventureTitle},{"subtitle",draft_.world}},
        QVariantMap{{"label","COMPLETED ON"},{"value",draft_.completedAt.isValid()?draft_.completedAt.toUTC().toString("yyyy-MM-dd"):"Not recorded"},{"subtitle","YYYYMMDD · optional"}},
        QVariantMap{{"label","PLAYTIME"},{"value",draft_.playtimeMinutes?QString::number(*draft_.playtimeMinutes)+" minutes":"Not recorded"},{"subtitle","From your own record · optional"}},
        QVariantMap{{"label","YOUR NOTE"},{"value",draft_.notes.isEmpty()?"Add a memory…":draft_.notes},{"subtitle","Up to 160 characters"}},
        QVariantMap{{"label","CHAMPION TEAM"},{"value",members?QString("%1 of 6 members recorded").arg(members):"Record your team"},{"subtitle","Names and levels · optional"}}};
}
QVariantList ArchiveEditor::rows() const {
    QVariantList result;
    for (const auto& adventure : adventures()) {
        QString world=adventure.worldId;
        if (library_) for (const auto& item : library_->worlds()) if (item.id==world) { world=item.name;break; }
        result.append(QVariantMap{{"id",adventure.id},{"title",adventure.title},{"subtitle",world+(adventure.variant.isEmpty()?QString():" · "+adventure.variant)}});
    }
    return result;
}
void ArchiveEditor::activate(int index) {
    if (!isOpen() || saving_) return;
    error_.clear();
    if (route_=="adventures") {
        const auto values=adventures();
        if (values.isEmpty()) { route_="form";focus_=0;emit changed();return; }
        if (index<0 || index>=values.size()) return;
        choose(values[index]);route_="form";focus_=0;
    } else if (route_=="team") {
        if (index<0 || index>=6) return;
        focus_=index;textMember_=index;textTarget_="name";
        emit textRequested(QString("Team member %1 · name").arg(index+1),draft_.team[index].name,24);
    } else {
        if (index<0 || index>4) return;
        focus_=index;
        if (index==0) {
            route_="adventures";focus_=0;
            const auto values=adventures();for(int i=0;i<values.size();++i)if(values[i].id==draft_.adventureId)focus_=i;
        } else if (index==1) {
            textTarget_="date";emit textRequested("Completion date · YYYYMMDD or blank",draft_.completedAt.isValid()?draft_.completedAt.toUTC().toString("yyyyMMdd"):QString(),8);
        } else if (index==2) {
            textTarget_="time";emit textRequested("Playtime · minutes or blank",draft_.playtimeMinutes?QString::number(*draft_.playtimeMinutes):QString(),6);
        } else if (index==3) { textTarget_="note";emit textRequested("Your memory · optional",draft_.notes,160); }
        else { route_="team";focus_=0; }
    }
    emit changed();
}
void ArchiveEditor::editLevel() {
    if (route_!="team" || focus_<0 || focus_>=6 || saving_) return;
    textMember_=focus_;textTarget_="level";
    emit textRequested("Team member level · 1–100 or blank",draft_.team[focus_].level?QString::number(*draft_.team[focus_].level):QString(),3);
}
void ArchiveEditor::applyText(const QString& text) {
    if (!isOpen() || saving_) return;
    const auto value=text.trimmed();error_.clear();
    if (textTarget_=="search") { query_=value;focus_=0; }
    else if (textTarget_=="note") draft_.notes=value;
    else if (textTarget_=="name") { draft_.team[textMember_].name=value;if(value.isEmpty())draft_.team[textMember_].level.reset(); }
    else if (textTarget_=="date") {
        const auto date=QDate::fromString(value,"yyyyMMdd");
        if (!value.isEmpty() && (!date.isValid() || date.toString("yyyyMMdd")!=value)) error_="Use eight digits: year, month, day (YYYYMMDD), or leave it blank.";
        else draft_.completedAt=value.isEmpty()?QDateTime():QDateTime(date,QTime(0,0),QTimeZone("UTC"));
    } else if (textTarget_=="time" || textTarget_=="level") {
        bool ok=false;const int number=value.toInt(&ok);
        const bool digits=std::all_of(value.begin(),value.end(),[](QChar c){return c>='0'&&c<='9';});
        if (textTarget_=="time") {
            if (!value.isEmpty() && (!ok || !digits || number<0 || number>999999)) error_="Enter 0 to 999999 minutes, or leave it blank.";
            else draft_.playtimeMinutes=value.isEmpty()?std::nullopt:std::optional<int>(number);
        } else {
            if (!value.isEmpty() && (!ok || !digits || number<1 || number>100 || draft_.team[textMember_].name.isEmpty())) error_="Add a team member first, then a level from 1 to 100.";
            else draft_.team[textMember_].level=value.isEmpty()?std::nullopt:std::optional<int>(number);
        }
    }
    textTarget_.clear();emit changed();
}
void ArchiveEditor::save() {
    if (route_!="form" || saving_) return;
    error_=validateArchiveEntry(draft_);
    if (!error_.isEmpty()) { emit changed();return; }
    const auto candidate=draft_;
    saving_=true;emit changed();
    repository_.saveArchiveAsync(candidate,this,[this,candidate](const ArchiveWriteResult& result){
        saving_=false;
        if (result.success) { cancel();emit saved(candidate.id); }
        else {
            error_=result.error.isEmpty()?"Couldn't save this memory. Try again.":result.error;
            if (!isOpen()) emit messageRequested(error_);
        }
        emit changed();
    });
}
void ArchiveEditor::dispatch(Action action) {
    if (!isOpen()) return;
    if (action==Action::Back) {
        if (saving_) return;
        if (route_=="form") cancel();
        else { focus_=route_=="team"?4:0;route_="form";error_.clear();emit changed(); }
        return;
    }
    if (saving_) return;
    if (action==Action::Confirm) { activate(focus_);return; }
    if (action==Action::ToggleContinue && route_=="form") { save();return; }
    if (action==Action::Secondary) {
        if (route_=="team") editLevel();
        else if (route_=="adventures") { textTarget_="search";emit textRequested("Find an Adventure · title or version",query_,48); }
        return;
    }
    if (route_=="adventures") {
        const int last=std::max(0,int(adventures().size())-1);
        if (action==Action::Up) focus_=std::max(0,focus_-1);
        if (action==Action::Down) focus_=std::min(last,focus_+1);
        if (action==Action::Left) focus_=std::max(0,focus_-8);
        if (action==Action::Right) focus_=std::min(last,focus_+8);
    } else if (route_=="team") {
        if (action==Action::Left && focus_%2==1) --focus_;
        if (action==Action::Right && focus_%2==0) ++focus_;
        if (action==Action::Up) focus_=std::max(0,focus_-2);
        if (action==Action::Down) focus_=std::min(5,focus_+2);
    } else {
        if (action==Action::Up) focus_=focus_<=2?0:focus_-2;
        if (action==Action::Down) focus_=focus_==0?1:std::min(4,focus_+2);
        if (action==Action::Left && focus_>0 && focus_%2==0) --focus_;
        if (action==Action::Right && focus_%2==1) ++focus_;
    }
    emit changed();
}
}
