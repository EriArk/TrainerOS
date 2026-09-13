import QtQuick

Item {
    id: root
    required property var shell
    visible: shell.menuOpen || shell.notice.length > 0
    Rectangle { anchors.fill: parent; color: "#70102927" }
    // This module grows directly out of the right chassis edge.
    Rectangle {
        anchors { top: parent.top; right: parent.right; bottom: parent.bottom }
        width: 480; color: Theme.chassis
        Panel {
            anchors.fill: parent; anchors.margins: 8
            Text {
                x: 25; y: 21; text: root.shell.modeConfirmation ? "Change mode" : root.shell.notice.length > 0 ? "TrainerOS" : "System menu"
                color: Theme.ink; font.pixelSize: 26; font.weight: Font.DemiBold
            }
            Column {
                x: 27; y: 70; spacing: 8
                visible: root.shell.notice.length === 0
                Repeater {
                    model: root.shell.menuItems
                    delegate: CapButton {
                        required property int index
                        required property string modelData
                        objectName: "menu-" + index
                        width: 408; height: 40; textSize: 16; label: modelData
                        tint: index === 6 ? Theme.pink : index >= 4 ? Theme.blue : Theme.green
                        selected: root.shell.menuOpen && root.shell.notice.length === 0 && root.shell.focusIndex === index
                        onActivated: root.shell.activate(index)
                    }
                }
            }
            Text {
                x: 28; y: 88; width: 390; visible: root.shell.notice.length > 0
                text: root.shell.notice; wrapMode: Text.WordWrap; color: Theme.ink; font.pixelSize: 21; lineHeight: 1.25
            }
            Rectangle {
                anchors { bottom: parent.bottom; left: parent.left; right: parent.right; margins: 9 }
                height: 74; color: "#ccdcd0"; visible: root.shell.notice.length > 0
                CapButton {
                    objectName: "notice-close"
                    x: 20; y: 15; width: 180; height: 42; label: root.shell.modeConfirmation ? "A   Continue" : "A   Got it"
                    selected: root.shell.notice.length > 0
                    onActivated: root.shell.activate(0)
                }
                Text { x: 222; y: 27; text: "B   Cancel"; visible: root.shell.modeConfirmation; color: Theme.ink; font.pixelSize: 17 }
            }
        }
    }
    MouseArea { anchors.fill: parent; z: -1 } // Block pointer activation beneath the modal.
}
