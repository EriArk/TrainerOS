import QtQuick
Item {
    id: root
    required property var shell
    property bool overlay: false
    function hints(h) { return [h("A","Increment fixture"),h("B","Back")] }
    Rectangle {
        anchors.fill: parent; visible: !root.overlay; color: "#d5e5de"
        Text { objectName: "fixture-body"; anchors.centerIn: parent; color: "#163f3b"; font.pixelSize: 30; text: root.shell.homeView + " · " + root.shell.home.fixtureGame + " · " + root.shell.home.fixtureValue }
    }
}
