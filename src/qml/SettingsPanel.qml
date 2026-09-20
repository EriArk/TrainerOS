import QtQuick

Item {
    id: root
    required property var shell
    readonly property var settings: shell.settings
    readonly property bool takesFocus: visible && !shell.menuOpen && shell.notice.length === 0
    // The main chassis owns the recessed surface for pages and services alike.
    Item {
        anchors.fill: parent; anchors.margins: Theme.panelInset
        anchors.topMargin: Theme.contentTopInset
        PageHeader { id: settingsHeader; compact: true; title: root.settings.creditsOpen ? "Credits" : root.settings.mediaOpen ? "Media & feedback" : "Make it yours"; subtitle: root.settings.creditsOpen ? "A little collection, made with care." : root.settings.mediaOpen ? "Your pictures, sounds and little touches." : "Same familiar controls, your favorite color." }
        MountedPanel {
            x: 0; y: settingsHeader.height; width: parent.width; height: parent.height - y; color: "#d4e2d6"
            Column {
                visible: !root.settings.creditsOpen && !root.settings.mediaDetailOpen
                x: 28; y: 12; spacing: root.settings.mediaOpen ? 10 : 5
                Repeater {
                    model: root.settings.rows
                    delegate: CapButton {
                        required property int index; required property var modelData
                        objectName: "settings-" + index
                        width: root.settings.mediaOpen ? 420 : 566; height: root.settings.mediaOpen ? 48 : 38; label: modelData.title; detail: root.settings.mediaOpen ? modelData.value : ""
                        tint: index === 0 ? Theme.yellow : index === 1 ? Theme.blue : Theme.pink
                        selected: root.takesFocus && root.settings.focusIndex === index
                        onActivated: root.shell.activate(index)
                    }
                }
            }
            Rectangle {
                visible: !root.settings.creditsOpen && !root.settings.mediaOpen
                x: 618; y: 0; width: parent.width - x; height: parent.height; color: "#e5ecdf"
                Text { x: 20; y: 18; width: parent.width - 40; text: root.settings.rows[root.settings.focusIndex].value; color: Theme.ink; font.pixelSize: 16; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter }
                TrainerEmblem { anchors.horizontalCenter: parent.horizontalCenter; y: 81; width: 144; height: 144; emblem: "spark" }
                Text { x: 20; y: 242; width: parent.width - 40; text: "YOUR SHELL COLOR"; color: Theme.muted; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; font.letterSpacing: 1 }
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter; y: 267; spacing: 12
                    Repeater {
                        model: ["turquoise", "red", "green", "blue", "orange"]
                        delegate: Rectangle {
                            required property string modelData
                            width: 28; height: 28; radius: 14; color: Theme.palettes[modelData][0]
                            border.width: modelData === Theme.themeId ? 3 : 1
                            border.color: modelData === Theme.themeId ? Theme.focus : "#718a79"
                            Rectangle { anchors.fill: parent; anchors.margins: 5; radius: 9; color: "transparent"; border.color: "#80ffffff" }
                        }
                    }
                }
            }
            Item {
                visible: root.settings.mediaOpen
                anchors.fill: parent
                Text {
                    x: root.settings.mediaDetailOpen ? 30 : 478; y: 24
                    width: parent.width - x - 28
                    text: root.settings.mediaDetailOpen ? root.settings.rows[root.settings.focusIndex].title : "CURRENT AVAILABILITY"
                    color: Theme.ink; font.pixelSize: root.settings.mediaDetailOpen ? 25 : 16; font.bold: true
                }
                Text {
                    x: root.settings.mediaDetailOpen ? 30 : 478; y: 68
                    width: parent.width - x - 28
                    text: root.settings.mediaDescription; color: Theme.ink
                    font.pixelSize: root.settings.mediaDetailOpen ? 22 : 18; wrapMode: Text.WordWrap
                }
                CapButton {
                    objectName: "media-back"; visible: root.settings.mediaDetailOpen
                    x: 28; anchors.bottom: parent.bottom; anchors.bottomMargin: 42
                    width: 310; height: 44; label: "A / B · Back"; tint: Theme.yellow
                    selected: root.takesFocus && root.settings.mediaDetailOpen
                    onActivated: root.shell.activate(0)
                }
                Text {
                    visible: !root.settings.mediaDetailOpen && !root.settings.saving && !root.settings.error.length
                    x: 30; anchors.bottom: parent.bottom; anchors.bottomMargin: 16
                    text: "A · Details / Change    B · Settings"; color: Theme.muted; font.pixelSize: 16
                }
            }
            Item {
                anchors.fill: parent
                visible: root.settings.creditsOpen
                Column {
                    x: 30; y: 22; spacing: 14; width: parent.width - 60
                    Text { text: "Pokémon League badge SVG recreations"; color: Theme.ink; font.pixelSize: 24; font.bold: true }
                    Text { text: "Stephen Griffiths · 2011"; color: Theme.ink; font.pixelSize: 20 }
                    Text { text: "Creative Commons Attribution 3.0 Unported\nhttps://creativecommons.org/licenses/by/3.0/\nhttps://github.com/SteGriff/pokemon-badges"; color: Theme.muted; font.pixelSize: 16; lineHeight: 1.3 }
                    Text { width: parent.width; text: "Adapted for TrainerOS: vector layers isolated, padded and rendered for the handheld. Badge shapes and colors preserved. Original source references: Bulbapedia.\nPokémon designs belong to their respective owners."; color: Theme.muted; font.pixelSize: 14; wrapMode: Text.WordWrap; lineHeight: 1.2 }
                }
                CapButton {
                    objectName: "credits-back"
                    anchors { right: parent.right; bottom: parent.bottom; margins: 22 }
                    width: 210; height: 48; label: "Back"; detail: "A / B"; tint: Theme.yellow
                    selected: root.takesFocus && root.settings.creditsOpen
                    onActivated: root.shell.activate(0)
                }
            }
            Text { x: 32; y: parent.height - 33; text: root.settings.saving ? "Saving…" : root.settings.error; color: "#853b24"; font.pixelSize: 14 }
        }
    }
}
