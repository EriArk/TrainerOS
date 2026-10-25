import QtQuick
Item {
    id: root
    required property var flow
    Rectangle { anchors.fill: parent; color: Theme.surfaceColor("#99102022") }
    MouseArea { anchors.fill: parent }
    Panel {
        anchors.centerIn: parent; width: Math.min(640,parent.width-48); height: Math.min(176+72*Math.min(3,root.flow.rows.length),parent.height-42); radius: 24
        Text { x: 28; y: 25; text: root.flow.stage === "confirm" ? "Make it yours" : "Add a PC game"
            font.family: Theme.displayFamily; font.pixelSize: 28; color: Theme.ink }
        SocialIconButton { anchors.right: parent.right; anchors.rightMargin: 23; y: 21; icon: "x"; label: "Close"; onClicked: root.flow.close() }
        Text { x: 28; y: 76; width: parent.width-56; height: 54; text: root.flow.message || "Bring an installed game into your library."
            wrapMode: Text.WordWrap; textFormat: Text.PlainText; font.pixelSize: 15; color: Theme.muted }
        ControllerList { x: 25; y: 139; width: parent.width-50; height: parent.height-162; clip: true; interactive: true
            model: root.flow.rows; currentIndex: root.flow.focusIndex; takesFocus: !root.flow.picker.open
            namePrefix: "pc-game-"; tint: Theme.green; onActivated: row => root.flow.activate(row) }
    }
    ChatFilePicker { anchors.fill: parent; picker: root.flow.picker; title: root.flow.stage === "storage" ? "Choose storage" : "Choose game folder" }
}
