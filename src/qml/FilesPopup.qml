import QtQuick
Item {
    id: root
    required property var flow
    readonly property bool protectedDialog: Object.keys(flow.dialog).length > 0
    visible: protectedDialog || flow.view.menuOpen
    Rectangle { anchors.fill: parent; color: Theme.surfaceColor("#65162723"); MouseArea { anchors.fill: parent; onClicked: if(!root.protectedDialog)root.flow.leave() } }
    Rectangle {
        anchors.centerIn: parent; width: Math.min(370,parent.width-44); height: Math.min(parent.height-24, body.implicitHeight+28)
        radius: 16; color: Theme.paper; border.width: 2; border.color: Theme.rim
        Column { id: body; x: 14; y: 14; width: parent.width-28; spacing: 6
            Text { visible: root.protectedDialog; width: parent.width; text: root.flow.dialog.title || ""; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 20; wrapMode: Text.Wrap; textFormat: Text.PlainText }
            Text { visible: root.protectedDialog; width: parent.width; text: root.flow.dialog.text || ""; color: Theme.muted; font.pixelSize: 12; wrapMode: Text.WrapAnywhere; maximumLineCount: 4; elide: Text.ElideRight; textFormat: Text.PlainText }
            Repeater { model: root.protectedDialog ? root.flow.dialog.actions || [] : root.flow.menu
                Rectangle { required property var modelData; required property int index; width: body.width; height: 29; radius: 6
                    color: (root.protectedDialog ? root.flow.view.dialogIndex : root.flow.view.menuIndex) === index ? Theme.yellow : "transparent"
                    Text { x: 10; anchors.verticalCenter: parent.verticalCenter; text: modelData.label; font.pixelSize: 13; color: modelData.id === "delete" ? Theme.textColor("#984b51") : Theme.ink }
                    MouseArea { anchors.fill: parent; onClicked: root.protectedDialog ? root.flow.answer(index) : root.flow.command(modelData.id) }
                }
            }
        }
    }
}
