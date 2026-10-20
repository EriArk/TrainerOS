import QtQuick
Item {
    id: root
    required property var importer
    readonly property var files: importer.files
    Rectangle { anchors.fill: parent; color: "#aa102022"; visible: !root.files.open }
    MouseArea { anchors.fill: parent }
    Panel {
        visible: !root.files.open
        anchors.centerIn: parent; width: Math.min(620,parent.width-48); height: Math.min(280,parent.height-48)
        Text { x: 28; y: 23; width: parent.width-95; text: "Add your game"; font.family: Theme.displayFamily; font.pixelSize: 26; color: Theme.ink }
        Text { x: 28; y: 61; width: parent.width-56; text: root.importer.title; font.pixelSize: 16; color: Theme.muted; elide: Text.ElideRight; textFormat: Text.PlainText }
        SocialIconButton { anchors.right: parent.right; anchors.rightMargin: 24; y: 23; icon: "x"; label: "Cancel"; enabled: !root.importer.saving; onClicked: root.importer.cancel() }
        Text { x: 30; y: 108; width: parent.width-60
            text: root.importer.busy ? (root.importer.saving ? "Adding to your library…" : "Checking the game and installed emulator…") : root.importer.error
            font.pixelSize: 17; color: Theme.ink; wrapMode: Text.WordWrap; textFormat: Text.PlainText }
        Text { x: 30; anchors.bottom: parent.bottom; anchors.bottomMargin: 79; width: parent.width-60
            text: "The file stays in its current location. Saves stay untouched."
            font.pixelSize: 12; color: Theme.muted; wrapMode: Text.WordWrap; textFormat: Text.PlainText }
        CapButton { x: 28; anchors.bottom: parent.bottom; anchors.bottomMargin: 24; width: 190; height: 38; label: "Choose another file"; tint: Theme.yellow
            visible: !root.importer.busy; selected: root.visible; onActivated: root.importer.chooseAgain() }
    }
    ChatFilePicker { anchors.fill: parent; picker: root.files; title: "Choose a game file" }
}
