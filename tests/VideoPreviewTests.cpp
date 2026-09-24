#include <QtTest>
#include <QQuickView>
#include <QQuickItem>
#include <QProcess>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QQmlError>
#include <QVideoSink>
#include <QVideoFrame>

class VideoPreviewTests:public QObject {
    Q_OBJECT
private slots:
    void decodeAndReleaseOnNavigation() {
        QTemporaryDir dir;
        const auto ffmpeg=QStandardPaths::findExecutable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(),"ffmpeg is required to generate the original video fixture");
        const auto movie=dir.filePath("fixture.mp4");
        QProcess encoder;encoder.start(ffmpeg,{"-loglevel","error","-f","lavfi","-i","testsrc2=size=160x90:rate=12","-t","2","-c:v","mpeg4","-y",movie});
        QVERIFY(encoder.waitForFinished(20000));QCOMPARE(encoder.exitCode(),0);
        QQuickView view;view.setResizeMode(QQuickView::SizeRootObjectToView);view.resize(480,270);
        view.setInitialProperties({{"video",QUrl::fromLocalFile(movie)},{"playbackAllowed",true}});
        view.setSource(QUrl::fromLocalFile(QString(TRAINER_SOURCE_DIR)+"/src/qml/GamePreview.qml"));
        QVERIFY2(view.status()==QQuickView::Ready,qPrintable(view.errors().isEmpty()?QString():view.errors().front().toString()));
        view.show();view.requestActivate();QTRY_VERIFY(view.isActive());
        auto* root=view.rootObject();QVERIFY(root);auto* loader=root->findChild<QObject*>("game-preview-loader");QVERIFY(loader);
        QVERIFY(!loader->property("active").toBool()); // Settling delay avoids one decoder per scroll event.
        QTRY_VERIFY(loader->property("item").value<QObject*>());
        auto* video=loader->property("item").value<QObject*>();QTRY_VERIFY(video->property("playing").toBool());
        auto* player=video->findChild<QObject*>("game-preview-player");QVERIFY(player);
        auto* output=player->property("videoOutput").value<QObject*>();QVERIFY(output);
        auto* sink=output->property("videoSink").value<QVideoSink*>();QVERIFY(sink);
        QSignalSpy frames(sink,&QVideoSink::videoFrameChanged);QTRY_VERIFY(frames.size()>=3);
        root->setProperty("playbackAllowed",false);QTRY_VERIFY(!loader->property("item").value<QObject*>());
        root->setProperty("video",QUrl("https://example.invalid/forbidden.mp4"));root->setProperty("playbackAllowed",true);
        QVERIFY(!root->property("eligible").toBool());QVERIFY(!loader->property("active").toBool());
        root->setProperty("video",QUrl::fromLocalFile(dir.filePath("missing.mp4")));
        QTRY_VERIFY(loader->property("item").value<QObject*>());
        QTRY_VERIFY(loader->property("item").value<QObject*>()->property("failed").toBool());
        QVERIFY(!loader->property("item").value<QObject*>()->property("playing").toBool());
        root->setProperty("visible",false);QTRY_VERIFY(!loader->property("item").value<QObject*>());
        root->setProperty("video",QUrl::fromLocalFile(movie));root->setProperty("visible",true);
        QTRY_VERIFY(loader->property("item").value<QObject*>());
        QTRY_VERIFY(loader->property("item").value<QObject*>()->property("playing").toBool());
    }
};
QTEST_MAIN(VideoPreviewTests)
#include "VideoPreviewTests.moc"
