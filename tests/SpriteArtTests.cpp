#include "features/pokedex/SpriteArt.h"
#include "features/pokedex/PokedexController.h"
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
        const auto original=dex.detail(); dex.dispatch(Action::Down); QCOMPARE(dex.zone(),"sprites");
        QCOMPARE(dex.spriteChoices().size(),1); dex.dispatch(Action::Right); QCOMPARE(dex.focusIndex(),0);
        dex.dispatch(Action::Secondary); QCOMPARE(dex.detail()["formId"],original["formId"]);
        dex.dispatch(Action::Back); QCOMPARE(dex.zone(),"detail"); QCOMPARE(dex.detail(),original);
        dex.dispatch(Action::Up); QCOMPARE(dex.zone(),"art"); dex.dispatch(Action::Back);
        dex.cycleForm(); dex.openSprites(); QVERIFY(dex.spriteChoices().isEmpty());
        dex.cancelTransient(); QCOMPARE(dex.zone(),"detail");
    }
};
QTEST_GUILESS_MAIN(SpriteArtTests)
#include "SpriteArtTests.moc"
