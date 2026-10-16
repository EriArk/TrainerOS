import QtQuick
import QtQuick.Shapes

Item {
    id: root
    property bool expanded: false
    property real expandedHeight: 229
    property real expandedWidth: Theme.screenBounds.width
    property real contentTop: 12
    property bool handlesRecessed: expanded
    property bool otherPanelVisible: false
    property real panelHeight: 0
    readonly property bool panelVisible: panelHeight > 0
    property real handleDrop: handlesRecessed ? Theme.adventureTabHeight+8 : 0
    readonly property bool headerVisible: !panelVisible && handleDrop < Theme.adventureTabHeight+7.9
    Behavior on handleDrop {
        NumberAnimation { duration: Theme.motion(90); easing.type: Easing.InOutCubic }
    }
    readonly property real closedWidth: Theme.adventureTabWidth
    property string button: "Y"
    property string label: "Recent games"
    readonly property real keyInset: button === "Y" ? 30 : 22
    property string toggleName: "continue-toggle"
    property alias contentItem: tray
    signal toggled()
    // Same nested recess widths as ChassisFrame. A screen cutout, not a raised lip.
    function screenCutout(inset) {
        const left = inset, right = width-inset;
        const bottom = Theme.screenBevel-inset;
        return "M " + left + " -16 H " + right + " V " + bottom
            + " H " + left + " Z";
    }
    function shoulder() {
        // The right Y sits behind the left X. Only the inner joining foot
        // flares out; the exposed outer end rounds directly into the screen lip.
        if (button === "Y")
            return "M 0 34 Q 8 34 14 23 L 23 9 Q 28 1 37 1 H " + (width-32)
                + " Q " + (width-23) + " 1 " + (width-18) + " 9 L " + (width-2)
                + " 28 Q " + (width+1) + " 34 " + (width-5) + " 34";
        return "M 5 34 Q -1 34 2 28 L 18 9 Q 23 1 32 1 H " + (width-33)
            + " Q " + (width-23) + " 1 " + (width-18) + " 10 L " + (width-10)
            + " 23 Q " + (width-5) + " 34 " + width + " 34";
    }
    function topHighlight() {
        if (button === "Y") return shoulder();
        // Light catches the crown; it does not trace the exposed left foot.
        return "M 18 9 Q 23 1 32 1 H " + (width-33)
            + " Q " + (width-23) + " 1 " + (width-18) + " 10";
    }
    width: panelVisible ? expandedWidth : closedWidth
    height: Math.max(Theme.adventureTabHeight,panelHeight)
    onExpandedHeightChanged: if(expanded && !opening.running)panelHeight=expandedHeight
    onExpandedChanged: {
        opening.stop(); closing.stop();
        if (expanded) opening.start(); else closing.start();
    }
    SequentialAnimation {
        id: opening
        PauseAnimation { duration: Theme.motion(root.otherPanelVisible ? 180 : root.panelVisible ? 0 : 100) }
        NumberAnimation { target: root; property: "panelHeight"; to: root.expandedHeight; duration: Theme.motion(220); easing.type: Easing.OutCubic }
    }
    NumberAnimation {
        id: closing; target: root; property: "panelHeight"; to: 0
        duration: Theme.motion(170); easing.type: Easing.InOutCubic
    }
    // Clip the sinking handles at the footer lip; never paint over its controls.
    Item {
        x: -8; y: root.height-Theme.adventureTabHeight-8
        width: root.closedWidth+16; height: Theme.adventureTabHeight+8
        clip: true; visible: root.headerVisible; enabled: !root.handlesRecessed
        layer.enabled: true
        layer.textureSize: Qt.size(Math.ceil(width*Theme.uiScale),Math.ceil(height*Theme.uiScale))
        layer.smooth: true
        Item {
            x: 8; y: 8+root.handleDrop; width: root.closedWidth; height: Theme.adventureTabHeight
            // Mount at the existing lower screen lip. The curved feet join the bezel;
            // no separate rail or strip is painted over the chassis.
            Shape {
                x: 0; y: 2; width: root.width; height: root.height
                visible: root.height <= Theme.adventureTabHeight+0.1
                ShapePath {
                    strokeColor: "#0a101e1d"; strokeWidth: 10
                    fillColor: "transparent"; joinStyle: ShapePath.RoundJoin
                    PathSvg { path: root.shoulder() }
                }
                ShapePath {
                    strokeColor: "#14101e1d"; strokeWidth: 6
                    fillColor: "transparent"; joinStyle: ShapePath.RoundJoin
                    PathSvg { path: root.shoulder() }
                }
                ShapePath {
                    strokeColor: "#24101e1d"; strokeWidth: 3
                    fillColor: "transparent"; joinStyle: ShapePath.RoundJoin
                    PathSvg { path: root.shoulder() }
                }
            }
            Shape {
                anchors.fill: parent
                visible: root.headerVisible
                ShapePath {
                    strokeColor: "transparent"
                    fillGradient: LinearGradient {
                        x1: 0; y1: 0; x2: 0; y2: root.height
                        GradientStop { position: 0; color: Theme.chassisCrown }
                        GradientStop { position: 1; color: Theme.chassis }
                    }
                    PathSvg { path: root.shoulder() + " V " + root.height + " H 0 Z" }
                }
                ShapePath {
                    strokeColor: "transparent"
                    fillGradient: LinearGradient {
                        x1: 0; y1: 1; x2: 0; y2: 34
                        GradientStop { position: 0; color: !root.headerVisible ? "transparent" : root.button === "Y" ? Qt.lighter(Theme.yellow,1.15) : Qt.lighter(Theme.chassisTop,1.22) }
                        GradientStop { position: 1; color: !root.headerVisible ? "transparent" : root.button === "Y" ? Theme.yellow : Theme.chassis }
                    }
                    PathSvg { path: root.shoulder() + " L " + root.width + " 34 H 0 Z" }
                }
                ShapePath {
                    strokeColor: root.button === "Y" ? Qt.lighter(Theme.yellow,1.35) : Theme.rim
                    strokeWidth: 1; fillColor: "transparent"; joinStyle: ShapePath.RoundJoin
                    PathSvg { path: root.topHighlight() }
                }
            }
            Item {
                id: toggle
                objectName: root.toggleName
                visible: root.headerVisible
                width: root.width; height: Theme.adventureTabHeight
                Accessible.role: Accessible.Button; Accessible.name: root.button+" "+root.label
                Accessible.onPressAction: root.toggled()
                Repeater {
                    model: 4
                    delegate: Rectangle {
                        required property int index
                        x: root.keyInset-4+index; y: 7+index; width: 29-index*2; height: width; radius: width/2
                        color: ["#0a101e1d","#12101e1d","#1c101e1d","#28101e1d"][index]
                    }
                }
                Rectangle {
                    x: root.keyInset; y: 10; width: 21; height: 21; radius: width/2
                    color: Qt.darker(keyCap.tint,1.3)
                }
                Rectangle {
                    id: keyCap
                    objectName: "drawer-key-"+root.button
                    x: root.keyInset; y: 7; width: 21; height: 21; radius: width/2
                    readonly property color tint: root.button === "Y" ? Theme.yellow : Theme.pink
                    border.color: Qt.lighter(tint,1.35); border.width: 0.7
                    gradient: Gradient {
                        GradientStop { position: 0; color: Qt.lighter(keyCap.tint,1.28) }
                        GradientStop { position: 1; color: keyCap.tint }
                    }
                    Text { anchors.centerIn: parent; text: root.button; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 16; font.weight: Font.DemiBold }
                }
                Text {
                    x: (root.button === "Y" ? root.keyInset : 18)+28; y: 7; width: root.closedWidth-x-22; height: 24
                    text: root.label; color: root.button === "Y" ? Theme.ink : Theme.paper
                    font.pixelSize: 13; font.weight: Font.Medium
                    fontSizeMode: Text.HorizontalFit; minimumPixelSize: 10
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                MouseArea { anchors.fill: parent; onClicked: root.toggled() }
            }

        }
    }
    Item {
        x: -1; y: root.height-root.panelHeight-15; width: root.width+2; height: root.panelHeight+15
        visible: root.panelVisible
        clip: true
        Shape {
        x: 1; y: 15; width: root.width; height: root.panelHeight
        ShapePath {
            strokeColor: "transparent"; fillRule: ShapePath.OddEvenFill
            fillColor: Theme.chassis
            // Paint past the clipped side edges so antialiasing cannot expose
            // the previous screen perimeter below its raised bottom edge.
            PathSvg { path: "M -1 -16 H " + (root.width+1) + " V " + (root.panelHeight+1) + " H -1 Z" + root.screenCutout(0) }
        }
        ShapePath {
            strokeColor: "transparent"; fillRule: ShapePath.OddEvenFill
            fillColor: Theme.edgeShadow
            PathSvg { path: root.screenCutout(0) + root.screenCutout(2) }
        }
        ShapePath {
            strokeColor: "transparent"; fillRule: ShapePath.OddEvenFill
            fillColor: Theme.screenSlope
            PathSvg { path: root.screenCutout(2) + root.screenCutout(9) }
        }
        ShapePath {
            strokeColor: "transparent"; fillRule: ShapePath.OddEvenFill
            fillColor: Theme.screenContact
            PathSvg { path: root.screenCutout(9) + root.screenCutout(Theme.screenBevel) }
        }
        }
    }
    Item {
        id: tray
        objectName: root.toggleName+"-content"
        x: 14; y: root.height-root.panelHeight+root.contentTop; width: root.width-28; height: Math.max(0,root.panelHeight-root.contentTop-6); clip: true
        visible: root.panelVisible && root.panelHeight>root.contentTop+2
        enabled: root.expanded
    }
}
