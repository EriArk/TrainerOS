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
                Column {
                    x: 14; y: 8; width: parent.width-28; spacing: 4; visible: !root.shell.powerMenu
                    Repeater { model: 2
                        SettingControl {
                            required property int index
                            objectName: "menu-"+(index+7); width: parent.width; height: 40; compact: true
                            title: index===0 ? "Volume" : index===1 ? "Brightness" : "Shell color"
                            kind: index===0 ? "volume" : index===1 ? "brightness" : "theme"
                            level: index<2 ? root.shell.device.rows[index].level : -1
                            muted: index===0 && root.shell.device.rows[0].muted
                            selected: root.shell.menuOpen && !root.shell.notice.length && root.shell.focusIndex===index+7
                            onActivated: root.shell.activate(index+7)
                            onLevelRequested: value => root.shell.device.setQuickLevel(index,value)
                        }
                    }
                }
                Repeater {
                    model: root.shell.powerMenu ? root.shell.menuItems : [0,2,3,4,5,6].map(i => root.shell.menuItems[i])
                    CapButton {
                        required property int index; required property string modelData
                        readonly property int actionIndex: root.shell.powerMenu ? index : [0,2,3,4,5,6][index]
                        objectName: "menu-"+actionIndex; x: 14; y: root.shell.powerMenu ? 32+index*45 : 104+index*39
                        width: parent.width-28; height: 34; textSize: root.shell.powerMenu ? 15 : 15
                        label: modelData; warning: root.shell.powerMenu && index<2
                        tint: root.shell.powerMenu && index<2 ? Theme.pink : Theme.green
                        selected: root.shell.menuOpen && !root.shell.notice.length && root.shell.focusIndex===actionIndex
                        onActivated: root.shell.activate(actionIndex)
                    }
                }
                Text { x: 18; anchors.bottom: parent.bottom; anchors.bottomMargin: 3; text: "X  Quick controls    Left / Right adjust"; visible: !root.shell.powerMenu && !root.shell.device.error.length && !root.shell.settings.error.length; color: Theme.muted; font.pixelSize: 11 }
                Text {
                    x: 18; y: parent.height - 24; width: parent.width - 36; height: 33
                    text: root.shell.device.error || root.shell.settings.error; visible: !root.shell.powerMenu
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
