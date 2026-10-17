import QtQuick
import QtQuick.Shapes
import QtQuick.Controls

Item {
    id: root
    required property var flow
    property bool takesFocus: false
    property bool recessed: false
    readonly property string outline: "M 0 36 Q 6 36 12 24 L 21 9 Q 26 1 36 1 H " + (width-8) + " Q " + width + " 1 " + width + " 9 V 36 Z"
    function point(px) {const p=buttons.mapFromItem(root,px,12);const i=Math.floor(p.x/(31+buttons.spacing));if(i>=0&&i<8)flow.focus("controls",i)}
    height: Theme.adventureTabHeight
    clip: true
    readonly property real drop: recessed || !flow.view.controls ? height : 0
    Item {
        y: root.drop; width: root.width; height: root.height
        Behavior on y { NumberAnimation { duration: Theme.motion(150); easing.type: Easing.InOutCubic } }
        readonly property string outline: "M 0 36 Q 6 36 12 24 L 21 9 Q 26 1 36 1 H " + (width-8) + " Q " + width + " 1 " + width + " 9 V 36 Z"
        Shape { anchors.fill: parent
            ShapePath { strokeColor: "transparent"; fillGradient: LinearGradient { x1: 0; y1: 0; x2: 0; y2: 36
                    GradientStop { position: 0; color: Qt.lighter(Theme.chassisTop,1.22) }
                    GradientStop { position: 1; color: Theme.chassis }
                }
                PathSvg { path: root.outline }
            }
            ShapePath { strokeColor: Theme.rim; strokeWidth: 1; fillColor: "transparent"
                PathSvg { path: "M 21 9 Q 26 1 36 1 H " + (root.width-8) + " Q " + root.width + " 1 " + root.width + " 9" }
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
                    Image { anchors.centerIn: parent; width: 19; height: 19; sourceSize: Qt.size(38,38); source: "qrc:/files-controls/"+modelData.icon+".svg" }
                    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; onClicked: {root.flow.focus("controls",index);root.flow.command(modelData.id)} }
                    ToolTip.visible: mouse.containsMouse; ToolTip.delay: 600; ToolTip.text: modelData.label
                }
            }
        }
    }
}
