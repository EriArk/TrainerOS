#pragma once
#include "core/input/Action.h"
#include "core/repository/PokedexRepository.h"
#include "PokedexJournalEditor.h"
#include "ClassicArt.h"
#include "SpriteArt.h"
#include <QObject>
#include <QVariantList>
#include <QJsonObject>

namespace trainer {
class PokedexController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString zone READ zone NOTIFY changed)
    Q_PROPERTY(bool saving READ saving NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(int entryIndex READ entryIndex NOTIFY changed)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY rowsChanged)
    Q_PROPERTY(QVariantMap detail READ detail NOTIFY changed)
    Q_PROPERTY(QVariantList rail READ rail NOTIFY changed)
    Q_PROPERTY(QVariantList choices READ choices NOTIFY pickerChanged)
    Q_PROPERTY(QString pickerTitle READ pickerTitle NOTIFY changed)
    Q_PROPERTY(QString query READ query NOTIFY changed)
    Q_PROPERTY(QString emptyMessage READ emptyMessage NOTIFY changed)
    Q_PROPERTY(QString recoveryLabel READ recoveryLabel NOTIFY changed)
    Q_PROPERTY(trainer::PokedexJournalEditor* journal READ journal CONSTANT)
    Q_PROPERTY(QString source READ source NOTIFY changed)
    Q_PROPERTY(QString artCoverage READ artCoverage NOTIFY changed)
    Q_PROPERTY(QVariantList artChoices READ artChoices NOTIFY changed)
    Q_PROPERTY(QVariantList spriteChoices READ spriteChoices NOTIFY changed)
    Q_PROPERTY(QVariantMap spriteClips READ spriteClips NOTIFY changed)
    Q_PROPERTY(QVariantMap spritePreview READ spritePreview NOTIFY changed)
    Q_PROPERTY(QString spriteStatus READ spriteStatus NOTIFY changed)
public:
    PokedexController(PokedexReferenceProvider&, PokedexProgressRepository&, QObject* parent = nullptr);
    QString zone() const { return zone_; }
    bool saving() const { return saving_; }
    int focusIndex() const;
    int entryIndex() const;
    QVariantList entries() const;
    QVariantMap detail() const;
    QVariantList rail() const;
    QVariantList choices() const;
    QString pickerTitle() const;
    QString query() const { return query_; }
    QString emptyMessage() const;
    QString recoveryLabel() const;
    PokedexJournalEditor* journal() { return &journal_; }
    QString source() const { return catalog_.source; }
    Q_INVOKABLE void cycleForm();
    Q_INVOKABLE void editJournal();
    void configureArtwork(ClassicArt* art);
    void configureSprites(SpriteArt* sprites);
    QVariantList spriteChoices() const;
    QString spriteStatus() const;
    QVariantMap spritePreview() const;
    QVariantMap spriteClips() const;
    Q_INVOKABLE void openSprites();
    QString artCoverage() const;
    QVariantList artChoices() const;
    Q_INVOKABLE void openArtwork();
    void dispatch(Action);
    Q_INVOKABLE void activate(int index);
    void activateControl(const QString& zone, int index);
    void applySearch(const QString& text);
    void cancelTransient();
    void refresh();
    QJsonObject navigationState() const;
    void restoreNavigation(const QJsonObject&);
signals:
    void changed();
    void rowsChanged();
    void pickerChanged();
    void searchRequested(const QString& initial);
    void messageRequested(const QString& message);
private:
    struct Choice { QString id; QString label; };
    QList<Choice> options() const;
    QVariantMap present(const PokedexEntry&, bool detailed=false) const;
    PokedexForm selectedForm(const PokedexEntry&) const;
    void rebuild();
    void reset();
    void openPicker(int railIndex);
    QString selection(int railIndex) const;
    QString selectionLabel(int railIndex) const;
    PokedexReferenceProvider& reference_;
    PokedexProgressRepository& progress_;
    PokedexJournalEditor journal_;
    PokedexCatalog catalog_;
    QHash<QString,QString> names_;
    QList<PokedexEntry> filtered_;
    QString selectedId_;
    QString formId_;
    QString query_, world_, type_, status_, sort_ = "number";
    QString zone_ = "list";
    QString error_;
    bool saving_ = false;
    int railFocus_ = 0, detailFocus_ = 0, pickerFocus_ = 0, pickerKind_ = 1;
    ClassicArt* art_ = nullptr;
    int artFocus_ = 0, spriteFocus_ = 0;
    SpriteArt* sprites_ = nullptr;
    QString artTarget() const;
};
}
