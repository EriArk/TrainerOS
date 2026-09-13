#pragma once
#include "Action.h"
#include <QObject>
#include <QVariantList>

namespace trainer {
// Shared controller text-entry session. No knowledge of Trainer, search or storage.
class TextEntryController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString displayText READ displayText NOTIFY changed)
    Q_PROPERTY(QString layoutHint READ layoutHint NOTIFY layoutChanged)
    Q_PROPERTY(QString hint READ hint NOTIFY changed)
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(int maximumLength READ maximumLength NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(QVariantList keys READ keys NOTIFY layoutChanged)
public:
    explicit TextEntryController(QObject* parent = nullptr);
    bool isOpen() const { return open_; }
    QString title() const { return title_; }
    QString text() const { return text_; }
    QString displayText() const { return secret_ ? QString(count(), QChar(0x2022)) : text_; }
    QString layoutHint() const;
    QString hint() const { return hint_; }
    int count() const;
    int maximumLength() const { return maximumLength_; }
    int focusIndex() const { return focus_; }
    QVariantList keys() const;
    void begin(const QString& title, const QString& initial, int maximumLength, bool secret = false);
    void cancel();
    void dispatch(Action);
    Q_INVOKABLE void activate(int index);
    static int characterCount(const QString& text);
signals:
    void changed();
    void layoutChanged();
    void accepted(const QString& text);
private:
    enum class KeyKind { Character, Delete, Clear, Space, Apply };
    struct Key {
        QString id;
        QString label;
        int row;
        int column;
        int span;
        KeyKind kind;
    };
    void move(Action);
    QString label(const Key&) const;
    QList<Key> keys_;
    bool open_ = false;
    bool lowercase_ = false, secret_ = false;
    int symbolPage_ = 0;
    QString title_;
    QString text_;
    QString hint_;
    int maximumLength_ = 24;
    int focus_ = 0;
    int preferredColumn_ = 0;
};
}
