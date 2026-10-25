import QtQuick
import QtQuick.Controls
import QtQuick.Effects

Item {
    id: root
    property string icon
    property string glyph: ""
    property string label
    property real iconRotation: 0
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
        color: root.highlighted ? Theme.yellow : mouse.containsMouse ? (root.tint.a > 0 ? Qt.lighter(root.tint, 1.12) : Theme.surfaceColor("#eee2f3")) : root.tint
        border.width: root.tint.a > 0 || root.highlighted ? 1 : 0
        border.color: Qt.darker(color, 1.12)
    }
    Image {
        visible: !root.glyph; anchors.centerIn: parent; width: 19; height: 19
        rotation: root.iconRotation; sourceSize: Qt.size(38,38)
        source: root.glyph ? "" : "qrc:/social-icons/" + root.icon + ".svg"
        layer.enabled: Theme.dark
        layer.effect: MultiEffect { brightness: 0.85 }
    }
    MenuGlyph { visible: !!root.glyph; anchors.centerIn: parent; width: 19; height: 19; glyph: root.glyph }
    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true
        onClicked: { root.tooltipSuppressed = true; root.clicked() }
        onExited: root.tooltipSuppressed = false
    }
    onHighlightedChanged: tooltipSuppressed = false
    ToolTip.visible: (mouse.containsMouse || highlighted) && !root.tooltipSuppressed && root.visible
    ToolTip.delay: 650
    ToolTip.text: label
}
