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
        width: root.shell.notice.length > 0 || root.shell.powerMenu ? 376 : 570; color: Theme.chassis
        Panel {
            anchors.fill: parent; anchors.margins: 8
            PageHeader {
                y: 10; compact: true
                title: root.shell.modeConfirmation ? "Before you go" : root.shell.notice.length > 0 ? "TrainerOS" : root.shell.powerMenu ? "Power" : "System menu"
            }
            MountedPanel {
                x: 9; y: 65; width: parent.width - 18; height: parent.height - y - 9
                color: "#d1e0d6"
                visible: root.shell.notice.length === 0
                Text { x: 19; y: 10; text: root.shell.powerMenu ? "YOUR HANDHELD" : "YOUR FIELD GEAR"; color: Theme.muted; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1 }
                Repeater {
                    model: root.shell.menuItems
                    delegate: CapButton {
                        required property int index
                        required property string modelData
                        readonly property bool quick: !root.shell.powerMenu && index >= 7
                        objectName: "menu-" + index
                        x: quick ? 306 : 18
                        y: quick ? 38 + (index - 7) * 112 : 32 + index * 45
                        width: quick ? parent.width - 324 : root.shell.powerMenu ? parent.width - 36 : 272
                        height: quick ? 88 : 39
                        textSize: quick ? 17 : 15
                        label: modelData
                        detail: quick ? root.shell.device.rows[index - 7].value.split("   ")[0] : ""
                        warning: root.shell.powerMenu && index < 2
                        tint: (quick && detail === "Unavailable") || (root.shell.powerMenu && index === 2) ? "#c4cdc7" : quick ? Theme.yellow : root.shell.powerMenu && index < 2 ? Theme.pink : index >= 4 ? Theme.blue : Theme.green
                        selected: root.shell.menuOpen && root.shell.notice.length === 0 && root.shell.focusIndex === index
                        onActivated: root.shell.activate(index)
                    }
                }
                Text {
                    visible: !root.shell.powerMenu
                    x: 306; y: 244; width: parent.width - 324
                    text: "Left / Right adjust\nA on Volume mutes\n\n" + (root.shell.focusIndex >= 7 ? "X Services" : "X Quick controls") + "\nUp / Down selects"
                    color: Theme.muted; font.pixelSize: 13; wrapMode: Text.WordWrap; lineHeight: 1.3
                }
                Text {
                    x: 18; y: parent.height - 37; width: parent.width - 36; height: 33
                    text: root.shell.device.error; visible: !root.shell.powerMenu
                    color: "#853b24"; font.pixelSize: 12; wrapMode: Text.WordWrap
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
