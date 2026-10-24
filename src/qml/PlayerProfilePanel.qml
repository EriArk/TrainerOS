import QtQuick

Panel {
    id: root
    required property var shell
    readonly property var profile: shell.trainer
    patterned: false
    MouseArea { anchors.fill: parent }
    PageHeader { id: title; x: 24; y: 18; width: parent.width - 48; title: "Your profile"; subtitle: "Your identity across every game" }
    SocialIconButton { anchors.right: parent.right; anchors.rightMargin: 18; y: 18; icon: "x"; label: root.profile.editing ? "Cancel editing" : "Back to Options"; onClicked: root.shell.pressButton("B") }
    TrainerEmblem { visible: !root.profile.avatarPreview || !root.profile.avatarRendererAvailable; x: 28; y: 114; width: 145; height: 145; emblem: root.profile.editing ? root.profile.draftEmblem : root.profile.profile.emblem }
    Loader {
        id: avatarView; x: 16; y: 58; width: root.profile.avatarEditing ? parent.width*.40 : 180; height: parent.height-101
        active: !!root.profile.avatarPreview && root.profile.avatarRendererAvailable; source: active ? "AvatarPreview3D.qml" : ""
        onLoaded: item.modelSource=Qt.binding(function(){return root.profile.avatarPreview})
    }
    Text { x: 22; y: parent.height-38; width: avatarView.width; horizontalAlignment: Text.AlignHCenter; text: root.profile.profile.name || "Your name"; textFormat: Text.PlainText; elide: Text.ElideRight; font.pixelSize: 18; font.family: Theme.displayFamily; color: Theme.ink }
    Column {
        visible: !root.profile.avatarEditing
        x: 205; y: 68; width: parent.width - x - 30; spacing: 8
        Text { width: parent.width; visible: !root.profile.editing; text: root.profile.profile.name || "Create your profile"; textFormat: Text.PlainText; font.pixelSize: 29; font.bold: true; color: Theme.ink; elide: Text.ElideRight }
        Text { width: parent.width; visible: !root.profile.editing; text: "Your identity and appearance stay with you across games."; wrapMode: Text.WordWrap; font.pixelSize: 15; color: Theme.muted }
        CapButton { width: parent.width; height: 36; visible: !root.profile.editing; label: root.profile.exists ? "Edit profile" : "Create profile"; selected: root.visible && !root.shell.menuOpen && root.profile.focusIndex===0; onActivated: root.shell.activate(0,"profile") }
        CapButton { width: parent.width; height: 42; visible: !root.profile.editing && root.profile.exists; enabled: root.profile.avatarAvailable; label: "Appearance"; detail: enabled ? "Shared with your games" : "Avatar pack is not installed"; selected: root.visible && !root.shell.menuOpen && root.profile.focusIndex===1; onActivated: root.shell.activate(1,"profile") }
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
    Column {
        visible: root.profile.avatarEditing; x: parent.width*.44; y: 64; width: parent.width-x-24; spacing: 4
        Repeater {
            model: root.profile.avatarEditing ? root.profile.avatarRows.slice(0,7) : []
            delegate: CapButton {
                required property var modelData
                required property int index
                width: parent.width; height: Math.max(22, Math.min(32,(root.height-112)/7-4))
                objectName: "avatar-field-"+index
                label: modelData.title + (!modelData.colour && modelData.detail ? " · " + modelData.detail : ""); textSize: 14
                tint: Theme.yellow
                Rectangle {
                    anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter
                    width: 24; height: 18; radius: 4; visible: !!modelData.colour
                    color: modelData.colour || "transparent"; border.color: "#665c54"
                }
                enabled: !root.profile.saving
                selected: root.visible && !root.shell.menuOpen && root.profile.focusIndex===index
                onActivated: root.profile.activate(index)
            }
        }
    }
    Row {
        visible: root.profile.avatarEditing; x: root.width*.44; y: root.height-42; width: root.width-x-24; spacing: 8
        Repeater { model: ["Save", "Cancel"]
            delegate: CapButton {
                required property string modelData
                required property int index
                objectName: "avatar-field-"+(index+7)
                width: (parent.width-8)/2; height: 28; label: modelData; textSize: 14
                enabled: !root.profile.saving; selected: root.profile.focusIndex===index+7
                onActivated: root.profile.finishAvatar(index===0)
            }
        }
    }
    Text { visible: root.profile.avatarEditing; x: 24; y: 64; width: avatarView.width-16; text: root.profile.error || (root.profile.avatarError ? "Could not load the avatar pack." : root.profile.avatarBusy ? "Trying on…" : "Drag to rotate"); wrapMode: Text.Wrap; color: Theme.muted; font.pixelSize: 12 }
    Text { visible: !root.profile.avatarEditing; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 18; text: "B · Back"; color: Theme.muted; font.pixelSize: 14 }
}
