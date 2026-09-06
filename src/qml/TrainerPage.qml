import QtQuick

Item {
    id: root
    required property var shell
    readonly property var trainer: shell.trainer
    readonly property bool takesFocus: visible && !shell.menuOpen && shell.notice.length === 0 && !shell.keyboard.open
    Text {
        x: 29; y: 20
        text: root.trainer.editing ? (root.trainer.exists ? "Edit Trainer" : "Create Trainer") : "Trainer"
        color: Theme.ink; font.pixelSize: 32; font.weight: Font.DemiBold
    }
    Text {
        x: 31; y: 66
        text: "Make this journey yours."
        color: Theme.muted; font.pixelSize: 16
    }
    Rectangle {
        x: 586; y: 0; width: parent.width - x; height: parent.height
        color: "#e1ecde"
        Rectangle { width: 1; height: parent.height; color: "#b7cbbb" }
        TrainerEmblem {
            x: 83; y: 76; width: 164; height: 164
            emblem: root.trainer.editing ? root.trainer.draftEmblem : root.trainer.profile.emblem
        }
        Text {
            x: 22; y: 259; width: parent.width - 44
            text: root.trainer.editing ? (root.trainer.draftName || "Your name") : (root.trainer.profile.name || "Your story starts here")
            textFormat: Text.PlainText; elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter; color: Theme.ink; font.pixelSize: 24; font.weight: Font.DemiBold
        }
        Text {
            x: 15; y: 296; width: parent.width - 30
            text: "PROFILE PREVIEW"; horizontalAlignment: Text.AlignHCenter
            color: Theme.muted; font.pixelSize: 11; font.letterSpacing: 1
        }
    }
    Column {
        x: 31; y: 119; spacing: 17
        visible: !root.trainer.editing
        Text { text: root.trainer.exists ? root.trainer.profile.name : "Ready to begin?"; color: Theme.ink; font.pixelSize: 31; font.weight: Font.DemiBold; width: 510; elide: Text.ElideRight; textFormat: Text.PlainText }
        Text {
            width: 490; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: 17
            text: root.trainer.exists ? "Favorite Pokémon · " + root.trainer.profile.favorite : "Choose a name, an emblem and a favorite Pokémon for your Trainer."
        }
        Text {
            width: 490; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: 14
            text: sessionState.persistent ? "Local profile · kept on this device." : "Sample profile · kept until you close this preview."
        }
    }
    Rectangle {
        x: 0; y: 99; width: 586; height: 226
        color: "#d8e5d8"; visible: root.trainer.editing
        Column {
            x: 30; y: 11; spacing: 11
            Repeater {
                model: [
                    {title: "Name", value: root.trainer.draftName || "Choose your name", hint: "A · Open keyboard"},
                    {title: "Emblem", value: root.trainer.draftEmblem, hint: "A · Next emblem"},
                    {title: "Favorite", value: root.trainer.draftFavorite, hint: "A · Next sample Pokémon"}
                ]
                delegate: CapButton {
                    required property int index
                    required property var modelData
                    objectName: "trainer-field-" + index
                    width: 526; height: 55
                    label: modelData.title + " · " + modelData.value; detail: modelData.hint
                    tint: index === 0 ? Theme.blue : index === 1 ? Theme.green : Theme.pink
                    selected: root.trainer.editing && root.takesFocus && root.trainer.focusIndex === index
                    onActivated: root.shell.activate(index)
                }
            }
        }
    }
    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 87; color: "#c6dcca"
        Text {
            x: 31; y: 7; width: parent.width - 62
            text: root.trainer.saving ? "Saving… You can leave this page; your save will finish." : root.trainer.error.length ? root.trainer.error : root.trainer.editing ? "Save keeps your profile. B discards changes before Save." : "Your identity, your next Adventure."
            color: root.trainer.error.length ? "#853b24" : Theme.muted
            font.pixelSize: 12; elide: Text.ElideRight
        }
        CapButton {
            objectName: root.trainer.editing ? "trainer-save" : "trainer-open"
            x: 30; y: 34; width: 270; height: 40
            label: root.trainer.saving ? "Saving…" : root.trainer.editing ? "Save Trainer" : root.trainer.exists ? "Edit Trainer" : "Create Trainer"
            selected: root.takesFocus && (!root.trainer.editing || root.trainer.focusIndex === 3)
            onActivated: root.shell.activate(root.trainer.editing ? 3 : 0)
        }
        CapButton {
            objectName: "trainer-cancel"
            x: 321; y: 34; width: 235; height: 40
            visible: root.trainer.editing; tint: Theme.pink; label: root.trainer.saving ? "Back to profile" : "Cancel"
            selected: root.trainer.editing && root.takesFocus && root.trainer.focusIndex === 4
            onActivated: root.shell.activate(4)
        }
    }
}
