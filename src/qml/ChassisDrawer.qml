import QtQuick
import QtQuick.Shapes

Item {
    id: root
    property bool expanded: false
    property real expandedHeight: 229
    property real expandedWidth: Theme.screenBounds.width
    property real contentTop: 58
    property bool showExpandedHeader: true
    readonly property bool headerVisible: showExpandedHeader || height <= Theme.adventureTabHeight + 0.1
    readonly property real closedWidth: Theme.adventureTabWidth
    property string button: "Y"
    property string label: "Recent games"
    readonly property real keyInset: button === "Y" ? 30 : 18
    property string legend: ""
    property string toggleName: "continue-toggle"
    property alias contentItem: tray
    signal toggled()
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
    width: closedWidth; height: Theme.adventureTabHeight
    onExpandedWidthChanged: if(expanded && !opening.running && !closing.running)width=expandedWidth
    onExpandedChanged: {
        opening.stop(); closing.stop();
        if (expanded) opening.start(); else closing.start();
    }
    SequentialAnimation {
        id: opening
        NumberAnimation { target: root; property: "width"; to: root.expandedWidth; duration: Theme.motion(180); easing.type: Easing.InOutCubic }
        NumberAnimation { target: root; property: "height"; to: root.expandedHeight; duration: Theme.motion(220); easing.type: Easing.OutCubic }
    }
    SequentialAnimation {
        id: closing
        NumberAnimation { target: root; property: "height"; to: Theme.adventureTabHeight; duration: Theme.motion(170); easing.type: Easing.InOutCubic }
        NumberAnimation { target: root; property: "width"; to: root.closedWidth; duration: Theme.motion(160); easing.type: Easing.OutCubic }
    }
    // Mount at the existing lower screen lip. The curved feet join the bezel;
    // no separate rail or strip is painted over the chassis.
    Shape {
        anchors.fill: parent
        // A soft contact shadow, not another bevel wrapped around the tab.
        ShapePath {
            strokeColor: "#18101e1d"; strokeWidth: 4
            fillColor: "transparent"; joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.shoulder() }
        }
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
            PathSvg { path: root.shoulder() }
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
            x: root.keyInset+28; y: 9; width: 101; height: 22
            text: root.label; color: root.button === "Y" ? Theme.ink : Theme.paper
            font.pixelSize: 13; font.weight: Font.Medium; verticalAlignment: Text.AlignVCenter
        }
        Item {
            x: 180; y: 11; width: Math.max(0,toggle.width-210); height: 20; clip: true
            Text { width: Math.max(0,root.expandedWidth-210); horizontalAlignment: Text.AlignRight; text: root.legend; color: root.button === "Y" ? Theme.ink : Theme.paper; font.pixelSize: 11 }
        }
        MouseArea { anchors.fill: parent; onClicked: root.toggled() }
    }
    Item {
        id: tray
        x: 14; y: root.contentTop; width: root.width-28; height: Math.max(0,root.height-root.contentTop-6); clip: true
        visible: root.expanded || root.height>root.contentTop+2
        enabled: root.expanded
    }
}
