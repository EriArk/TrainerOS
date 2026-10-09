import QtQuick

Rectangle {
    id: root
    required property var notice
    width: Theme.television ? 380 : 330
    height: contents.implicitHeight + 26
    radius: 12; color: "#fff0bb"; border.color: "#8e713e"; border.width: 2
    // Presentation only: no focus, pointer handler, input lease or acknowledgement.
    Column {
        id: contents; x: 16; y: 13; width: parent.width - 32; spacing: 5
        Text { width: parent.width; text: root.notice.title || ""; textFormat: Text.PlainText; elide: Text.ElideRight; font.family: Theme.displayFamily; font.pixelSize: Theme.television ? 23 : 19; color: Theme.ink }
        Text { width: parent.width; text: root.notice.detail || ""; textFormat: Text.PlainText; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight; font.pixelSize: Theme.television ? 18 : 15; color: Theme.ink }
        Text { text: "Home · Notifications"; color: Theme.muted; font.pixelSize: 12 }
    }
}
