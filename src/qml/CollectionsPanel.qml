import QtQuick

Item {
    id: root
    required property var shell
    readonly property var model: shell.collectionManager
    readonly property bool takesFocus: visible && !shell.keyboard.open && !shell.menuOpen && !shell.notice.length
    Rectangle { anchors.fill: parent; color: "#660d2525" }
    TapHandler { onTapped: {} }
    Rectangle {
        anchors.centerIn: parent; width: Math.min(640,parent.width-40); height: Math.min(420,parent.height-24)
        radius: 18; color: "#e8edda"; border.color: "#35544f"; border.width: 3
        Column {
            x: 20; y: 16; width: parent.width-40; spacing: 8
            Text { width: parent.width; text: root.model.title; textFormat: Text.PlainText; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 25; font.bold: true; elide: Text.ElideRight }
            Text { width: parent.width; text: root.model.detail; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 14; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight }
            ListView {
                id: list; width: parent.width; height: 254; clip: true; spacing: 6
                interactive: true; keyNavigationEnabled: false
                model: root.model.rows; currentIndex: root.model.focusIndex
                function reveal() {
                    if(!root.takesFocus || !currentItem)return
                    positionViewAtIndex(currentIndex,ListView.Contain)
                    currentItem.forceActiveFocus(Qt.OtherFocusReason)
                }
                onCurrentIndexChanged: Qt.callLater(reveal)
                onCurrentItemChanged: Qt.callLater(reveal)
                Connections { target: root; function onTakesFocusChanged() { Qt.callLater(list.reveal) } }
                delegate: CapButton {
                    required property int index; required property var modelData
                    objectName: "collection-option-"+index
                    width: list.width; height: 38; textSize: 17; label: modelData.label
                    tint: Theme.blue; selected: root.takesFocus && root.model.focusIndex===index
                    onActivated: root.shell.activate(index)
                }
            }
            Text { width: parent.width; text: root.model.error; visible: text.length>0; color: "#803724"; font.pixelSize: 14; wrapMode: Text.Wrap; textFormat: Text.PlainText }
        }
    }
}
