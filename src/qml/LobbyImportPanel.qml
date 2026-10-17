import QtQuick
Item {
    id: root
    required property var importer
    readonly property var files: importer.files
    Rectangle { anchors.fill: parent; color: "#aa102022" }
    MouseArea { anchors.fill: parent }
    Panel {
        anchors.centerIn: parent; width: Math.min(790,parent.width-48); height: Math.min(442,parent.height-48)
        Text { x: 28; y: 23; width: parent.width-95; text: "Add your game"; font.family: Theme.displayFamily; font.pixelSize: 26; color: Theme.ink }
        Text { x: 28; y: 61; width: parent.width-56; text: root.importer.title; font.pixelSize: 16; color: Theme.muted; elide: Text.ElideRight; textFormat: Text.PlainText }
        SocialIconButton { anchors.right: parent.right; anchors.rightMargin: 24; y: 23; icon: "x"; label: "Cancel"; enabled: !root.importer.saving; onClicked: root.importer.cancel() }
        Text { x: 28; y: 90; width: parent.width-56; text: root.files.path; visible: root.files.open; elide: Text.ElideMiddle; font.pixelSize: 12; color: Theme.muted; textFormat: Text.PlainText }
        ControllerList {
            x: 24; y: 115; width: Math.max(0,parent.width-48); height: Math.max(0,parent.height-211)
            visible: root.files.open; model: root.files.rows; currentIndex: root.files.rowIndex
            takesFocus: root.visible && root.files.zone === "list"; namePrefix: "lobby-import-file-"; tint: Theme.blue
            onActivated: row => root.files.activate(row,"list")
        }
        Text { x: 30; y: 143; width: parent.width-60; visible: !root.files.open
            text: root.importer.busy ? (root.importer.saving ? "Adding to your library…" : "Checking the game and installed emulator…") : root.importer.error
            font.pixelSize: 18; color: Theme.ink; wrapMode: Text.WordWrap; textFormat: Text.PlainText }
        Text { x: 30; anchors.bottom: buttons.top; anchors.bottomMargin: 12; width: parent.width-60
            text: root.files.error || "The file stays in its current location. Saves stay untouched."
            font.pixelSize: 12; color: Theme.muted; wrapMode: Text.WordWrap; textFormat: Text.PlainText }
        Row { id: buttons; x: 24; anchors.bottom: parent.bottom; anchors.bottomMargin: 23; spacing: 7; visible: root.files.open
            Repeater { model: root.files.actions
                CapButton { required property int index; required property var modelData
                    width: (root.width > 850 ? 742 : root.width-96)/6-6; height: 36; textSize: 11; label: modelData.title
                    tint: index===5 ? Theme.pink : Theme.blue; selected: root.files.zone==="actions" && root.files.focusIndex===index
                    onActivated: root.files.activate(index,"actions") }
            }
        }
        CapButton { x: 28; anchors.bottom: parent.bottom; anchors.bottomMargin: 24; width: 190; height: 38; label: "Choose another file"; tint: Theme.yellow
            visible: !root.importer.busy && !root.files.open; selected: root.visible; onActivated: root.importer.chooseAgain() }
    }
}
