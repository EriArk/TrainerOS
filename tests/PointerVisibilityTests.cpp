#include "core/input/PointerVisibility.h"
#include <QCursor>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

using namespace trainer;
class PointerVisibilityTests final : public QObject {
    Q_OBJECT
private slots:
    void touchAndMouseStillActivateControls_data() {
        QTest::addColumn<bool>("dedicated");
        QTest::newRow("desktop") << false;
        QTest::newRow("handheld") << true;
    }
    void touchAndMouseStillActivateControls() {
        QFETCH(bool, dedicated);
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(import QtQuick
Item {
    width: 200; height: 100
    property int clicks: 0
    MouseArea { anchors.fill: parent; onClicked: parent.clicks++ }
})", QUrl());
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QQuickWindow window;
        window.resize(200, 100);
        auto* item = qobject_cast<QQuickItem*>(component.create());
        QVERIFY(item);
        item->setParentItem(window.contentItem());
        item->setParent(&window);
        PointerVisibility cursor(window, dedicated);
        QCOMPARE(window.cursor().shape(), Qt::BlankCursor);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        const QPoint point(100, 50);
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, point);
        QTRY_COMPARE(item->property("clicks").toInt(), 1);
        QCOMPARE(window.cursor().shape(), dedicated ? Qt::BlankCursor : Qt::ArrowCursor);
        auto* touch = QTest::createTouchDevice();
        QTest::touchEvent(&window, touch).press(0, point, &window);
        QTest::touchEvent(&window, touch).release(0, point, &window);
        QTRY_COMPARE(item->property("clicks").toInt(), 2);
        QCOMPARE(window.cursor().shape(), Qt::BlankCursor);
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, point);
        QTRY_COMPARE(item->property("clicks").toInt(), 3);
        QCOMPARE(window.cursor().shape(), dedicated ? Qt::BlankCursor : Qt::ArrowCursor);
        cursor.hide();
        QCOMPARE(window.cursor().shape(), Qt::BlankCursor);
        // Pointer policy belongs to this window; maintenance/other apps are free.
        QWindow other;
        QCOMPARE(other.cursor().shape(), Qt::ArrowCursor);
    }
};
QTEST_MAIN(PointerVisibilityTests)
#include "PointerVisibilityTests.moc"
