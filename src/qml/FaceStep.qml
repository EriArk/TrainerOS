import QtQuick

Item {
    id: root
    property bool forward: false
    signal activated()
    width: 46; height: 26
    opacity: enabled ? 1 : .4
    Accessible.role: Accessible.Button
    Accessible.name: forward ? "R2, next" : "L2, previous"
    Accessible.onPressAction: if(enabled) activated()
    Text {
        x: root.forward ? 31 : 0; width: 13; height: parent.height
        text: root.forward ? "›" : "‹"; color: pointer.containsMouse ? Theme.ink : Theme.muted
        font.family: Theme.displayFamily; font.pixelSize: 23
        verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignHCenter
    }
    Text {
        x: root.forward ? 0 : 15; width: 28; height: parent.height
        text: root.forward ? "R2" : "L2"; color: pointer.containsMouse ? Theme.ink : Theme.muted
        font.family: Theme.displayFamily; font.pixelSize: 13; font.weight: Font.DemiBold
        verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignHCenter
    }
    MouseArea { id: pointer; anchors.fill: parent; anchors.margins: -3; hoverEnabled: true; onClicked: root.activated() }
}
