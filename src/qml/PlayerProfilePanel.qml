import QtQuick

Panel {
    id: root
    required property var shell
    readonly property var profile: shell.trainer
    patterned: false
    MouseArea { anchors.fill: parent }
    PageHeader { id: title; x: 24; y: 18; width: parent.width - 48; title: "Your profile"; subtitle: "Your identity across every game" }
    SocialIconButton { anchors.right: parent.right; anchors.rightMargin: 18; y: 18; icon: "x"; label: root.profile.editing ? "Cancel editing" : "Back to Options"; onClicked: root.shell.pressButton("B") }
    TrainerEmblem { x: 28; y: 114; width: 145; height: 145; emblem: root.profile.editing ? root.profile.draftEmblem : root.profile.profile.emblem }
    Text { x: 22; y: 275; width: 164; horizontalAlignment: Text.AlignHCenter; text: root.profile.profile.name || "Your name"; textFormat: Text.PlainText; elide: Text.ElideRight; font.pixelSize: 22; font.family: Theme.displayFamily; color: Theme.ink }
    Column {
        x: 205; y: 106; width: parent.width - x - 30; spacing: 12
        Text { width: parent.width; visible: !root.profile.editing; text: root.profile.profile.name || "Create your profile"; textFormat: Text.PlainText; font.pixelSize: 29; font.bold: true; color: Theme.ink; elide: Text.ElideRight }
        Text { width: parent.width; visible: !root.profile.editing; text: "Your name and emblem stay with you when you change games. Accounts and PIN settings are available in Settings."; wrapMode: Text.WordWrap; font.pixelSize: 17; color: Theme.muted }
        CapButton { width: parent.width; height: 48; visible: !root.profile.editing; label: root.profile.exists ? "Edit profile" : "Create profile"; selected: root.visible && !root.shell.menuOpen; onActivated: root.shell.activate(0,"profile") }
        Repeater {
            model: root.profile.editing ? root.profile.editRows : []
            delegate: CapButton {
                required property var modelData
                required property int index
                width: parent.width; height: 48
                label: modelData.title; detail: modelData.detail || ""
                selected: root.profile.focusIndex === index && !root.shell.menuOpen && !root.shell.keyboard.open
                onActivated: root.shell.activate(index,"profile")
            }
        }
        Text { width: parent.width; text: root.profile.error; visible: text.length > 0; wrapMode: Text.WordWrap; font.pixelSize: 15; color: "#853b24" }
    }
    Text { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 22; text: "B · Back"; color: Theme.muted; font.pixelSize: 16 }
}
