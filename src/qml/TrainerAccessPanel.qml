import QtQuick

Item {
    id: root
    required property var access
    ChassisFrame { anchors.fill: parent }
    Text { x: 38; y: 30; text: "TRAINER OS"; color: "#f6e4b3"; font.pixelSize: 26; font.bold: true }
    MountedPanel {
        x: 30; y: 83; width: parent.width-60; height: parent.height-136; color: "#e5eadc"
        Text { x: 28; y: 22; width: parent.width-56; text: root.access.title; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 27; font.bold: true; elide: Text.ElideRight }
        Text { x: 28; y: 65; width: parent.width-56; text: root.access.description; color: Theme.muted; font.pixelSize: 16; wrapMode: Text.WordWrap }
        Item {
            x: 28; y: 115; width: parent.width-56; height: 228; visible: root.access.keypad
            Rectangle {
                x: 8; y: 19; width: 370; height: 107; radius: 18; color: "#cddccc"; border.width: 2; border.color: "#9aad9c"
                Text { anchors.centerIn: parent; text: root.access.mask || (root.access.minimumDigits===6 ? "○ ○ ○ ○ ○ ○" : "○ ○ ○ ○"); color: Theme.ink; font.pixelSize: root.access.minimumDigits===6 ? 34 : 41; font.letterSpacing: 6 }
            }
            Text { x: 25; y: 153; width: 340; text: "D-pad  Choose a key\nA  Enter digit    B  Back"; color: Theme.muted; font.pixelSize: 18; lineHeight: 1.5 }
            Grid { x: 458; y: 0; columns: 3; columnSpacing: 12; rowSpacing: 10
                Repeater { model: ["1","2","3","4","5","6","7","8","9","⌫","0","Clear"]
                    CapButton {
                        required property int index; required property string modelData
                        objectName: "access-key-"+index
                        width: 105; height: 36; label: modelData; centered: true
                        tint: index===9 || index===11 ? Theme.pink : Theme.blue
                        selected: root.visible && root.access.keypad && root.access.focusIndex===index
                        enabled: !root.access.busy
                        onActivated: root.access.activate(index)
                    }
                }
            }
            CapButton { x: 458; y: 190; width: 339; height: 38; label: "Continue"; centered: true
                objectName: "access-key-12"; selected: root.visible && root.access.keypad && root.access.focusIndex===12
                enabled: !root.access.busy; onActivated: root.access.activate(12)
            }
        }
        Column { x: 36; y: 125; spacing: 14; visible: !root.access.keypad
            Repeater { model: root.access.choices
                CapButton {
                    required property int index; required property string modelData
                    objectName: "access-choice-"+index; width: 550; height: 48; label: modelData
                    selected: root.visible && !root.access.keypad && root.access.focusIndex===index
                    enabled: !root.access.busy; onActivated: root.access.activate(index)
                }
            }
        }
        Text { x: 28; y: parent.height-29; width: parent.width-56; text: root.access.error; color: "#873b25"; font.pixelSize: 15; elide: Text.ElideRight }
    }
    Text { x: 40; y: parent.height-37; text: root.access.canRecover ? "X  Forgot PIN?     B  Choose another Trainer" : "B  Back"; color: "#f6e4b3"; font.pixelSize: 17 }
    TapHandler { enabled: root.access.canRecover; onTapped: point => { if(point.position.y>root.height-53)root.access.recover() } }
}
