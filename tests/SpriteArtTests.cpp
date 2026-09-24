#include "features/pokedex/SpriteArt.h"
#include "features/pokedex/PokedexController.h"
#include "features/center/PartyPresentation.h"
#include "core/repository/OfflinePokedex.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QCryptographicHash>
#include <QBuffer>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
using namespace trainer;
class SpriteArtTests final : public QObject {
    Q_OBJECT
    QString fixture(const QString& dir, const QSize& size = QSize(40,40), const QString& path = {}) {
        QImage image(size,QImage::Format_ARGB32); image.fill(Qt::green);
        QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); image.save(&buffer,"PNG");
        const auto id = QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
        QDir(dir).mkpath("images"); QFile file(dir+"/images/"+id+".png");
        if (!file.open(QIODevice::WriteOnly)) return {};
        file.write(bytes); file.close();
        QJsonObject asset{{"file",path.isEmpty()?"images/"+id+".png":path},{"credit","Original test fixture"},
            {"source","TrainerOS test"},{"license","Test-only original geometry"}};
        QJsonObject root{{"version",1},{"kind","pmd-detail-preview"},
            {"targets",QJsonObject{{"vulpix/37",QJsonArray{QJsonObject{{"asset",id},{"label","Portrait"}}}}}},
            {"assets",QJsonObject{{id,asset}}}};
        QFile manifest(dir+"/sprite-index.json"); if (!manifest.open(QIODevice::WriteOnly)) return {};
        manifest.write(QJsonDocument(root).toJson()); return id;
    }
private slots:
    void partyKeepsPortraitsSeparateFromAnimationAndExactForm() {
        QTemporaryDir dir;const auto id=fixture(dir.path());
        QFile file(dir.filePath("sprite-index.json"));QVERIFY(file.open(QIODevice::ReadOnly));
        auto root=QJsonDocument::fromJson(file.readAll()).object();file.close();
        auto assets=root["assets"].toObject();auto face=assets[id].toObject();
        face["kind"]="portrait";face["action"]="Normal";assets[id]=face;root["assets"]=assets;
        QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(root).toJson());file.close();
        SpriteArt sprites(dir.path());PartyPresentation party(false);party.configureArtwork(nullptr,&sprites);
        party.setAdventure("game","Game");GameProgress progress;progress.availability=ProgressAvailability::Available;
        PartySnapshot snapshot;PokemonRecord mon;mon.kind=PokemonSlotKind::Known;mon.speciesId="vulpix";mon.formId="37";
        snapshot.party.append(mon);progress.party=snapshot;progress.saveRevision="one";party.setProgress("game",progress);
        auto row=party.activities()->actors().first().toMap();
        QCOMPARE(row["portraits"].toMap()["Normal"].toMap()["url"].toString(),"image://sprite-detail/"+id);
        QVERIFY(row["clips"].toMap().isEmpty());
        progress.party->party[0].formId="10205";progress.saveRevision="two";party.setProgress("game",progress);
        QVERIFY(party.activities()->actors().first().toMap()["portraits"].toMap().isEmpty());
    }
    void fittedCompanionCropsPaddingAcrossAllFrames() {
        QTemporaryDir dir;QDir(dir.path()).mkpath("images");
        QImage sheet(40,20,QImage::Format_ARGB32);sheet.fill(Qt::transparent);
        sheet.setPixelColor(3,4,Qt::red);sheet.setPixelColor(27,12,Qt::blue);
        QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);QVERIFY(sheet.save(&buffer,"PNG"));
        const auto id=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
        QFile image(dir.filePath("images/"+id+".png"));QVERIFY(image.open(QIODevice::WriteOnly));image.write(bytes);image.close();
        QJsonObject asset{{"file","images/"+id+".png"},{"frames",2},{"frameWidth",20},{"durations",QJsonArray{100,100}},
            {"credit","Original fixture"},{"source","Test"},{"license","Test"}};
        QFile manifest(dir.filePath("sprite-index.json"));QVERIFY(manifest.open(QIODevice::WriteOnly));
        manifest.write(QJsonDocument(QJsonObject{{"version",1},{"kind","pmd-detail-preview"},{"targets",QJsonObject{}},{"assets",QJsonObject{{id,asset}}}}).toJson());manifest.close();
        SpriteArt sprites(dir.path());const auto fitted=sprites.requestImage(id+"/trimmed",nullptr,{});
        QCOMPARE(fitted.size(),QSize(10,9));QCOMPARE(fitted.pixelColor(0,0),QColor(Qt::red));QCOMPARE(fitted.pixelColor(9,8),QColor(Qt::blue));
        QCOMPARE(sprites.requestImage(id,nullptr,{}),sheet); // Original/private source is unchanged.
        QCOMPARE(sprites.requestImage(id+"/trimmed",nullptr,{}),fitted);
    }
    void exactIdentityAndBoundedDecode() {
        QTemporaryDir dir; auto id=fixture(dir.path()); QVERIFY(!id.isEmpty()); SpriteArt sprites(dir.path());
        QCOMPARE(sprites.choices("vulpix/37").size(),1);
        QVERIFY(sprites.choices("vulpix/10205").isEmpty()); // Never substitute the ordinary form.
        QVERIFY(sprites.choices("vulpix/37/shiny").isEmpty());
        QSize size; auto image=sprites.requestImage(id,&size,QSize(9999,9999));
        QCOMPARE(size,QSize(40,40)); QCOMPARE(image.size(),QSize(40,40));
        QVERIFY(sprites.requestImage("unknown",&size,{}).isNull());
        QVERIFY(SpriteArt("").choices("vulpix/37").isEmpty());
    }
    void badHashPathsAndPixelBomb() {
        QTemporaryDir dir; const auto id=fixture(dir.path());
        QFile damaged(dir.path()+"/images/"+id+".png"); QVERIFY(damaged.open(QIODevice::WriteOnly)); damaged.write("bad"); damaged.close();
        SpriteArt corrupt(dir.path()); QVERIFY(corrupt.requestImage(id,nullptr,{}).isNull());
        fixture(dir.path(),QSize(40,40),"../outside.png"); SpriteArt escaped(dir.path()); QVERIFY(escaped.choices("vulpix/37").isEmpty());
        const auto large=fixture(dir.path(),QSize(257,40)); SpriteArt oversized(dir.path()); QVERIFY(oversized.requestImage(large,nullptr,{}).isNull());
        QVERIFY(oversized.requestImage("../"+large,nullptr,{}).isNull());
    }
    void controllerRestoresIllustrationDetailAndForm() {
        QTemporaryDir dir; fixture(dir.path()); SpriteArt sprites(dir.path());
        OfflinePokedex reference; MockPokedexRepository progress; PokedexController dex(reference,progress);
        dex.configureSprites(&sprites); dex.applySearch("37"); dex.activateControl("list",0);
        const auto original=dex.detail(); dex.dispatch(Action::Down); QCOMPARE(dex.zone(),"list");
        QCOMPARE(dex.spriteChoices().size(),1); QCOMPARE(dex.detail(),original);
        dex.activateControl("rail",7); QCOMPARE(dex.zone(),"art"); dex.dispatch(Action::Back);
        dex.cycleForm(); QVERIFY(dex.spriteChoices().isEmpty());
        dex.cancelTransient(); QCOMPARE(dex.zone(),"list");
        dex.dispatch(Action::Back); QCOMPARE(dex.zone(),"rail");
    }
};
QTEST_GUILESS_MAIN(SpriteArtTests)
#include "SpriteArtTests.moc"
