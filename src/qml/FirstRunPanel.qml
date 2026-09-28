import QtQuick

Item {
    id: root
    required property var shell
    required property var flow
    Item {
        x: 34; y: 98; width: parent.width-68; height: parent.height-y-60
        Row {
            x: 2; y: 0; spacing: 7
            Repeater {
                model: 6
                Rectangle {
                    required property int index
                    width: 28; height: 5; radius: 2
                    color: index <= root.flow.step ? "#c58b20" : "#c9d4c9"
                }
            }
        }
        Text { x: 0; y: 21; width: parent.width; text: root.flow.title; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 30; font.bold: true; visible: !root.flow.connections }
        Text { x: 0; y: 66; width: parent.width; text: root.flow.description; color: Theme.muted; font.pixelSize: 17; wrapMode: Text.WordWrap; visible: !root.flow.connections }
        ConnectionsPane {
            x: 0; y: 18; width: parent.width; height: parent.height-18
            visible: root.flow.connections; shell: root.shell; onboarding: true
            Rectangle { anchors.fill: parent; color: Theme.paper; z: -1 }
        }
        Item {
            x: 0; y: 116; width: parent.width; height: parent.height-y-34
            visible: root.flow.stage === "controls"
            Row {
                anchors.centerIn: parent; spacing: 12
                Repeater {
                    model: ["↑", "↓", "←", "→", "A", "B"]
                    CapButton {
                        required property int index; required property string modelData
                        readonly property bool checked: (root.flow.checkedControls & (1 << index)) !== 0
                        width: 112; height: 102; label: modelData; textSize: 36; centered: true
                        tint: checked ? Theme.green : Theme.blue
                        selected: checked
                    }
                }
            }
        }
        TrainerEmblem {
            x: parent.width-270; y: 126; width: 210; height: 210
            emblem: "compass"
            visible: !root.flow.connections && (root.flow.stage === "welcome" || root.flow.stage === "network" || root.flow.stage === "ready")
        }
        ListView {
            id: choices
            objectName: "first-run-choices"
            x: 0; y: 119
            width: root.flow.stage === "storage" ? parent.width : parent.width-310
            height: parent.height-y-37
            visible: !root.flow.connections && root.flow.stage !== "controls"
            interactive: false; clip: true; spacing: 5
            model: root.flow.rows; currentIndex: root.flow.focusIndex
            onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
            onCountChanged: positionViewAtIndex(currentIndex,ListView.Contain)
            delegate: CapButton {
                required property int index; required property var modelData
                objectName: "first-run-choice-"+index
                x: 5; width: choices.width-10; height: root.flow.stage === "storage" ? 68 : 75
                label: modelData.label; detail: modelData.detail; textSize: 22
                tint: index % 2 ? Theme.blue : Theme.yellow
                selected: !root.flow.busy && index === root.flow.focusIndex
                enabled: !root.flow.busy
                onActivated: root.flow.activate(index)
            }
        }
        Text {
            x: 4; anchors.bottom: parent.bottom; width: parent.width; height: 32
            text: root.flow.busy ? "Preparing your library…" : root.flow.error
            textFormat: Text.PlainText; wrapMode: Text.WordWrap
            color: root.flow.error.length ? "#853b24" : Theme.muted; font.pixelSize: 15
        }
    }
}
