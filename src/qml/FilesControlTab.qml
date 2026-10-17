import QtQuick
import QtQuick.Shapes
import QtQuick.Controls

Item {
    id: root
    required property var flow
    property bool takesFocus: false
    property bool recessed: false
    function point(px) {const p=buttons.mapFromItem(root,px,12);const i=Math.floor(p.x/(31+buttons.spacing));if(i>=0&&i<8)flow.focus("controls",i)}
    height: Theme.adventureTabHeight
    readonly property real drop: recessed || !flow.view.controls ? height : 0
    Item {
        x: -8; y: -8; width: root.width+16; height: root.height+8
        clip: true
        layer.enabled: true
        layer.textureSize: Qt.size(Math.ceil(width*Theme.uiScale),Math.ceil(height*Theme.uiScale))
        layer.smooth: true
        Item {
        x: 8; y: 8+root.drop; width: root.width; height: root.height
        Behavior on y { NumberAnimation { duration: Theme.motion(150); easing.type: Easing.InOutCubic } }
        // Same soft contact shadow as the neighbouring closed X/Y handles.
        Shape {
            x: panel.x; y: 2; width: panel.width; height: panel.height
            ShapePath {
                strokeColor: "#0a101e1d"; strokeWidth: 10; fillColor: "transparent"; joinStyle: ShapePath.RoundJoin
                PathSvg { path: panel.shoulder }
            }
            ShapePath {
                strokeColor: "#14101e1d"; strokeWidth: 6; fillColor: "transparent"; joinStyle: ShapePath.RoundJoin
                PathSvg { path: panel.shoulder }
            }
            ShapePath {
                strokeColor: "#24101e1d"; strokeWidth: 3; fillColor: "transparent"; joinStyle: ShapePath.RoundJoin
                PathSvg { path: panel.shoulder }
            }
        }
        // Keep the row's existing screen coordinates; fit only the chassis skin.
        Shape {
            id: panel
            x: buttons.x - 28; width: buttons.width + 56; height: parent.height
            readonly property string shoulder: "M 0 36 Q 6 36 12 24 L 21 9 Q 26 1 36 1 H " + (width-36)
                + " Q " + (width-26) + " 1 " + (width-21) + " 9 L " + (width-12) + " 24 Q " + (width-6) + " 36 " + width + " 36"
            ShapePath { strokeColor: "transparent"; fillGradient: LinearGradient { x1: 0; y1: 0; x2: 0; y2: 36
                    GradientStop { position: 0; color: Qt.lighter(Theme.chassisTop,1.22) }
                    GradientStop { position: 1; color: Theme.chassis }
                }
                PathSvg { path: panel.shoulder + " Z" }
            }
            ShapePath { strokeColor: Theme.rim; strokeWidth: 1; fillColor: "transparent"
                // Light catches only the crown, never the feet overlapping the chassis.
                PathSvg { path: "M 21 9 Q 26 1 36 1 H " + (panel.width-36)
                    + " Q " + (panel.width-26) + " 1 " + (panel.width-21) + " 9" }
            }
        }
        Row {
            id: buttons
            anchors.centerIn: parent; anchors.horizontalCenterOffset: 9; spacing: Math.min(18,Math.max(4,(root.width-320)/9))
            Repeater { model: root.flow.controls
                Item { required property var modelData; required property int index; width: 31; height: 30
                    Accessible.role: Accessible.Button; Accessible.name: modelData.label
                    Accessible.onPressAction: root.flow.command(modelData.id)
                    Rectangle { anchors.fill: parent; anchors.margins: 1; radius: 7; color: root.takesFocus && root.flow.view.area === "controls" && root.flow.view.controlIndex === index ? "#4476b3a4" : mouse.containsMouse ? "#2276b3a4" : "transparent"; border.width: root.takesFocus && root.flow.view.area === "controls" && root.flow.view.controlIndex === index ? 1 : 0; border.color: Theme.yellow }
                    Image { anchors.centerIn: parent; width: 19; height: 19; sourceSize: Qt.size(38,38); source: modelData.source || "qrc:/files-controls/"+modelData.icon+".svg" }
                    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; onClicked: {root.flow.focus("controls",index);root.flow.command(modelData.id)} }
                    ToolTip.visible: mouse.containsMouse; ToolTip.delay: 600; ToolTip.text: modelData.label
                }
            }
        }
        }
    }
}
