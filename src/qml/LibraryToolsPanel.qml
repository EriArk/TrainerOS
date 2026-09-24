import QtQuick

Item {
    id: root
    required property var shell
    readonly property var tools: shell.libraryTools
    readonly property bool takesFocus: visible && !shell.keyboard.open && !shell.menuOpen && !shell.notice.length && !tools.busy
    Rectangle { anchors.fill: parent; color: "#660d2525" }
    TapHandler { onTapped: {} }
    Rectangle {
        anchors.centerIn: parent; width: tools.route==="properties" ? 660 : 480
        height: Math.min(parent.height-24, header.height+body.height+list.height+error.height+44)
        radius: 18; color: "#e8edda"; border.color: "#35544f"; border.width: 3
        Rectangle { x: 3; y: 3; width: parent.width-6; height: parent.height-6; radius: 15; color: "transparent"; border.color: "#ffffff"; opacity: 0.6 }
        Column {
            x: 20; y: 18; width: parent.width-40; spacing: 8
            Text { id: header; width: parent.width; text: root.tools.title; textFormat: Text.PlainText; font.family: Theme.displayTypeface.name; font.pixelSize: 26; font.bold: true; color: Theme.ink; elide: Text.ElideRight }
            Text { id: body; width: parent.width; visible: text.length>0; height: visible?implicitHeight:0; text: root.tools.detail; textFormat: Text.PlainText; font.pixelSize: 16; color: Theme.muted; wrapMode: Text.Wrap; maximumLineCount: 7; elide: Text.ElideMiddle }
            ListView {
                id: list; width: parent.width; height: Math.min(232,contentHeight); clip: true; spacing: 6
                interactive: false; keyNavigationEnabled: false
                model: root.tools.rows; currentIndex: root.tools.focusIndex
                function reveal() {if(count)positionViewAtIndex(currentIndex,ListView.Contain)}
                onCurrentIndexChanged: Qt.callLater(reveal)
                onCountChanged: Qt.callLater(reveal)
                delegate: CapButton {
                    required property int index; required property var modelData
                    objectName: "library-tool-"+index
                    width: list.width; height: 48; textSize: 20; label: modelData.label
                    opacity: modelData.enabled ? 1 : 0.42
                    tint: root.tools.route==="remove" && index===1 ? Theme.pink : Theme.blue
                    selected: root.takesFocus && root.tools.focusIndex===index
                    onActivated: root.shell.activate(index)
                }
            }
            Text { id: error; width: parent.width; height: text.length?implicitHeight:0; text: root.tools.busy?"Saving…":root.tools.error; textFormat: Text.PlainText; font.pixelSize: 16; color: "#803724"; wrapMode: Text.WordWrap }
        }
    }
}
