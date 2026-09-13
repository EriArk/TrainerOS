import QtQuick

Item {
    id: root
    required property var stateController
    Rectangle { anchors.fill: parent; color: Theme.chassisDark }
    Text { x: 30; y: 25; text: "TRAINER / OS"; color: "#e5eee1"; font.pixelSize: 20; font.bold: true; font.letterSpacing: 2 }
    Panel {
        x: 14; y: 68; width: parent.width - 28; height: 430
        Text { x: 32; y: 30; text: root.stateController.title; color: Theme.ink; font.pixelSize: 30; font.weight: Font.DemiBold }
        Text {
            x: 32; y: 91; width: parent.width - 64; height: 100
            text: root.stateController.message; textFormat: Text.PlainText
            wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: 21
        }
        MountedPanel {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 220; color: "#c6dcca"
            Column {
                x: 32; y: 18; spacing: 14
                Repeater {
                    model: root.stateController.choices
                    delegate: CapButton {
                        required property int index
                        required property string modelData
                        objectName: "storage-action-" + index
                        width: 580; height: 48; label: modelData
                        tint: index === 0 ? Theme.yellow : index === 1 ? Theme.blue : Theme.pink
                        selected: root.visible && root.stateController.focusIndex === index
                        onActivated: root.stateController.activate(index)
                    }
                }
            }
            Text {
                x: 32; y: 30; visible: root.stateController.choices.length === 0
                text: "One moment…"; color: Theme.muted; font.pixelSize: 20
            }
        }
    }
    Row {
        x: 30; y: 512; spacing: 30; visible: root.stateController.choices.length > 0
        Hint { button: "A"; label: "Select" }
        Hint { button: "B"; label: "Back / exit"; tint: Theme.pink }
    }
}
