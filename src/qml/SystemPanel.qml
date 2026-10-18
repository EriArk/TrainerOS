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
    ControlsPanel { anchors.centerIn: parent; width: parent.width-24; height: parent.height-12; shell: root.shell; visible: root.shell.menuOpen && !root.shell.powerMenu && !root.shell.notice.length }
    // Existing guarded confirmations keep their own surface.
    Rectangle {
        anchors { top: parent.top; right: parent.right; bottom: parent.bottom }
        visible: root.shell.powerMenu || root.shell.notice.length > 0
        width: root.shell.notice.length > 0 || root.shell.powerMenu ? 376 : 420; color: Theme.chassis
        Panel {
            anchors.fill: parent; anchors.margins: 8
            PageHeader {
                y: 4; compact: true
                title: root.shell.modeConfirmation ? "Before you go" : root.shell.notice.length > 0 ? "TrainerOS" : root.shell.powerMenu ? "Power" : "System menu"
            }
            MountedPanel {
                x: 9; y: root.shell.powerMenu ? 65 : 48; width: parent.width - 18; height: parent.height - y - 9
                color: "#d1e0d6"
                visible: root.shell.notice.length === 0
                Column { x: 18; y: 25; width: parent.width-36; spacing: 13
                    Repeater { model: root.shell.powerMenu ? root.shell.menuItems : []
                        CapButton { required property int index; required property string modelData
                            objectName: "menu-"+index; width: parent.width; height: 43; textSize: 18
                            label: modelData; warning: index<2; tint: index<2?Theme.pink:Theme.green
                            selected: root.shell.focusIndex===index; onActivated: root.shell.activate(index)
                        }
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
                    x: 18; y: 15; width: 160; height: 42; label: root.shell.modeConfirmation ? "Continue" : "Got it"
                    selected: root.shell.notice.length > 0
                    onActivated: root.shell.activate(0)
                }
            }
        }
    }
    MouseArea { anchors.fill: parent; z: -1 } // Block pointer activation beneath the modal.
}
