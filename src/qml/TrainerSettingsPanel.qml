import QtQuick

Item {
    id: root
    required property var shell
    readonly property bool takesFocus: visible && !shell.menuOpen && !shell.keyboard.open && !shell.hall.account.open && shell.notice.length === 0
    Item {
        visible: !root.shell.hall.account.open
        anchors.fill: parent; anchors.margins: Theme.panelInset; anchors.topMargin: Theme.contentTopInset
        PageHeader { id: header; compact: true; title: "Trainer & accounts"; subtitle: "Your identity, your journey." }
        MountedPanel {
            x: 0; y: header.height; width: parent.width; height: parent.height - y; color: "#d4e2d6"
            Column {
                x: 28; y: 20; spacing: 13
                Repeater {
                    model: [
                        {label: root.shell.trainer.exists ? "Edit Trainer" : "Create Trainer", detail: root.shell.trainer.profile.name || "Give your journey a name"},
                        {label: "RetroAchievements", detail: "Manage the connected account"},
                        {label: root.shell.sampleLibrary ? "Preview registration & PIN" : "Trainers", detail: root.shell.sampleLibrary ? "Development only · no personal changes" : "Choose a player or create a Trainer"},
                        {label: "Back to settings", detail: "B · Back"}
                    ]
                    delegate: CapButton {
                        required property int index; required property var modelData
                        objectName: "trainer-settings-" + index
                        width: 510; height: 54; label: modelData.label; detail: modelData.detail
                        tint: index === 1 ? Theme.blue : Theme.yellow
                        selected: root.takesFocus && root.shell.focusIndex === index
                        onActivated: root.shell.activate(index)
                    }
                }
            }
            TrainerEmblem { x: 622; y: 20; width: 146; height: 146; emblem: root.shell.trainer.profile.emblem || "compass" }
            Text {
                x: 580; y: 181; width: parent.width - x - 24; text: root.shell.trainer.profile.name || "Your Trainer card"
                textFormat: Text.PlainText; elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter
                color: Theme.ink; font.pixelSize: 23; font.bold: true
            }
            Text {
                x: 580; y: 224; width: parent.width - x - 24
                text: "Your library and saved profile stay on this handheld."
                wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; color: Theme.muted; font.pixelSize: 16
            }
        }
    }
    AchievementAccountPanel { anchors.fill: parent; shell: root.shell; serviceSurface: true; visible: root.shell.hall.account.open }
}
