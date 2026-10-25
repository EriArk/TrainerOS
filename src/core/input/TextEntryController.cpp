#include "TextEntryController.h"
#include <QTextBoundaryFinder>
#include <QSettings>
#include <QDir>
#include <QGuiApplication>
#include <QClipboard>
#include <QMap>
#include <QCryptographicHash>
#include <algorithm>
#include <limits>

namespace trainer {
TextEntryController::TextEntryController(QObject* parent) : QObject(parent) { rebuild(); }
void TextEntryController::configure(const QString& directory, const QString& owner) {
    cancel();
    preferences_ = directory.isEmpty() ? QString() : QDir(directory).filePath("keyboard.ini");
    preferenceKey_ = owner.isEmpty() ? QString() : QString::fromLatin1(QCryptographicHash::hash(owner.toUtf8(),QCryptographicHash::Sha256).toHex()) + "/language";
    touchLayout_=true;language_ = "en";numberPad_=functionRow_=navigationBlock_=systemKeys_=true;
    if(!preferences_.isEmpty()&&!preferenceKey_.isEmpty()) {
        QSettings settings(preferences_,QSettings::IniFormat);
        if(settings.value(preferenceKey_).toString()=="ru")language_="ru";
        settings.beginGroup(preferenceKey_.section('/',0,0));
        touchLayout_=settings.value("layout","touch").toString()!="pc";
        numberPad_=settings.value("number-pad",true).toBool();
        functionRow_=settings.value("function-row",true).toBool();
        navigationBlock_=settings.value("navigation-block",true).toBool();
        systemKeys_=settings.value("system-keys",true).toBool();
    }
    rebuild(); emit layoutChanged(); emit changed();
}
void TextEntryController::rebuild() {
    keys_.clear();
    if(touchLayout_){rebuildTouch();return;}
    const bool ru=language_=="ru";
    const QStringList rows=ru?QStringList{QString::fromUtf8("ЙЦУКЕНГШЩЗХЪ"),QString::fromUtf8("ФЫВАПРОЛДЖЭ"),QString::fromUtf8("ЯЧСМИТЬБЮ")}:QStringList{"QWERTYUIOP","ASDFGHJKL","ZXCVBNM"};
    const QStringList physical{"QWERTYUIOP[]","ASDFGHJKL;'","ZXCVBNM,."};
    const qreal offsets[]{1.5,1.75,2.25};int slot=0;
    for(int row=0;row<3;++row)for(int col=0;col<rows[row].size();++col) {
        const QString letter(rows[row][col]);
        keys_.append({letter,letter,row+2,offsets[row]+col,1,KeyKind::Character,false,slot++,physical[row][col].unicode()});
    }
    if(ru)keys_.append({QString::fromUtf8("Ё"),QString::fromUtf8("Ё"),1,0,1,KeyKind::Character,false,slot++,Qt::Key_QuoteLeft});
    auto add=[&](QString id,QString text,int row,qreal col,qreal span,KeyKind kind,int code=0,QString shifted={},bool pad=false,int height=1){keys_.append({id,text,row,col,span,kind,pad,-1,code,shifted,height});};
    if(!ru)add("`","`",1,0,1,KeyKind::Character,Qt::Key_QuoteLeft,"~");
    const QString shifted=ru?QString::fromUtf8("!\"№;%:?*()"):QString("!@#$%^&*()");
    for(int i=0;i<10;++i){const auto n=QString::number((i+1)%10);add(n,n,1,i+1,1,KeyKind::Character,n[0].unicode(),QString(shifted[i]));}
    add("-","-",1,11,1,KeyKind::Character,Qt::Key_Minus,"_");add("=","=",1,12,1,KeyKind::Character,Qt::Key_Equal,"+");
    add("delete","Backspace",1,13,2,KeyKind::Delete,Qt::Key_Backspace);
    add("tab","Tab",2,0,1.5,KeyKind::Native,Qt::Key_Tab);
    if(!ru){add("[","[",2,11.5,1,KeyKind::Character,Qt::Key_BracketLeft,"{");add("]","]",2,12.5,1,KeyKind::Character,Qt::Key_BracketRight,"}");}
    add("backslash","\\",2,13.5,1.5,KeyKind::Character,Qt::Key_Backslash,ru?"/":"|");
    add("caps","Caps Lock",3,0,1.75,KeyKind::Caps,Qt::Key_CapsLock);
    if(!ru){add(";",";",3,10.75,1,KeyKind::Character,Qt::Key_Semicolon,":");add("'","'",3,11.75,1,KeyKind::Character,Qt::Key_Apostrophe,"\"");}
    add("apply","Enter",3,12.75,2.25,KeyKind::Apply,Qt::Key_Return);
    add("shift","Shift",4,0,2.25,KeyKind::Shift,Qt::Key_Shift);
    if(!ru){add(",",",",4,9.25,1,KeyKind::Character,Qt::Key_Comma,"<");add(".",".",4,10.25,1,KeyKind::Character,Qt::Key_Period,">");}
    add("/",ru?".":"/",4,11.25,1,KeyKind::Character,Qt::Key_Slash,ru?",":"?");
    add("shift-right","Shift",4,12.25,2.75,KeyKind::Shift,Qt::Key_Shift);
    add("ctrl","Ctrl",5,0,1.25,KeyKind::Control,Qt::Key_Control);
    if(systemKeys_)add("win","Win",5,1.25,1.25,KeyKind::Meta,Qt::Key_Meta);
    add("alt","Alt",5,systemKeys_?2.5:1.25,1.25,KeyKind::Alt,Qt::Key_Alt);
    add("space","Space",5,systemKeys_?3.75:2.5,systemKeys_?6.25:10,KeyKind::Space,Qt::Key_Space);
    add("alt-right","Alt",5,systemKeys_?10:12.5,1.25,KeyKind::Alt,Qt::Key_Alt);
    if(systemKeys_){add("win-right","Win",5,11.25,1.25,KeyKind::Meta,Qt::Key_Meta);add("menu","Menu",5,12.5,1.25,KeyKind::Native,Qt::Key_Menu);}
    add("ctrl-right","Ctrl",5,13.75,1.25,KeyKind::Control,Qt::Key_Control);
    if(functionRow_){
        add("esc","Esc",0,0,1,KeyKind::Native,Qt::Key_Escape);
        for(int i=0;i<12;++i)add("f"+QString::number(i+1),"F"+QString::number(i+1),0,2+i+(i/4)*.5,1,KeyKind::Native,Qt::Key_F1+i);
    }
    if(navigationBlock_){
        const QStringList names{"Insert","Home","PgUp","Delete","End","PgDn"};const int codes[]{Qt::Key_Insert,Qt::Key_Home,Qt::Key_PageUp,Qt::Key_Delete,Qt::Key_End,Qt::Key_PageDown};
        for(int i=0;i<6;++i)add("nav-"+names[i],names[i],1+i/3,15.5+i%3,1,KeyKind::Native,codes[i]);
        add("up","↑",4,16.5,1,KeyKind::Native,Qt::Key_Up);add("left","←",5,15.5,1,KeyKind::Native,Qt::Key_Left);add("down","↓",5,16.5,1,KeyKind::Native,Qt::Key_Down);add("right","→",5,17.5,1,KeyKind::Native,Qt::Key_Right);
        if(functionRow_&&systemKeys_){add("print","PrtSc",0,15.5,1,KeyKind::Native,Qt::Key_Print);add("scroll","ScrLk",0,16.5,1,KeyKind::Native,Qt::Key_ScrollLock);add("pause","Pause",0,17.5,1,KeyKind::Native,Qt::Key_Pause);}
    }
    if(numberPad_){
        const qreal col=navigationBlock_?19:15.5;
        add("num-lock","Num",1,col,1,KeyKind::NumLock,Qt::Key_NumLock,{},true);
        const QString ops="/*-";for(int i=0;i<3;++i)add("num-"+QString(ops[i]),QString(ops[i]),1,col+i+1,1,KeyKind::Character,ops[i].unicode(),{},true);
        for(int i=1;i<=9;++i){auto n=QString::number(i);add("num-"+n,n,4-(i-1)/3,col+(i-1)%3,1,KeyKind::Character,Qt::Key_0+i,{},true);}
        add("num-+","+",2,col+3,1,KeyKind::Character,Qt::Key_Plus,{},true,2);
        add("num-enter","Enter",4,col+3,1,KeyKind::Apply,Qt::Key_Enter,{},true,2);
        add("num-0","0",5,col,2,KeyKind::Character,Qt::Key_0,{},true);
        add("num-decimal",".",5,col+2,1,KeyKind::Character,Qt::Key_Period,{},true);
    }
    add("layout","Layout",-1,7,2,KeyKind::Options);add("paste","Paste",-1,9,2,KeyKind::Paste);add("symbols","Symbols",-1,11,2,KeyKind::Symbols);
    focus_=0;preferredColumn_=keys_.first().column+.5;preferredRow_=keys_.first().row;
}
void TextEntryController::rebuildTouch() {
    const bool ru=language_=="ru";const qreal units=layoutUnits();int slot=0;
    auto add=[&](QString id,QString text,int row,qreal column,qreal span,KeyKind kind,int code=0,int letter=-1){keys_.append({id,text,row,column,span,kind,false,letter,code});};
    if(symbolPage_==0){
        const QStringList rows=ru?QStringList{QString::fromUtf8("ЙЦУКЕНГШЩЗХ"),QString::fromUtf8("ФЫВАПРОЛДЖЭ"),QString::fromUtf8("ЯЧСМИТЬБЮ")}:QStringList{"QWERTYUIOP","ASDFGHJKL","ZXCVBNM"};
        for(int row=0;row<3;++row){const qreal offset=(units-rows[row].size())/2;
            for(int col=0;col<rows[row].size();++col){QString c(rows[row][col]);add(c,c,row,offset+col,1,KeyKind::Character,0,slot++);}}
    }else if(symbolPage_==3){
        // Keep the same emoji catalogue as the PC layout's alternate page.
        for(int i=0;i<26;++i)add("emoji-"+QString::number(i),"",i/10,(i/10==2?2:0)+i%10,1,KeyKind::Character,0,i);
    }else{
        const QStringList rows=symbolPage_==1?QStringList{"1234567890",QString::fromUtf8("-/:;()$&@\""),".,?!'"}:QStringList{QString::fromUtf8("[]{}#%^*+="),QString::fromUtf8("_\\|~<>€£¥•"),".,?!'"};
        for(int row=0;row<3;++row)for(int col=0;col<rows[row].size();++col){QString c(rows[row][col]);add(c,c,row,(10-rows[row].size())/2.0+col,1,KeyKind::Character);}
    }
    add("shift",symbolPage_==0?"⇧":symbolPage_==1?"#+=":"123",2,0,ru&&!symbolPage_?1:1.3,symbolPage_==0?KeyKind::Shift:KeyKind::Symbols);
    add("delete","⌫",2,units-(ru&&!symbolPage_?1:1.3),ru&&!symbolPage_?1:1.3,KeyKind::Delete,Qt::Key_Backspace);
    add("symbols",symbolPage_?"ABC":"123",3,0,1.4,symbolPage_?KeyKind::Letters:KeyKind::Symbols);
    add("language",language_=="ru"?"РУС":"EN",3,1.4,1.1,KeyKind::Language);
    add("space",language_=="ru"?"пробел":"space",3,2.5,units-5,KeyKind::Space,Qt::Key_Space);
    add("apply",live_?"return":submitLabel_,3,units-2.5,2.5,KeyKind::Apply,Qt::Key_Return);
    add("layout","Layout",-1,4,2,KeyKind::Options);add("paste","Paste",-1,6,2,KeyKind::Paste);
    if(emoji_)add("emoji","☺",-1,8,2,KeyKind::Emoji);
    focus_=0;preferredColumn_=keys_.first().column+.5;preferredRow_=keys_.first().row;
}
void TextEntryController::setLayout(const QString& layout) {
    if(layout!="touch"&&layout!="pc")return;
    touchLayout_=layout=="touch";symbolPage_=0;control_=alt_=meta_=false;spaceTap_.invalidate();optionIndex_=0;
    if(!preferences_.isEmpty()&&!preferenceKey_.isEmpty()){QSettings settings(preferences_,QSettings::IniFormat);settings.setValue(preferenceKey_.section('/',0,0)+"/layout",layout);}
    rebuild();emit layoutChanged();emit changed();
}
void TextEntryController::setSymbolPage(int page) {
    symbolPage_=std::clamp(page,0,emoji_?3:2);spaceTap_.invalidate();
    if(touchLayout_)rebuild();emit layoutChanged();emit changed();
}
QStringList TextEntryController::alternatives(int index) const {
    if(!touchLayout_||index<0||index>=keys_.size())return {};
    const auto key=keys_[index];if(key.kind!=KeyKind::Character||symbolPage_==3)return {};
    static const QMap<QString,QString> variants{{"A","àáâäæãåā"},{"E","èéêëēėę"},{"I","ìíîïīį"},{"O","òóôöœõøō"},{"U","ùúûüū"},{"C","çćč"},{"N","ñń"},{"S","ßśš"},{"Y","ÿ"},{"Z","žźż"},{QString::fromUtf8("Е"),QString::fromUtf8("ё")},{QString::fromUtf8("Ь"),QString::fromUtf8("ъ")},{".","…"},{"-","–—"},{"'","‘’"},{"\"","“”«»"},{"$","€£¥₽"},{"0","°"}};
    QStringList result;QString choices=variants.value(key.id);
    if(shift_!=capsLock_)choices=choices.toUpper();
    for(const auto c:choices)result.append(QString(c));return result;
}
void TextEntryController::typeAlternative(const QString& text) {
    if(!open_||optionsOpen_)return;
    insertText(text);shift_=false;spaceTap_.invalidate();updateAutomaticCase();emit layoutChanged();
}
void TextEntryController::activateShifted(int index) {shift_=!capsLock_;activate(index);}
void TextEntryController::updateAutomaticCase() {
    if(!touchLayout_||live_||secret_||capsLock_||submitLabel_=="Go")return;
    const QString before=text_.left(std::min(anchor_,cursor_)).trimmed();
    shift_=before.isEmpty()||QString(".!?\n").contains(before.back());
}
void TextEntryController::typeSpace() {
    const bool twice=spaceTap_.isValid()&&spaceTap_.elapsed()<400;spaceTap_.invalidate();
    if(touchLayout_&&twice&&!secret_&&submitLabel_!="Go"){
        if(live_){emit keyRequested("period-space");return;}
        if(anchor_==cursor_&&cursor_>=2&&text_[cursor_-1]==' '&&text_[cursor_-2].isLetterOrNumber()){
            anchor_=cursor_-1;insertText(". ");updateAutomaticCase();return;
        }
    }
    insertText(" ");spaceTap_.start();
}
void TextEntryController::cycleLanguage() {
    language_=language_=="en"?"ru":"en";
    if(!preferences_.isEmpty()&&!preferenceKey_.isEmpty()) {
        QSettings settings(preferences_,QSettings::IniFormat);settings.setValue(preferenceKey_,language_);
    }
    rebuild();hint_.clear();emit layoutChanged();emit changed();
}
void TextEntryController::toggleOptions() {
    if(!open_)return;
    optionsOpen_=!optionsOpen_;
    if(!optionsOpen_)for(int i=0;i<keys_.size();++i)if(keys_[i].kind==KeyKind::Options){focus_=i;preferredColumn_=keys_[i].column+keys_[i].span/2;preferredRow_=keys_[i].row;break;}
    emit changed();
}
void TextEntryController::toggleNumberPad() { toggleGroup("number-pad"); }
void TextEntryController::toggleGroup(const QString& group) {
    bool* value=group=="number-pad"?&numberPad_:group=="function-row"?&functionRow_:group=="navigation-block"?&navigationBlock_:group=="system-keys"?&systemKeys_:nullptr;
    if(!value)return;*value=!*value;
    optionIndex_=1+QStringList{"number-pad","function-row","navigation-block","system-keys"}.indexOf(group);
    if(!systemKeys_)meta_=false;
    if(!preferences_.isEmpty()&&!preferenceKey_.isEmpty()) {
        QSettings settings(preferences_,QSettings::IniFormat);settings.setValue(preferenceKey_.section('/',0,0)+"/"+group,*value);
    }
    rebuild();emit layoutChanged();emit changed();
}
int TextEntryController::characterCount(const QString& text) {
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
    int count = 0;
    while (finder.toNextBoundary() > 0) ++count;
    return count;
}
int TextEntryController::count() const { return characterCount(text_); }
void TextEntryController::select(int anchor, int cursor) {
    if(!open_ || live_)return;
    spaceTap_.invalidate();
    const auto boundary=[&](int value){
        QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme,text_);
        finder.setPosition(std::clamp(value,0,int(text_.size())));
        return finder.isAtBoundary()?finder.position():qMax(0,finder.toPreviousBoundary());
    };
    anchor_=boundary(anchor);cursor_=boundary(cursor);
    emit changed();
}
void TextEntryController::moveCursor(int delta) {
    if(!open_)return;
    spaceTap_.invalidate();
    if(live_){emit keyRequested(delta<0?"left":"right");return;}
    if(cursor_!=anchor_){const int edge=delta<0?std::min(cursor_,anchor_):std::max(cursor_,anchor_);select(edge,edge);return;}
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme,text_);finder.setPosition(cursor_);
    const int next=delta<0?finder.toPreviousBoundary():finder.toNextBoundary();
    if(next>=0)select(next,next);
}
void TextEntryController::insertText(const QString& text) {
    if(!open_)return;
    if(live_){emit textCommitted(text);return;}
    QString clean;
    for(const auto c:text)if(c.isPrint()||c.isHighSurrogate()||c.isLowSurrogate()||c.category()==QChar::Other_Format)clean+=c;
    const int start=std::min(cursor_,anchor_),length=std::abs(cursor_-anchor_);
    QString result=text_;result.replace(start,length,clean);
    if(characterCount(result)>maximumLength_ && characterCount(result)>=count())hint_=QString("Maximum %1 characters reached.").arg(maximumLength_);
    else {
        if(result!=text_){undo_.append({text_,anchor_,cursor_});if(undo_.size()>64)undo_.removeFirst();redo_.clear();}
        text_=result;cursor_=anchor_=start+clean.size();hint_.clear();
    }
    emit changed();
}
void TextEntryController::backspace() {
    if(!open_)return;spaceTap_.invalidate();
    if(live_){emit keyRequested("backspace");return;}
    if(cursor_==anchor_){
        QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme,text_);finder.setPosition(cursor_);
        const int previous=finder.toPreviousBoundary();if(previous<0)return;anchor_=previous;
    }
    insertText({});updateAutomaticCase();emit layoutChanged();
}
void TextEntryController::paste() {
    spaceTap_.invalidate();
    if(live_)emit keyRequested("paste");
    else insertText(QGuiApplication::clipboard()->text());
}
void TextEntryController::nativeAction(const QString& action) {if(open_&&live_)emit keyRequested(action);}
void TextEntryController::shortcut(const QString& action) {
    if(!open_)return;
    spaceTap_.invalidate();
    if(live_){emit keyRequested(action);return;}
    if(action=="select-all")select(0,text_.size());
    else if(action=="paste")paste();
    else if(action=="undo"||action=="redo") {
        auto& source=action=="undo"?undo_:redo_;auto& destination=action=="undo"?redo_:undo_;
        if(source.isEmpty())return;
        destination.append({text_,anchor_,cursor_});const auto edit=source.takeLast();
        text_=edit.text;anchor_=edit.anchor;cursor_=edit.cursor;hint_.clear();emit changed();
    }
    else if((action=="copy"||action=="cut")&&!secret_&&cursor_!=anchor_) {
        QGuiApplication::clipboard()->setText(text_.mid(std::min(cursor_,anchor_),std::abs(cursor_-anchor_)));
        if(action=="cut")insertText({});
    }
}
void TextEntryController::shift(bool lock) {
    shiftTap_.invalidate();
    if(lock){capsLock_=true;shift_=false;}else shift_=!shift_;
    emit layoutChanged();
}
void TextEntryController::tapShift() {
    if(touchLayout_&&capsLock_){capsLock_=shift_=false;shiftTap_.invalidate();emit layoutChanged();return;}
    const bool lock=shiftTap_.isValid()&&shiftTap_.elapsed()<350;
    shift(lock);if(!lock)shiftTap_.start();
}
void TextEntryController::symbols() { dispatch(Action::ToggleContinue); }
void TextEntryController::beginLive(int hints) {
    begin("Website",{},8192,hints&Qt::ImhHiddenText,"Enter",true);
    live_=true;shift_=capsLock_=false;
    if(touchLayout_){symbolPage_=(hints&(Qt::ImhDigitsOnly|Qt::ImhFormattedNumbersOnly|Qt::ImhDialableCharactersOnly))?1:0;rebuild();}

    emit layoutChanged();emit changed();
}
QString TextEntryController::label(const Key& key) const {
    if (key.kind == KeyKind::Apply) return touchLayout_?(live_?QString("return"):submitLabel_):QString("Enter");
    if(touchLayout_&&key.kind==KeyKind::Shift)return capsLock_?QString::fromUtf8("⇪"):key.label;
    if(touchLayout_&&(key.kind==KeyKind::Symbols||key.kind==KeyKind::Letters))return key.label;
    if (key.kind == KeyKind::Control) return control_ ? "Ctrl ●" : "Ctrl";
    if (key.kind == KeyKind::Symbols) return symbolPage_==0 ? "#+=" : symbolPage_==1 ? "~[]" : symbolPage_==2&&emoji_ ? "☺" : "ABC";
    if (control_ && key.kind == KeyKind::Character && !key.numeric) return key.id;
    if(key.numeric&&!numLock_){static const QMap<QString,QString> names{{"num-0","Ins"},{"num-1","End"},{"num-2","↓"},{"num-3","PgDn"},{"num-4","←"},{"num-5","Clear"},{"num-6","→"},{"num-7","Home"},{"num-8","↑"},{"num-9","PgUp"},{"num-decimal","Del"}};if(names.contains(key.id))return names[key.id];}
    if (key.kind != KeyKind::Character || key.numeric) return key.label;
    if(key.slot<0)return shift_&&!key.shifted.isEmpty()?key.shifted:key.label;
    if(symbolPage_&&key.slot>=26)return {};
    if (symbolPage_ == 3) {
        static const QStringList emoji{
            QString::fromUtf8("😀"),QString::fromUtf8("😁"),QString::fromUtf8("😂"),QString::fromUtf8("😊"),QString::fromUtf8("😍"),QString::fromUtf8("😎"),
            QString::fromUtf8("🤔"),QString::fromUtf8("😴"),QString::fromUtf8("😢"),QString::fromUtf8("😭"),QString::fromUtf8("👍"),QString::fromUtf8("👎"),
            QString::fromUtf8("👋"),QString::fromUtf8("👏"),QString::fromUtf8("🙌"),QString::fromUtf8("🤝"),QString::fromUtf8("❤️"),QString::fromUtf8("💛"),
            QString::fromUtf8("💚"),QString::fromUtf8("💙"),QString::fromUtf8("🎉"),QString::fromUtf8("✨"),QString::fromUtf8("🔥"),QString::fromUtf8("🎮"),
            QString::fromUtf8("🏆"),QString::fromUtf8("💬")};
        return emoji.value(key.slot);
    }
    if (!symbolPage_) return (shift_!=capsLock_) ? key.label : key.label.toLower();
    QString symbols;
    for (int value = 33; value <= 126; ++value) if (!QChar(value).isLetterOrNumber()) symbols.append(QChar(value));
    const int index = (symbolPage_ - 1) * 26 + key.slot;
    return index < symbols.size() ? QString(symbols[index]) : QString();
}
QString TextEntryController::layoutHint() const {
    if(control_)return "Ctrl + A · All    C · Copy    X · Cut    V · Paste    Z · Undo    Y · Redo";
    return QString("L2/R2 · EN / RU    X · %1    Y · %2").arg(shift_!=capsLock_ ? "lowercase" : "UPPERCASE",
        nextLayout());
}
QString TextEntryController::nextLayout() const {
    return symbolPage_ == 0 ? "Symbols 1/2" : symbolPage_ == 1 ? "Symbols 2/2"
        : symbolPage_ == 2 && emoji_ ? "Emoji" : "Letters";
}
QVariantList TextEntryController::keys() const {
    QVariantList result;
    for (const auto& key : keys_) {
        result.append(QVariantMap{{"id", key.id}, {"label", label(key)}, {"visible", !label(key).isEmpty()}, {"row", key.row},
            {"column", key.column}, {"span", key.span}, {"height",key.height}, {"code",code(key)}, {"letter",key.slot>=0}, {"numeric", key.numeric},
            {"utility", key.kind!=KeyKind::Character&&key.kind!=KeyKind::Space},
            {"latched", key.kind==KeyKind::Control?control_:key.kind==KeyKind::Shift?(shift_||(touchLayout_&&capsLock_)):key.kind==KeyKind::Caps?capsLock_:key.kind==KeyKind::Alt?alt_:key.kind==KeyKind::Meta?meta_:key.kind==KeyKind::NumLock&&numLock_}});
    }
    return result;
}
void TextEntryController::begin(const QString& title, const QString& initial, int maximumLength, bool secret,
                                const QString& submitLabel, bool emoji) {
    title_ = title;
    text_ = initial; // Do not silently truncate an existing value.
    maximumLength_ = std::clamp(maximumLength, 1, 65536);
    live_=false;cursor_=anchor_=text_.size();shift_=capsLock_=control_=alt_=meta_=false;optionsOpen_=false;optionIndex_=0;undo_.clear();redo_.clear();shiftTap_.invalidate();
    hint_.clear();
    focus_ = 0;
    preferredColumn_ = keys_.first().column + 0.5;
    open_ = true;
    secret_ = secret; shift_=!secret&&initial.isEmpty()&&submitLabel!="Go"; symbolPage_ = 0;preferredRow_=keys_.first().row;
    submitLabel_ = submitLabel; emoji_ = emoji && !secret;spaceTap_.invalidate();
    if(touchLayout_)rebuild();
    emit layoutChanged();
    emit changed();
}
void TextEntryController::cancel() {
    if (!open_) return;
    open_ = false;
    text_.clear();cursor_=anchor_=0;undo_.clear();redo_.clear();
    live_=false;shift_=capsLock_=control_=alt_=meta_=false;optionsOpen_=false;hint_.clear();emit layoutChanged();
    emit changed();
}
int TextEntryController::code(const Key& key) const {
    if(key.numeric&&!numLock_) {
        static const QMap<QString,int> codes{{"num-0",Qt::Key_Insert},{"num-1",Qt::Key_End},{"num-2",Qt::Key_Down},{"num-3",Qt::Key_PageDown},{"num-4",Qt::Key_Left},{"num-5",Qt::Key_Clear},{"num-6",Qt::Key_Right},{"num-7",Qt::Key_Home},{"num-8",Qt::Key_Up},{"num-9",Qt::Key_PageUp},{"num-decimal",Qt::Key_Delete}};
        if(codes.contains(key.id))return codes[key.id];
    }
    return key.code;
}
void TextEntryController::pressKey(int key,int modifiers,const QString& text) {
    if(!open_)return;
    if(live_){emit keyStroke(key,modifiers,text);return;}
    if(modifiers&Qt::ControlModifier) {
        static const QMap<int,QString> commands{{Qt::Key_A,"select-all"},{Qt::Key_C,"copy"},{Qt::Key_X,"cut"},{Qt::Key_V,"paste"},{Qt::Key_Z,"undo"},{Qt::Key_Y,"redo"}};
        if(commands.contains(key))shortcut(commands[key]);return;
    }
    if(modifiers&(Qt::AltModifier|Qt::MetaModifier))return;
    if(key==Qt::Key_Backspace){backspace();return;}
    if(key==Qt::Key_Delete){if(cursor_==anchor_){QTextBoundaryFinder f(QTextBoundaryFinder::Grapheme,text_);f.setPosition(cursor_);int next=f.toNextBoundary();if(next<0)return;anchor_=next;}insertText({});return;}
    if(key==Qt::Key_Left||key==Qt::Key_Right||key==Qt::Key_Home||key==Qt::Key_End){
        const int oldAnchor=anchor_;int next=cursor_;
        if(key==Qt::Key_Home)next=0;else if(key==Qt::Key_End)next=text_.size();
        else {if(!(modifiers&Qt::ShiftModifier)&&anchor_!=cursor_)next=key==Qt::Key_Left?std::min(anchor_,cursor_):std::max(anchor_,cursor_);else {QTextBoundaryFinder f(QTextBoundaryFinder::Grapheme,text_);f.setPosition(cursor_);int n=key==Qt::Key_Left?f.toPreviousBoundary():f.toNextBoundary();if(n>=0)next=n;}}
        select(modifiers&Qt::ShiftModifier?oldAnchor:next,next);return;
    }
    if(key==Qt::Key_Escape){cancel();return;}
    if(key==Qt::Key_Return||key==Qt::Key_Enter){
        if(count()>maximumLength_){hint_=QString("Use up to %1 characters.").arg(maximumLength_);emit changed();return;}
        const QString result=text_;cancel();emit accepted(result);return;
    }
    if(!text.isEmpty())insertText(text);
}
void TextEntryController::move(Action action) {
    const auto current=keys_.at(focus_);int target=focus_;qreal best=std::numeric_limits<qreal>::max();int targetRow=preferredRow_;
    for(int i=0;i<keys_.size();++i){
        const auto& k=keys_[i];if(i==focus_||label(k).isEmpty())continue;
        if(action==Action::Left||action==Action::Right){
            if(preferredRow_<k.row||preferredRow_>=k.row+k.height)continue;
            qreal delta=(k.column-current.column)*(action==Action::Right?1:-1);
            if(delta>0&&delta<best){best=delta;target=i;}
        }else{
            const int row=action==Action::Down?k.row:k.row+k.height-1;
            const int delta=(row-preferredRow_)*(action==Action::Down?1:-1);if(delta<=0)continue;
            qreal distance=delta*100+std::abs(std::clamp(preferredColumn_,k.column,k.column+k.span)-preferredColumn_);
            if(distance<best){best=distance;target=i;targetRow=row;}
        }
    }
    if(target!=focus_){if(action==Action::Left||action==Action::Right)preferredColumn_=keys_[target].column+keys_[target].span/2;else preferredRow_=targetRow;focus_=target;}
}
void TextEntryController::activate(int index) {
    if(!open_||optionsOpen_||index<0||index>=keys_.size()||label(keys_[index]).isEmpty())return;
    if(focus_!=index){focus_=index;preferredColumn_=keys_[index].column+keys_[index].span/2;preferredRow_=keys_[index].row;}
    const auto key=keys_[index];hint_.clear();
    if(key.kind==KeyKind::Options){toggleOptions();return;}
    if(key.kind==KeyKind::Letters){setSymbolPage(0);return;}
    if(key.kind==KeyKind::Language){cycleLanguage();return;}
    if(key.kind==KeyKind::Emoji){setSymbolPage(symbolPage_==3?0:3);return;}
    if(key.kind==KeyKind::Symbols){if(touchLayout_)setSymbolPage(symbolPage_==1?2:1);else symbols();return;}
    if(key.kind==KeyKind::Paste){paste();return;}
    if(key.kind==KeyKind::Shift){tapShift();return;}
    if(key.kind==KeyKind::Caps){capsLock_=!capsLock_;shift_=false;}
    else if(key.kind==KeyKind::Control)control_=!control_;
    else if(key.kind==KeyKind::Alt)alt_=!alt_;
    else if(key.kind==KeyKind::Meta)meta_=!meta_;
    else if(key.kind==KeyKind::NumLock)numLock_=!numLock_;
    else {
        if(key.kind!=KeyKind::Space)spaceTap_.invalidate();
        const int modifiers=(control_?Qt::ControlModifier:0)|(alt_?Qt::AltModifier:0)|(meta_?Qt::MetaModifier:0)|(shift_?Qt::ShiftModifier:0)|(key.numeric?Qt::KeypadModifier:0);
        const bool printable=(key.kind==KeyKind::Character||key.kind==KeyKind::Space)&&code(key)==key.code;
        const QString text=printable?(key.kind==KeyKind::Space?QString(" "):label(key)):key.kind==KeyKind::Apply?QString("\r"):QString();
        if(touchLayout_&&key.kind==KeyKind::Space)typeSpace();
        else if(printable&&!control_&&!alt_&&!meta_)insertText(text);
        else if(live_&&key.kind==KeyKind::Apply&&!key.numeric&&!control_&&!alt_&&!meta_&&!shift_)emit keyRequested("enter");
        else pressKey(code(key),modifiers,text);
        shift_=control_=alt_=meta_=false;shiftTap_.invalidate();updateAutomaticCase();
        if(touchLayout_&&key.kind==KeyKind::Space&&symbolPage_){symbolPage_=0;rebuild();}
    }
    emit layoutChanged();emit changed();
}
void TextEntryController::dispatch(Action action) {
    if (!open_) return;
    if(optionsOpen_) {
        if(action==Action::Back||action==Action::ContextMenu||action==Action::LocalAction)toggleOptions();
        else if(action==Action::Up||action==Action::Down){optionIndex_=std::clamp(optionIndex_+(action==Action::Down?1:-1),0,touchLayout_?0:4);emit changed();}
        else if(action==Action::Confirm||action==Action::Left||action==Action::Right){if(!optionIndex_)setLayout(touchLayout_?"pc":"touch");else toggleGroup(QStringList{"number-pad","function-row","navigation-block","system-keys"}[optionIndex_-1]);}
        return;
    }
    if (action == Action::Back) cancel();
    else if(action==Action::ContextMenu)toggleOptions();
    else if (action == Action::PreviousFace || action == Action::NextFace) cycleLanguage();
    else if (action == Action::LocalAction) {
        for (int i=0;i<keys_.size();++i) if(keys_[i].kind==KeyKind::Apply) {activate(i);break;}
    }
    else if (action == Action::Confirm) activate(focus_);
    else if (action == Action::Secondary || action == Action::ToggleContinue) {
        if (action == Action::Secondary) { tapShift(); }
        else {symbolPage_ = (symbolPage_ + 1) % (emoji_ ? 4 : 3);if(touchLayout_)rebuild();}
        if (label(keys_[focus_]).isEmpty()) { focus_ = 0; preferredColumn_ = keys_.first().column+.5;preferredRow_=keys_.first().row; }
        hint_.clear(); emit layoutChanged(); emit changed();
    }
    else if (action <= Action::Right) { move(action); emit changed(); }
    // The shell owns Start and L1/R1; L2/R2, X and Y affect only this keyboard.
}
}
