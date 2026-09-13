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
        PageHeader { id: settingsHeader; compact: true; title: "Make it yours"; subtitle: "Same familiar controls, your favorite color." }
        MountedPanel {
            x: 0; y: settingsHeader.height; width: parent.width; height: parent.height - y; color: "#d4e2d6"
            Column {
                x: 28; y: 18; spacing: 14
                Repeater {
                    model: root.settings.rows
                    delegate: CapButton {
                        required property int index; required property var modelData
                        objectName: "settings-" + index
                        width: 566; height: 58; label: modelData.title; detail: modelData.value
                        tint: index === 0 ? Theme.yellow : index === 1 ? Theme.blue : Theme.pink
                        selected: root.takesFocus && root.settings.focusIndex === index
                        onActivated: root.shell.activate(index)
                    }
                }
            }
            Rectangle {
                x: 618; y: 0; width: parent.width - x; height: parent.height; color: "#e5ecdf"
                TrainerEmblem { anchors.horizontalCenter: parent.horizontalCenter; y: 37; width: 144; height: 144; emblem: "spark" }
                Text { x: 20; y: 204; width: parent.width - 40; text: "YOUR SHELL COLOR"; color: Theme.muted; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; font.letterSpacing: 1 }
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter; y: 235; spacing: 12
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
            Text { x: 32; y: parent.height - 33; text: root.settings.saving ? "Saving…" : root.settings.error; color: "#853b24"; font.pixelSize: 14 }
        }
    }
}
