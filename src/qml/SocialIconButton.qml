import QtQuick
import QtQuick.Controls

Item {
    id: root
    property string icon
    property string label
    property bool highlighted: false
    property color tint: "transparent"
    property bool tooltipSuppressed: false
    signal clicked()
    width: 32; height: 32
    opacity: enabled ? 1 : 0.35
    Accessible.role: Accessible.Button
    Accessible.name: label
    Accessible.onPressAction: if (enabled) clicked()
    Rectangle {
        anchors.fill: parent; anchors.margins: 2; radius: 7
        color: root.highlighted ? Theme.yellow : mouse.containsMouse ? (root.tint.a > 0 ? Qt.lighter(root.tint, 1.12) : "#eee2f3") : root.tint
        border.width: root.tint.a > 0 || root.highlighted ? 1 : 0
        border.color: Qt.darker(color, 1.12)
    }
    Image { anchors.centerIn: parent; width: 19; height: 19; sourceSize: Qt.size(38,38); source: "qrc:/social-icons/" + root.icon + ".svg" }
    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true
        onClicked: { root.tooltipSuppressed = true; root.clicked() }
        onExited: root.tooltipSuppressed = false
    }
    ToolTip.visible: mouse.containsMouse && !root.tooltipSuppressed
    ToolTip.delay: 650
    ToolTip.text: label
}
