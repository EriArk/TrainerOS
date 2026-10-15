import QtQuick

Item {
    id: root
    property string title: ""
    property string message: ""
    PageHeader { id: heading; width: parent.width; title: root.title }
    MountedPanel {
        x: 0; y: heading.height+12; width: parent.width; height: parent.height-y
        color: "#e2e9dd"
        Text {
            anchors.centerIn: parent; width: Math.min(parent.width-48,520)
            text: root.message; color: Theme.muted; font.pixelSize: 19
            wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter
        }
    }
}
