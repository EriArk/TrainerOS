import QtQuick

Item {
    id: root
    required property var flow
    Rectangle {
        anchors.fill: parent; color: "#70162723"; visible: root.flow.menu.length > 0
        MouseArea { anchors.fill: parent; onClicked: root.flow.menuAction(3) }
        Rectangle {
            anchors.centerIn: parent; width: 310; height: root.flow.menu.length * 49 + 24
            radius: 18; color: Theme.paper; border.color: Theme.ink; border.width: 2
            Column {
                anchors.fill: parent; anchors.margins: 12; spacing: 5
                Repeater {
                    model: root.flow.menu
                    Rectangle {
                        required property var modelData; required property int index
                        width: 286; height: 44; radius: 10; color: root.flow.menuIndex === index ? Theme.yellow : "#e1e8df"
                        Text { anchors.centerIn: parent; text: modelData.label; font.pixelSize: 16; color: Theme.ink }
                        MouseArea { anchors.fill: parent; onClicked: root.flow.menuAction(index) }
                    }
                }
            }
        }
    }
}
