import QtQuick

Item {
    id: root
    property var art: ({})
    property bool showLabel: false
    readonly property bool ready: picture.status === Image.Ready
    Image {
        id: picture
        anchors.fill: parent
        source: root.visible && root.art && root.art.url ? root.art.url : ""
        asynchronous: true; cache: false
        fillMode: Image.PreserveAspectFit
        // The provider only exposes validated, pre-sized PNG derivatives.
        // Runtime never loads the source corpus or crops opaque backgrounds.
    }
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width, parent.height) * 0.68; height: width
        visible: !root.ready; radius: width / 2
        color: "#edf0de"; border.color: "#adc0ae"; border.width: 1
        Text { anchors.centerIn: parent; text: "?"; color: "#799384"; font.pixelSize: parent.width * .45 }
    }
    Text {
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom }
        width: parent.width; horizontalAlignment: Text.AlignHCenter
        visible: root.showLabel && !root.ready
        text: picture.status === Image.Loading ? "Loading illustration…" : picture.status === Image.Error
              ? "Illustration unavailable" : root.art && root.art.status ? root.art.status : "No illustration installed"
        color: Theme.muted; font.pixelSize: 12; wrapMode: Text.WordWrap
    }
}
