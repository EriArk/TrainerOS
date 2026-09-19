#include "features/pokedex/ClassicArt.h"
#include "features/pokedex/PokedexController.h"
#include "core/repository/OfflinePokedex.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QImage>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
using namespace trainer;
class ClassicArtTests final : public QObject {
    Q_OBJECT
    void fixture(const QString& directory, bool invalid = false) {
        QDir(directory).mkpath("images");
        QJsonObject images;
        for (const auto& id : {QString("first"), QString("second")}) {
            QJsonObject files;
            for (const auto& name : {QString("pokedexListArt"),QString("pokedexDetailArt"),QString("speciesPickerArt")}) {
                const QSize size = name == "pokedexListArt" ? QSize(96,96) : name == "speciesPickerArt" ? QSize(72,72) : QSize(480,400);
                QImage image(size, QImage::Format_ARGB32); image.fill(id == "first" ? Qt::red : Qt::blue);
                const auto file = "images/" + id + name + ".png";
                QVERIFY(image.save(QDir(directory).filePath(file)));
                files[name] = invalid && id == "first" ? "../outside.png" : file;
            }
            images[id] = QJsonObject{{"files",files},{"confidence","reviewed"}};
        }
        const QJsonObject targets{{"vulpix/37",QJsonObject{{"selected","first"},{"candidates",QJsonArray{"first","second"}},{"status","mapped"}}},
            {"vulpix/10205",QJsonObject{{"selected",QJsonValue::Null},{"candidates",QJsonArray{}},{"status","review-required"}}}};
        QFile file(directory + "/bootstrap-index.json"); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(QJsonObject{{"version",1},{"stage","flip-bootstrap"},{"images",images},{"targets",targets}}).toJson());
    }
private slots:
    void exactIdentityChoicePersistenceAndMissingFallback() {
        QTemporaryDir dir; fixture(dir.path()); ClassicArt art(dir.path());
        QVERIFY(art.image("vulpix/37","pokedexListArt")["available"].toBool());
        QVERIFY(!art.image("vulpix/10205","pokedexDetailArt")["available"].toBool());
        QVERIFY(!art.image("unknown/1","pokedexDetailArt")["available"].toBool());
        QCOMPARE(art.choices("vulpix/37").size(),2);
        QVERIFY(!art.select("vulpix/10205","first").isEmpty());
        QVERIFY(art.select("vulpix/37","second").isEmpty());
        ClassicArt reopened(dir.path()); QCOMPARE(reopened.image("vulpix/37","pokedexDetailArt")["id"].toString(),"second");
        QCOMPARE(reopened.coverage(),"Illustrations · 1 / 2 forms");
    }
    void escapedOrMissingFilesNeverLoadAndFailurePreservesChoice() {
        QTemporaryDir dir; fixture(dir.path(),true); ClassicArt art(dir.path());
        QVERIFY(!art.image("vulpix/37","pokedexListArt")["available"].toBool());
        QCOMPARE(art.choices("vulpix/37").size(),1);
        QDir(dir.path()).mkdir("choices.json");
        QVERIFY(!art.select("vulpix/37","second").isEmpty());
        QVERIFY(!art.image("vulpix/37","pokedexListArt")["available"].toBool());
    }
    void controllerDraftCancelAndFormIsolation() {
        QTemporaryDir dir; fixture(dir.path()); ClassicArt art(dir.path());
        OfflinePokedex reference; MockPokedexRepository progress; PokedexController dex(reference,progress);
        dex.configureArtwork(&art); dex.applySearch("37"); dex.activateControl("list",0);
        dex.dispatch(Action::Up); QCOMPARE(dex.zone(),"art");
        dex.dispatch(Action::Right); QCOMPARE(dex.focusIndex(),1);
        dex.dispatch(Action::Back); QCOMPARE(dex.zone(),"detail");
        QCOMPARE(art.image("vulpix/37","pokedexDetailArt")["id"].toString(),"first");
        dex.dispatch(Action::Up); dex.dispatch(Action::Right); dex.dispatch(Action::Confirm);
        QCOMPARE(dex.zone(),"detail"); QCOMPARE(art.image("vulpix/37","pokedexDetailArt")["id"].toString(),"second");
        dex.cycleForm(); QVERIFY(!dex.detail()["art"].toMap()["available"].toBool());
        dex.openArtwork(); QCOMPARE(dex.artChoices().size(),0); dex.cancelTransient(); QCOMPARE(dex.zone(),"detail");
    }
};
QTEST_GUILESS_MAIN(ClassicArtTests)
#include "ClassicArtTests.moc"
