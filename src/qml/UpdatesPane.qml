import QtQuick

Item {
    id: root
    required property var flow
    property bool takesFocus: false
    readonly property bool compact: width<460
    Text { id: heading; x: 5; y: 12; text: root.flow.notesOpen ? "What's new" : "System · Updates"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 25 }
    Rectangle {
        id: confirmation; x: 4; y: 52; width: parent.width-12; height: Math.max(54,question.height+20)
        visible: root.flow.confirming && !root.flow.notesOpen; color: Theme.surfaceColor("#fff0be"); radius: 8
        Text { id: question; x: 10; y: 10; width: parent.width-116; text: root.flow.message; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: Theme.ink; font.pixelSize: 14 }
        CapButton { anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter; width: 88; height: 34; label: "Cancel"; tint: Theme.pink; onActivated: root.flow.activate(root.flow.actions.length-1) }
    }
    Flickable {
        id: page; x: 0; y: confirmation.visible ? confirmation.y+confirmation.height+8 : 52; width: parent.width; height: parent.height-y-10; clip: true
        visible: !root.flow.notesOpen; contentHeight: content.height+12; boundsBehavior: Flickable.StopAtBounds
        function followFocus() {
            if (!root.takesFocus || !visible) return
            const item=buttons.itemAt(root.flow.focusIndex)
            if (!item) return
            const p=item.mapToItem(content,0,0), top=p.y, bottom=p.y+item.height+8
            if (root.flow.focusIndex===0 && bottom<=height) { contentY=0; return }
            contentY=Math.max(0,Math.min(Math.max(0,contentHeight-height),top<contentY?top:bottom>contentY+height?bottom-height:contentY))
        }
        onHeightChanged: Qt.callLater(followFocus)
        Connections { target: root.flow; function onChanged() { Qt.callLater(page.followFocus) } }
        Column {
            id: content; x: 4; width: parent.width-12; spacing: 10
            Rectangle {
                width: parent.width; height: summary.height+(root.compact?16:24); radius: 10; color: Theme.surfaceColor("#eaf0e6"); border.color: Theme.surfaceColor("#a9c5b5")
                Column { id: summary; x: root.compact?8:12; y: x; width: parent.width-2*x; spacing: root.compact?4:7
                    Text { visible: !root.compact; text: "TrainerOS Software"; color: Theme.ink; font.pixelSize: 19; font.bold: true }
                    Text { objectName: "updates-installed"; width: parent.width; text: "Installed: "+root.flow.installedVersion; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: Theme.muted; font.pixelSize: root.compact?12:14 }
                    Text { width: parent.width; visible: root.flow.version.length>0; text: "Release: "+root.flow.version+(root.flow.sizeText.length?" · "+root.flow.sizeText:""); textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: Theme.ink; font.pixelSize: root.compact?14:16; font.bold: true }
                    Text { objectName: "updates-status"; width: parent.width; text: root.flow.message; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: root.flow.status==="error" || root.flow.status==="blocked" ? Theme.textColor("#853b24") : Theme.muted; font.pixelSize: root.compact?12:14 }
                    Rectangle {
                        width: parent.width; height: 5; radius: 3; color: Theme.surfaceColor("#bacfc3"); visible: root.flow.busy
                        Rectangle { width: parent.width*(root.flow.progress>=0?root.flow.progress:0.2); height: parent.height; radius: 3; color: Theme.surfaceColor("#4e8fb5")
                            SequentialAnimation on opacity { running: root.visible && page.visible && root.flow.busy && root.flow.progress<0 && Theme.motion(650)>0; loops: Animation.Infinite; NumberAnimation { to: 0.3; duration: 650 } NumberAnimation { to: 1; duration: 650 } }
                        }
                    }
                    Text { width: parent.width; visible: root.flow.lastChecked.length>0; text: "Last checked: "+root.flow.lastChecked; color: Theme.muted; font.pixelSize: root.compact?10:12 }
                }
            }
            Text { width: parent.width; visible: !root.compact; text: root.flow.channel==="preview" ? "Preview includes experimental changes. Downloading an update does not close your game." : "Beta channel. Checking does not download or install updates."; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: 13 }
            Grid {
                width: parent.width; columns: width>=460 ? 2 : 1; spacing: 8
                onPositioningComplete: Qt.callLater(page.followFocus)
                Repeater { id: buttons; model: root.flow.actions
                    CapButton {
                        required property int index; required property var modelData
                        objectName: "updates-"+modelData.id
                        width: (parent.width-(parent.columns-1)*parent.spacing)/parent.columns; height: 38
                        label: modelData.label; textSize: 14; enabled: modelData.enabled
                        selected: root.takesFocus && root.flow.focusIndex===index
                        tint: modelData.id==="download" ? Theme.green : modelData.id==="cancel" ? Theme.pink : Theme.blue
                        onActivated: root.flow.activate(index)
                    }
                }
            }
            Rectangle {
                width: parent.width; height: ownership.height+20; radius: 8; color: Theme.surfaceColor("#dde7e2")
                Column { id: ownership; x: 10; y: 10; width: parent.width-20; spacing: 6
                    Text { text: "Operating system"; color: Theme.ink; font.pixelSize: 16; font.bold: true }
                    Text { width: parent.width; text: "Armada and other host systems have their own updates. System updating is not connected here yet."; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: 13 }
                    Text { text: "Crates"; color: Theme.ink; font.pixelSize: 16; font.bold: true }
                    Text { width: parent.width; text: "Game and app updates are checked when you open Crates."; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: 13 }
                }
            }
        }
    }
    Flickable {
        id: notes; x: 5; y: 52; width: parent.width-10; height: parent.height-y-60; clip: true
        visible: root.flow.notesOpen; contentHeight: noteText.height; boundsBehavior: Flickable.StopAtBounds
        onVisibleChanged: if(visible)contentY=0
        Text { id: noteText; width: parent.width; text: root.flow.notes; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: Theme.ink; font.pixelSize: 16 }
        Connections { target: root.flow; function onNotesScrollRequested(direction) { notes.contentY=Math.max(0,Math.min(Math.max(0,notes.contentHeight-notes.height),notes.contentY+direction*80)) } }
    }
    CapButton { objectName: "updates-notes-back"; x: 5; y: parent.height-48; width: 210; height: 36; visible: root.flow.notesOpen; label: "Back to Updates"; selected: visible && root.takesFocus; onActivated: root.flow.closeNotes() }
}
