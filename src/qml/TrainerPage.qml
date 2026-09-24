import QtQuick

Item {
    id: root
    required property var shell
    readonly property var trainer: shell.trainer
    enabled: !shell.drawerOpen
    readonly property bool takesFocus: visible && !shell.drawerOpen && !shell.menuOpen && shell.notice.length === 0 && !shell.keyboard.open && !trainer.picker.open
    PageHeader {
        id: trainerHeader; width: 586
        title: root.trainer.editing ? (root.trainer.exists ? "Edit Trainer" : "Create Trainer") : "Trainer"
        subtitle: "Make this journey yours."
    }
    Item {
        x: 586; y: 0; width: parent.width - x; height: parent.height
        TrainerEmblem {
            x: 83; y: 56; width: 164; height: 164
            emblem: root.trainer.editing ? root.trainer.draftEmblem : root.trainer.profile.emblem
        }
        Text {
            x: 22; y: 239; width: parent.width - 44
            text: root.trainer.editing ? (root.trainer.draftName || "Your name") : (root.trainer.profile.name || "Your story starts here")
            textFormat: Text.PlainText; elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter; color: Theme.ink; font.pixelSize: 24; font.weight: Font.DemiBold
        }
        Text {
            x: 15; y: 276; width: parent.width - 30
            text: root.trainer.editing ? "PROFILE PREVIEW" : "TRAINER CARD"; horizontalAlignment: Text.AlignHCenter
            color: Theme.muted; font.pixelSize: 11; font.letterSpacing: 1
        }
    }
    Column {
        x: 29; y: trainerHeader.height + 13; spacing: 9
        visible: !root.trainer.editing
        Text { text: root.trainer.exists ? root.trainer.profile.name : "Ready to begin?"; color: Theme.ink; font.pixelSize: 31; font.weight: Font.DemiBold; width: 510; elide: Text.ElideRight; textFormat: Text.PlainText }
        Text {
            width: 490; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: 17
            text: root.trainer.exists ? "Favorite Pokémon · " + root.trainer.profile.favorite : "Choose a name, an emblem and a favorite Pokémon for your Trainer."
        }
    }
    MountedPanel {
        x: 0; y: 176; width: 586; height: 118; color: "#d8e5d8"
        visible: !root.trainer.editing
        Flow {
            x: 30; y: 9; width: 526; spacing: 8
            Repeater {
                model: root.trainer.overview
                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    width: index === 4 ? 344 : 168; height: 46; radius: 7; color: "#edf2e7"; border.color: "#b7cbbb"
                    Text { x: 10; y: 3; width: 148; text: modelData.value; color: Theme.ink; font.pixelSize: 20; font.bold: true; elide: Text.ElideRight }
                    Text { x: 10; y: 32; width: 148; elide: Text.ElideRight; text: modelData.label; color: Theme.muted; font.pixelSize: 11 }
                }
            }
        }
    }
    MountedPanel {
        x: 0; y: trainerHeader.height + 12; width: 586; height: 325 - y
        color: "#d8e5d8"; visible: root.trainer.editing
        Column {
            x: 30; y: 11; spacing: 11
            Repeater {
                model: [
                    {title: "Name", value: root.trainer.draftName || "Choose your name", hint: ""},
                    {title: "Emblem", value: root.trainer.draftEmblem, hint: ""},
                    {title: "Favorite", value: root.trainer.draftFavorite, hint: ""}
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
    MountedPanel {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 87; color: "#c6dcca"
        Text {
            x: 31; y: 7; width: parent.width - 62
            text: root.trainer.saving ? "Saving… You can leave this page; your save will finish." : root.trainer.error.length ? root.trainer.error : ""
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
    SpeciesPickerPanel {
        anchors.fill: parent; visible: root.trainer.picker.open
        picker: root.trainer.picker
        takesFocus: visible && !root.shell.menuOpen && !root.shell.keyboard.open && root.shell.notice.length === 0
    }
}
