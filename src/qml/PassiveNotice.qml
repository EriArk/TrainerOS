import QtQuick

Rectangle {
    id: root
    required property var notice
    signal dismissed()
    width: Theme.television ? 380 : 330
    height: contents.implicitHeight + 26
    radius: 12; color: Theme.surfaceColor("#fff0bb"); border.color: Theme.surfaceColor("#8e713e"); border.width: 2
    // Dismiss by touch without stealing controller focus.
    Column {
        id: contents; x: 16; y: 13; width: parent.width - 54; spacing: 5
        Text { width: parent.width; text: root.notice.title || ""; textFormat: Text.PlainText; elide: Text.ElideRight; font.family: Theme.displayFamily; font.pixelSize: Theme.television ? 23 : 19; color: Theme.ink }
        Text { width: parent.width; text: root.notice.detail || ""; textFormat: Text.PlainText; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight; font.pixelSize: Theme.television ? 18 : 15; color: Theme.ink }
        Text { text: "Home · Notifications"; color: Theme.muted; font.pixelSize: 12 }
    }
    SocialIconButton { anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 4; icon: "x"; label: "Dismiss notification"; onClicked: root.dismissed() }
}
