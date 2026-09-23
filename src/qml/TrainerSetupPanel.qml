import QtQuick

Item {
    id: root
    required property var shell
    property bool entry: false
    readonly property var flow: shell.trainerSetup
    readonly property bool takesFocus: visible && !shell.menuOpen && !shell.keyboard.open && shell.notice.length === 0
    Item {
        anchors.fill: parent; anchors.margins: Theme.panelInset; anchors.topMargin: Theme.contentTopInset
        PageHeader { id: header; compact: true; title: root.flow.title; subtitle: root.flow.live ? "Your Trainers" : "DEVELOPMENT PREVIEW · Nothing is saved or locked" }
        MountedPanel {
            x: 0; y: header.height; width: parent.width; height: parent.height - y; color: "#d4e2d6"
            Text {
                x: 28; y: 14; width: parent.width - 56; height: 43; text: root.flow.description
                color: Theme.muted; font.pixelSize: 16; wrapMode: Text.WordWrap
            }
            ListView {
                id: setupList
                objectName: "setup-list"
                visible: !root.flow.keypad; x: 28; y: 66; width: 526; height: parent.height - y - 30
                spacing: 0; clip: true; interactive: false
                model: root.flow.rows; currentIndex: root.flow.focusIndex
                onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
                onCountChanged: positionViewAtIndex(currentIndex, ListView.Contain)
                delegate: FocusScope {
                        required property int index; required property var modelData
                        width: setupList.width; height: 66
                    CapButton {
                        focus: true
                        objectName: "setup-action-" + index
                        x: 8; y: 9; width: 510; height: 48; label: modelData.label; detail: modelData.detail
                        tint: index % 2 ? Theme.blue : Theme.yellow
                        selected: root.takesFocus && !root.flow.keypad && root.flow.focusIndex === index
                        onActivated: root.entry ? root.flow.activate(index) : root.shell.activate(index)
                        enabled: !root.flow.busy
                    }
                }
            }
            Item {
                visible: !root.flow.keypad; x: 567; y: 60; width: parent.width - x - 20; height: 230
                TrainerEmblem { anchors.horizontalCenter: parent.horizontalCenter; width: 118; height: 118; emblem: root.flow.emblem }
                Text { y: 132; width: parent.width; text: root.flow.name || (root.flow.live ? "Your Trainer card" : "Your sample card"); textFormat: Text.PlainText; elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter; color: Theme.ink; font.pixelSize: 23; font.bold: true }
                Text { y: 172; width: parent.width; text: root.flow.favorite; horizontalAlignment: Text.AlignHCenter; color: Theme.muted; font.pixelSize: 16 }
                Text { y: 202; width: parent.width; text: root.flow.stage === "review" ? root.flow.pinChoice : root.flow.canRemove ? "X  Remove this Trainer" : root.flow.live ? "Shared games \u00b7 Personal journal" : "Original Trainer emblem"; horizontalAlignment: Text.AlignHCenter; color: Theme.muted; font.pixelSize: 13 }
            }
            Item {
                visible: root.flow.keypad; x: 28; y: 65; width: parent.width - 56; height: 232
                Text { x: 0; y: 16; width: 420; text: root.flow.pinMask || "○ ○ ○ ○"; color: Theme.ink; font.pixelSize: 43; font.letterSpacing: 8; horizontalAlignment: Text.AlignHCenter }
                Text { x: 35; y: 100; width: 350; text: "D-pad · Choose a key\nA · Enter digit\nB · Previous screen"; color: Theme.muted; font.pixelSize: 18; lineHeight: 1.4 }
                Grid {
                    x: 485; y: 0; columns: 3; columnSpacing: 10; rowSpacing: 10
                    Repeater {
                        model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "⌫", "0", "Clear"]
                        delegate: CapButton {
                            required property int index; required property string modelData
                            objectName: "setup-key-" + index
                            width: 92; height: 40; label: modelData; centered: true
                            tint: index === 9 || index === 11 ? Theme.pink : Theme.blue
                            selected: root.takesFocus && root.flow.keypad && root.flow.focusIndex === index
                            onActivated: root.entry ? root.flow.activate(index) : root.shell.activate(index)
                        }
                    }
                }
                CapButton {
                    x: 485; y: 202; width: root.flow.stage === "pin" ? 144 : 296; height: 38
                    objectName: "setup-key-12"; label: root.flow.stage === "unlock" ? "Try PIN" : "Continue"; centered: true
                    selected: root.takesFocus && root.flow.keypad && root.flow.focusIndex === 12
                    onActivated: root.entry ? root.flow.activate(12) : root.shell.activate(12)
                }
                CapButton {
                    x: 637; y: 202; width: 144; height: 38; visible: root.flow.stage === "pin"
                    objectName: "setup-key-13"; label: "Skip PIN"; centered: true; tint: Theme.green
                    selected: root.takesFocus && root.flow.keypad && root.flow.focusIndex === 13
                    onActivated: root.entry ? root.flow.activate(13) : root.shell.activate(13)
                }
            }
            Text { x: 28; y: parent.height - 24; width: parent.width - 56; text: root.flow.error; color: "#853b24"; font.pixelSize: 13; elide: Text.ElideRight }
        }
    }
}
