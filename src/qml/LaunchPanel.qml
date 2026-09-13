import QtQuick

Item {
    required property var launch
    Rectangle { anchors.fill: parent; color: "#99081d20" }
    Panel {
        x: 12; y: 294; width: 936; height: 204
        Text { x: 30; y: 26; text: "Getting your Adventure ready"; color: Theme.ink; font.pixelSize: 28; font.bold: true }
        Text { x: 30; y: 72; width: 840; text: "Checking the selected moment and saving your place in TrainerOS…"; color: Theme.muted; font.pixelSize: 18; wrapMode: Text.WordWrap }
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 65; color: Theme.chassis
            CapButton {
                objectName: "launch-cancel"
                x: 20; y: 10; width: 260; height: 45; tint: Theme.pink
                label: "B   Cancel opening"; selected: parent.parent.parent.visible
                onActivated: launch.cancel()
            }
        }
    }
}
