import QtQuick

Item {
    id: root
    required property var shell
    visible: shell.menuOpen || shell.notice.length > 0
    Rectangle {
        anchors.fill: parent
        anchors.topMargin: -Theme.screenBounds.y
        anchors.leftMargin: -Theme.screenBounds.x
        color: "#70102927"
    }
    // This module grows directly out of the right chassis edge.
    Rectangle {
        anchors { top: parent.top; right: parent.right; bottom: parent.bottom }
        width: 376; color: Theme.chassis
        Panel {
            anchors.fill: parent; anchors.margins: 8
            PageHeader {
                y: 10; compact: true
                title: root.shell.modeConfirmation ? "Before you go" : root.shell.notice.length > 0 ? "TrainerOS" : "System menu"
            }
            MountedPanel {
                x: 9; y: 65; width: parent.width - 18; height: parent.height - y - 9
                color: "#d1e0d6"
                visible: root.shell.notice.length === 0
                Text { x: 19; y: 10; text: "YOUR FIELD GEAR"; color: Theme.muted; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1 }
                Rectangle { x: 18; y: 205; width: parent.width - 36; height: 1; color: "#a6bcae" }
                Text { x: 19; y: 214; text: "DEVICE & SESSION"; color: Theme.muted; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1 }
                Repeater {
                    model: root.shell.menuItems
                    delegate: CapButton {
                        required property int index
                        required property string modelData
                        objectName: "menu-" + index
                        x: 18; y: index < 4 ? 29 + index * 43 : 236 + (index - 4) * 43
                        width: parent.width - 36; height: index === 6 ? 42 : 38; textSize: 16; label: modelData
                        warning: index === 6
                        tint: index === 6 ? Theme.pink : index >= 4 ? Theme.blue : Theme.green
                        selected: root.shell.menuOpen && root.shell.notice.length === 0 && root.shell.focusIndex === index
                        onActivated: root.shell.activate(index)
                    }
                }
            }
            Text {
                x: 28; y: 88; width: parent.width - 56; visible: root.shell.notice.length > 0
                height: parent.height - y - 95
                text: root.shell.notice; wrapMode: Text.WordWrap; color: Theme.ink; font.pixelSize: 19; lineHeight: 1.2
            }
            MountedPanel {
                anchors { bottom: parent.bottom; left: parent.left; right: parent.right; margins: 9 }
                height: 74; color: "#ccdcd0"; visible: root.shell.notice.length > 0
                CapButton {
                    objectName: "notice-close"
                    x: 18; y: 15; width: 160; height: 42; label: root.shell.modeConfirmation ? "A   Continue" : "A   Got it"
                    selected: root.shell.notice.length > 0
                    onActivated: root.shell.activate(0)
                }
                Text { x: 197; y: 27; text: "B   Cancel"; visible: root.shell.modeConfirmation; color: Theme.ink; font.pixelSize: 17 }
            }
        }
    }
    MouseArea { anchors.fill: parent; z: -1 } // Block pointer activation beneath the modal.
}
