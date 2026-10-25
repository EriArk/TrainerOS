import QtQuick

Item {
    id: root
    required property var shell
    visible: shell.menuOpen || shell.notice.length > 0
    Rectangle {
        anchors.fill: parent
        anchors.topMargin: -Theme.screenBounds.y
        anchors.leftMargin: -Theme.screenBounds.x
        color: Theme.surfaceColor("#70102927")
    }
    ControlsPanel { anchors.centerIn: parent; width: Math.min(implicitWidth,parent.width-32); height: Math.min(implicitHeight,parent.height-24); shell: root.shell; visible: root.shell.menuOpen && !root.shell.powerMenu && !root.shell.notice.length }
    Rectangle {
        id: powerPopup
        visible: root.shell.powerMenu && !root.shell.notice.length
        anchors.centerIn: parent
        width: Math.min(400,parent.width-32)
        height: Math.min(parent.height-12, powerContent.height+30)
        radius: 24; color: Theme.chassis; border.color: Qt.darker(Theme.chassis,1.35)
        Rectangle {anchors.fill:parent;anchors.margins:3;radius:22;color:Theme.paper;border.color:Qt.alpha("white",.5)}
        Flickable {
            anchors.fill:parent;anchors.margins:15;contentHeight:powerContent.height;clip:true
            Column {
                id: powerContent; width:parent.width;spacing:10
                Row {
                    width:parent.width;spacing:12
                    Text {width:parent.width-46;text:"Power";font.family:Theme.displayFamily;font.pixelSize:27;color:Theme.ink}
                    Text {text:"×";font.pixelSize:27;color:Theme.muted
                        MouseArea {anchors.fill:parent;anchors.margins:-6;onClicked:root.shell.activate(4)}
                    }
                }
                Text {width:parent.width;text:root.shell.powerControl.busy?"Preparing sleep…":"Rest now, pick up where you left off.";wrapMode:Text.WordWrap;color:Theme.muted;font.pixelSize:13}
                Repeater {
                    model:root.shell.menuItems
                    CapButton {
                        required property int index
                        required property string modelData
                        objectName:"menu-"+index
                        width:powerContent.width;height:index===1?54:39
                        label:index===1?"Keep awake":modelData
                        detail:index===1?"Keep downloads running with the lid closed":""
                        contentInset:index===1?46:14
                        textSize:16
                        tint:index===0?Theme.blue:index===1?Theme.green:index===3?Theme.pink:Theme.yellow
                        selected:root.shell.focusIndex===index
                        enabled: index>1 || (root.shell.powerControl.available&&!root.shell.powerControl.busy)
                        onActivated:root.shell.activate(index)
                        Rectangle {
                            visible:index===1;x:13;y:17;width:22;height:22;radius:6
                            color:root.shell.powerControl.keepAwake?Theme.chassis:Theme.paper
                            border.color:Qt.alpha(Theme.ink,.35)
                            Text {anchors.centerIn:parent;text:root.shell.powerControl.keepAwake?"✓":"";color:Theme.accentInk;font.pixelSize:17}
                        }
                    }
                }
                Text {width:parent.width;visible:text.length>0;text:root.shell.powerControl.error;wrapMode:Text.WordWrap;font.pixelSize:13;color:Theme.ink}
            }
        }
    }
    // Existing guarded confirmations keep their own surface.
    Rectangle {
        anchors { top: parent.top; right: parent.right; bottom: parent.bottom }
        visible: root.shell.notice.length > 0
        width: root.shell.notice.length > 0 || root.shell.powerMenu ? 376 : 420; color: Theme.chassis
        Panel {
            anchors.fill: parent; anchors.margins: 8
            PageHeader {
                y: 4; compact: true
                title: root.shell.modeConfirmation ? "Before you go" : root.shell.notice.length > 0 ? "TrainerOS" : root.shell.powerMenu ? "Power" : "System menu"
            }
            MountedPanel {
                x: 9; y: root.shell.powerMenu ? 65 : 48; width: parent.width - 18; height: parent.height - y - 9
                color: Theme.surfaceColor("#d1e0d6")
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
                height: 74; color: Theme.surfaceColor("#ccdcd0"); visible: root.shell.notice.length > 0
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
