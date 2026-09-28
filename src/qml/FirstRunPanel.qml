import QtQuick

Item {
    id: root
    required property var shell
    required property var flow
    readonly property string currentStage: flow.stage
    onCurrentStageChanged: entrance.restart()
    NumberAnimation { id: entrance; target: sheet; property: "opacity"; from: 0; to: 1; duration: Theme.motion(160); easing.type: Easing.OutCubic }
    readonly property var labels: ["Welcome", "Controls", "Connection", "Games", "Trainer", "Ready"]
    Row {
        x: 237; y: 28; spacing: 10
        Repeater {
            model: root.labels
            Item {
                required property int index; required property string modelData
                width: 102; height: 39
                Rectangle { x: 7; y: 9; width: 98; height: 2; color: index<root.flow.step ? "#b7a56d" : "#dce2d6"; visible: index<5 }
                Rectangle { width: 19; height: 19; radius: 10; color: index<=root.flow.step ? "#f1ce74" : "#e0e7db"; border.color: index===root.flow.step ? "#997e42" : "#b9c9ba"
                    Text { anchors.centerIn: parent; text: index<root.flow.step ? "✓" : index+1; font.pixelSize: 10; color: Theme.ink; font.bold: true }
                }
                Text { x: -7; y: 23; text: modelData; color: index===root.flow.step ? Theme.ink : Theme.muted; font.pixelSize: 11; font.bold: index===root.flow.step }
            }
        }
    }
    Item {
        id: sheet
        x: 24; y: 91; width: parent.width-48; height: parent.height-y-53
        visible: root.flow.stage!=="trainer" && !root.flow.connections
        Rectangle { anchors.fill: parent; color: "#fffdf4" }
        Rectangle { width: 327; height: parent.height; color: "#e4eddc"
            Rectangle { x: parent.width-1; width: 1; height: parent.height; color: "#afc7b6" }
        }
        Text { x: 24; y: 18; text: "YOUR ADVENTURE KIT"; font.family: Theme.brandFamily; font.pixelSize: 13; font.letterSpacing: 1.4; color: "#5a7d70" }
        SetupIllustration { x: 4; y: 53; width: 320; height: 272; stage: root.flow.stage; checkedControls: root.flow.checkedControls }
        Text {
            x: 22; y: 325; width: 286; height: 48
            text: root.flow.stage==="storage" ? (root.flow.rows[root.flow.focusIndex]?.path || "Connect a memory card to add more room.")
                : root.flow.stage==="controls" ? "A little practice before you set off."
                : root.flow.stage==="network" ? "Your worlds travel with you."
                : root.flow.stage==="ready" ? "All packed. Where to first?" : "Big worlds. A little handheld."
            textFormat: Text.PlainText; color: "#547267"; font.pixelSize: root.flow.stage==="storage" ? 12 : 15
            wrapMode: root.flow.stage==="storage" ? Text.NoWrap : Text.WordWrap; horizontalAlignment: Text.AlignHCenter; maximumLineCount: 3; elide: Text.ElideMiddle
        }
        Item {
            id: content
            x: 356; y: 20; width: parent.width-x-24; height: parent.height-y-15
            Text { id: heading; width: parent.width; text: root.flow.title; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 31; wrapMode: Text.WordWrap; lineHeight: .95 }
            Text { y: heading.height+13; width: parent.width; text: root.flow.description; color: Theme.muted; font.pixelSize: 16; wrapMode: Text.WordWrap }
            StorageChoices { x: -5; y: 105; width: parent.width+10; height: parent.height-y-(root.flow.error.length || root.flow.busy ? 40 : 8); flow: root.flow; visible: root.flow.stage==="storage" }
            Item {
                x: 0; y: 135; width: parent.width; height: 185; visible: root.flow.stage==="controls"
                Row { y: 15; spacing: 12
                    Repeater { model: ["↑", "↓", "←", "→", "A", "B"]
                        Rectangle {
                            required property int index; required property string modelData
                            readonly property bool done: (root.flow.checkedControls & (1<<index))!==0
                            width: 62; height: 68; radius: 13; color: done ? "#f2d275" : "#e4ecdf"; border.color: done ? "#b99039" : "#abbfaf"; border.width: 2
                            Text { anchors.centerIn: parent; text: modelData; color: Theme.ink; font.pixelSize: 28; font.bold: true }
                            Text { anchors.horizontalCenter: parent.horizontalCenter; y: 82; text: done ? "✓" : "·"; color: done ? "#618566" : "#adbca7"; font.pixelSize: 22 }
                        }
                    }
                }
            }
            Rectangle {
                x: -29; y: parent.height-155; width: parent.width+53; height: 171
                visible: !["storage","controls"].includes(root.flow.stage)
                color: "#eef0df"
                Rectangle { width: parent.width; height: 1; color: "#c5d2be" }
                Column { x: 29; y: 19; width: parent.width-53; spacing: 13
                    Repeater { model: root.flow.rows
                        CapButton {
                            required property int index; required property var modelData
                            width: parent.width; height: 54; label: modelData.label; detail: modelData.detail
                            textSize: 20; tint: index===0 ? Theme.yellow : "#bdd9ca"
                            selected: root.visible && index===root.flow.focusIndex && !root.flow.busy
                            enabled: !root.flow.busy; onActivated: root.flow.activate(index)
                        }
                    }
                }
            }
            Text { anchors.bottom: parent.bottom; width: parent.width; height: 24; text: root.flow.busy ? "Preparing your library…" : root.flow.error; textFormat: Text.PlainText; color: "#853b24"; font.pixelSize: 13; wrapMode: Text.WordWrap }
        }
    }
    ConnectionsPane { x: 40; y: 95; width: parent.width-80; height: parent.height-y-61; shell: root.shell; onboarding: true; visible: root.flow.connections }
}
