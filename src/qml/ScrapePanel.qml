import QtQuick

Item {
    id: root
    required property var shell
    readonly property var flow: shell.scraper
    readonly property bool takesFocus: visible && !shell.keyboard.open
    Rectangle { anchors.fill: parent; color: "#770d2525" }
    TapHandler { onTapped: {} }
    Rectangle {
        anchors.centerIn: parent; width: Math.min(680,parent.width-40); height: Math.min(content.implicitHeight+36,parent.height-30)
        radius: 18; color: "#e8edda"; border.color: "#35544f"; border.width: 3
        Column {
            id: content
            x: 22; y: 18; width: parent.width-44; spacing: 10
            Text { width: parent.width; text: root.flow.title; textFormat: Text.PlainText; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 26; font.bold: true }
            Text { width: parent.width; height: 78; text: root.flow.detail; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 16; wrapMode: Text.WordWrap; maximumLineCount: 4; elide: Text.ElideRight }
            ListView {
                id: list; width: parent.width; height: Math.min(190,contentHeight); clip: true; spacing: 7
                model: root.flow.rows; currentIndex: root.flow.focusIndex; keyNavigationEnabled: false
                function reveal(){if(count)positionViewAtIndex(currentIndex,ListView.Contain)}
                onCurrentIndexChanged: Qt.callLater(reveal)
                onCountChanged: Qt.callLater(reveal)
                delegate: SettingControl {
                    required property int index; required property var modelData
                    objectName: "scraper-action-"+index; width: list.width; height: modelData.detail.length?56:40
                    title: modelData.label; detail: modelData.detail; kind: modelData.enabled?"action":"unavailable"
                    selected: root.takesFocus && index===list.currentIndex
                    onActivated: root.shell.activate(index)
                }
            }
            Text { width: parent.width; visible: text.length>0; height: Math.min(48,implicitHeight); text: root.flow.status; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 14; wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight }
        }
    }
}
