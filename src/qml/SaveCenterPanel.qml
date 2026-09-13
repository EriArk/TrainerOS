import QtQuick

Item {
    id: root
    required property var shell
    readonly property var center: shell.center
    readonly property bool takesFocus: visible && !shell.menuOpen && !shell.keyboard.open && shell.notice.length === 0
    // The main chassis owns the recessed surface for pages and services alike.
    Item {
        anchors.fill: parent; anchors.margins: Theme.panelInset
        anchors.topMargin: Theme.contentTopInset; clip: true
        PageHeader { id: centerHeader; compact: true; title: root.center.title; subtitle: root.center.message; multilineStatus: true }
        MountedPanel {
            x: 0; y: centerHeader.height; width: parent.width; height: 320 - y; color: "#d8e5d8"
            ListView {
                id: list
                x: 25; y: 9; width: parent.width - 50; height: 196
                model: root.center.rows; currentIndex: root.center.focusIndex
                clip: true; interactive: false; keyNavigationEnabled: false
                function reveal() {
                    if(count && root.visible)positionViewAtIndex(currentIndex,ListView.Contain)
                    if(root.takesFocus && !root.center.confirming && !root.center.busy && currentItem)currentItem.control.forceActiveFocus(Qt.OtherFocusReason)
                }
                onCurrentIndexChanged: Qt.callLater(reveal)
                onCurrentItemChanged: Qt.callLater(reveal)
                onModelChanged: Qt.callLater(reveal)
                Connections { target: root; function onTakesFocusChanged(){Qt.callLater(list.reveal)} }
                Connections { target: root.center; function onChanged(){Qt.callLater(list.reveal)} }
                delegate: Item {
                    required property int index
                    required property var modelData
                    width: list.width; height: 64
                    property alias control: button
                    CapButton {
                        id: button; objectName: "center-row-"+index
                        x: 5; y: 5; width: parent.width - 10; height: 51
                        label: modelData.title; detail: modelData.detail; tint: modelData.available?Theme.blue:"#c4cdc7"
                        selected: root.takesFocus && !root.center.confirming && !root.center.busy && root.center.focusIndex===index
                        onActivated: root.center.activate(index)
                    }
                }
            }
            Text { x: 31; y: 52; width: parent.width - 62; visible: list.count===0; text: root.center.busy?"Checking your save shelf…":root.center.route==="adventures"?"No Adventures match. X opens search.":"No saved copies yet. Y makes a copy when an in-game save is available."; color: Theme.muted; font.pixelSize: 20; wrapMode: Text.WordWrap }
        }
        MountedPanel {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 88; color: "#c6dcca"
            Text { x: 31; y: 7; width: parent.width-62; text: root.center.route==="adventures"?(root.center.query?"Search · "+root.center.query:"↑ / ↓ Browse · ← / → Jump 8 · A Open save shelf"):"A Restore selected copy · your current save is protected first"; textFormat: Text.PlainText; elide: Text.ElideRight; color: Theme.muted; font.pixelSize: 12 }
            CapButton { objectName: "center-check"; x: 30; y: 34; width: 270; height: 40; label: root.center.route==="adventures"?"X Search":"X Check again"; tint: Theme.blue; selected: root.takesFocus && !root.center.confirming && !root.center.busy && list.count===0; onActivated: root.center.route==="adventures"?root.center.search():root.center.refresh() }
            CapButton { x: 324; y: 34; width: 270; height: 40; visible: root.center.route==="copies"; label: root.center.canCreate?"Y New save backup":"No save to copy"; tint: root.center.canCreate?Theme.yellow:"#c4cdc7"; onActivated: root.center.create() }
            CapButton { objectName: "center-back"; x: 618; y: 34; width: 266; height: 40; label: root.center.busy?"B Leave while working":"B Back"; tint: Theme.green; selected: root.takesFocus && root.center.busy; onActivated: root.center.back() }
        }
        Rectangle {
            anchors.fill: parent; visible: root.center.confirming; color: Theme.paper
            MouseArea { anchors.fill: parent }
            Rectangle { width: parent.width; height: 104; color: "#e6cecb" }
            Text { x: 30; y: 29; text: "Restore this in-game save?"; color: Theme.ink; font.pixelSize: 30; font.weight: Font.DemiBold }
            Text { x: 31; y: 135; width: parent.width-62; text: root.center.title+"\n"+root.center.restoreLabel; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 23; wrapMode: Text.WordWrap }
            Text { x: 31; y: 235; width: parent.width-62; text: "The current save will be copied first, then replaced.\nOlder saved moments stay separate."; color: Theme.muted; font.pixelSize: 18; wrapMode: Text.WordWrap }
            MountedPanel {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: 88;color:"#c6dcca"
                CapButton { objectName:"center-confirm";x:30;y:24;width:420;height:43;label:"A Protect current save and restore";tint:Theme.pink;selected:root.takesFocus&&root.center.confirming;onActivated:root.center.activate(0) }
                CapButton { x:480;y:24;width:405;height:43;label:"B Cancel";tint:Theme.blue;onActivated:root.center.back() }
            }
        }
    }
}
