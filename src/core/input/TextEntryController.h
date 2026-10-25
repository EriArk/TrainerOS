#pragma once
#include "Action.h"
#include <QObject>
#include <QVariantList>
#include <QElapsedTimer>

namespace trainer {
// Shared editing session and standard PC key geometry. Preferences are trainer-scoped.
class TextEntryController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString language READ language NOTIFY layoutChanged)
    Q_PROPERTY(bool open READ isOpen NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString displayText READ displayText NOTIFY changed)
    Q_PROPERTY(QString layoutHint READ layoutHint NOTIFY layoutChanged)
    Q_PROPERTY(QString hint READ hint NOTIFY changed)
    Q_PROPERTY(QString submitLabel READ submitLabel NOTIFY changed)
    Q_PROPERTY(QString nextLayout READ nextLayout NOTIFY layoutChanged)
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(int maximumLength READ maximumLength NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(QVariantList keys READ keys NOTIFY layoutChanged)
    Q_PROPERTY(bool live READ live NOTIFY changed)
    Q_PROPERTY(bool secret READ secret NOTIFY changed)
    Q_PROPERTY(QString editableText READ text NOTIFY changed)
    Q_PROPERTY(int cursor READ cursor NOTIFY changed)
    Q_PROPERTY(int anchor READ anchor NOTIFY changed)
    Q_PROPERTY(bool control READ control NOTIFY layoutChanged)
    Q_PROPERTY(bool numberPad READ numberPad NOTIFY layoutChanged)
    Q_PROPERTY(bool functionRow READ functionRow NOTIFY layoutChanged)
    Q_PROPERTY(bool navigationBlock READ navigationBlock NOTIFY layoutChanged)
    Q_PROPERTY(bool systemKeys READ systemKeys NOTIFY layoutChanged)
    Q_PROPERTY(qreal layoutUnits READ layoutUnits NOTIFY layoutChanged)
    Q_PROPERTY(bool optionsOpen READ optionsOpen NOTIFY changed)
    Q_PROPERTY(int optionIndex READ optionIndex NOTIFY changed)
public:
    explicit TextEntryController(QObject* parent = nullptr);
    void configure(const QString& directory, const QString& owner);
    QString language() const { return language_; }
    Q_INVOKABLE void cycleLanguage();
    Q_INVOKABLE void insertText(const QString& text);
    Q_INVOKABLE void backspace();
    bool live() const { return live_; }
    bool secret() const { return secret_; }
    int cursor() const { return cursor_; }
    int anchor() const { return anchor_; }
    bool control() const { return control_; }
    bool numberPad() const { return numberPad_; }
    bool functionRow() const { return functionRow_; }
    bool navigationBlock() const { return navigationBlock_; }
    bool systemKeys() const { return systemKeys_; }
    qreal layoutUnits() const { return 15+(navigationBlock_?3.5:0)+(numberPad_?4.5:0); }
    bool optionsOpen() const { return optionsOpen_; }
    int optionIndex() const { return optionIndex_; }
    Q_INVOKABLE void toggleOptions();
    Q_INVOKABLE void toggleNumberPad();
    Q_INVOKABLE void toggleGroup(const QString& group);
    Q_INVOKABLE void pressKey(int key, int modifiers = 0, const QString& text = {});
    void beginLive(int hints);
    Q_INVOKABLE void select(int anchor, int cursor);
    Q_INVOKABLE void moveCursor(int delta);
    Q_INVOKABLE void shift(bool lock = false);
    Q_INVOKABLE void symbols();
    Q_INVOKABLE void paste();
    Q_INVOKABLE void nativeAction(const QString& action);
    Q_INVOKABLE void shortcut(const QString& action);
    bool isOpen() const { return open_; }
    QString title() const { return title_; }
    QString text() const { return text_; }
    QString displayText() const { return secret_ ? QString(count(), QChar(0x2022)) : text_; }
    QString layoutHint() const;
    QString hint() const { return hint_; }
    QString submitLabel() const { return submitLabel_; }
    QString nextLayout() const;
    int count() const;
    int maximumLength() const { return maximumLength_; }
    int focusIndex() const { return focus_; }
    QVariantList keys() const;
    void begin(const QString& title, const QString& initial, int maximumLength, bool secret = false,
               const QString& submitLabel = "Apply", bool emoji = false);
    void cancel();
    void dispatch(Action);
    Q_INVOKABLE void activate(int index);
    static int characterCount(const QString& text);
signals:
    void changed();
    void layoutChanged();
    void accepted(const QString& text);
    void textCommitted(QString text);
    void keyRequested(QString action);
    void keyStroke(int key, int modifiers, QString text);
private:
    enum class KeyKind { Character, Delete, Space, Apply, Shift, Caps, Control, Alt, Meta, NumLock, Native, Symbols, Paste, Options };
    struct Key {
        QString id;
        QString label;
        int row;
        qreal column;
        qreal span;
        KeyKind kind;
        bool numeric = false;
        int slot = -1;
        int code = 0;
        QString shifted;
        int height = 1;
    };
    void rebuild();
    void tapShift();
    void move(Action);
    QString language_ = "en", preferences_, preferenceKey_;
    QString label(const Key&) const;
    int code(const Key&) const;
    QList<Key> keys_;
    bool open_ = false;
    bool live_ = false, shift_ = false, capsLock_ = false, numLock_ = true;
    bool control_ = false, alt_ = false, meta_ = false;
    bool numberPad_ = true, optionsOpen_ = false;
    bool functionRow_ = true, navigationBlock_ = true, systemKeys_ = true;
    int optionIndex_ = 0;
    QElapsedTimer shiftTap_;
    struct Edit { QString text; int anchor; int cursor; };
    QList<Edit> undo_, redo_;
    int cursor_ = 0, anchor_ = 0;
    bool secret_ = false;
    int symbolPage_ = 0;
    bool emoji_ = false;
    QString submitLabel_ = "Apply";
    QString title_;
    QString text_;
    QString hint_;
    int maximumLength_ = 24;
    int focus_ = 0;
    qreal preferredColumn_ = 0;
    int preferredRow_ = 2;
};
}
