import QtQuick
import QtQuick.Shapes

Item {
    id: root
    property bool expanded: false
    property bool mirrored: false
    property real expandedHeight: 229
    property real expandedWidth: Theme.screenBounds.width + Theme.screenBounds.x
    readonly property real closedWidth: Theme.adventureTabWidth
    readonly property real leftJoin: Theme.screenBounds.x + Theme.screenBevel
    readonly property real lowerJoin: Theme.adventureTabHeight - (Theme.footerTop - Theme.screenBounds.y - Theme.screenBounds.height + Theme.screenBevel)
    property string button: "Y"
    property string label: "Recent games"
    property string legend: ""
    property string toggleName: "continue-toggle"
    property alias contentItem: tray
    signal toggled()
    function shoulder() {
        return "M " + leftJoin + " " + lowerJoin + " L " + (leftJoin + 6) + " 6"
            + " Q " + (leftJoin + 7) + " 0 " + (leftJoin + 11) + " 0"
            + " H " + (width - 45) + " Q " + (width - 42) + " 0 " + (width - 38) + " 3"
            + " L " + width + " " + lowerJoin;
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
    Item {
        anchors.fill: parent
        transform: Scale { origin.x: root.width/2; xScale: root.mirrored ? -1 : 1 }
    // Fade the existing bevel into the contact shadow at each mounting point.
    // These strips stay on the bevel itself, before the drawer's face is drawn.
    Rectangle {
        x: root.leftJoin; y: root.lowerJoin - 24; width: Theme.screenBevel; height: 24
        opacity: 1 - Math.max(0, Math.min(1, (root.height - 53) / 18))
        gradient: Gradient {
            GradientStop { position: 0; color: "#00101e1d" }
            GradientStop { position: 1; color: "#98101e1d" }
        }
    }
    Rectangle {
        x: root.width; y: root.lowerJoin
        width: Math.min(20, Math.max(0, root.expandedWidth - root.width)); height: Theme.screenBevel
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: "#88101e1d" }
            GradientStop { position: 1; color: "#00101e1d" }
        }
    }
    Repeater {
        model: 2
        delegate: Item {
            id: sideContact
            required property int index
            x: index === 0 ? root.leftJoin : root.expandedWidth; y: root.lowerJoin - 18
            width: Theme.screenBevel
            height: Math.max(18, root.height - (53 - root.lowerJoin) - y)
            opacity: Math.max(0, Math.min(1, (root.height - 53) / 18))
            Repeater {
                model: 3
                delegate: Rectangle {
                    required property int index
                    x: index * sideContact.width / 3; width: sideContact.width / 3; height: sideContact.height
                    opacity: sideContact.index === 0 ? [0.2, 0.55, 1][index] : [1, 0.55, 0.2][index]
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#00101e1d" }
                        GradientStop { position: Math.min(1, 18 / sideContact.height); color: "#88101e1d" }
                        GradientStop { position: 1; color: "#88101e1d" }
                    }
                }
            }
        }
    }
    Shape {
        anchors.fill: parent
        // A dark contact fades through wider, lighter contour bands.
        // The filled body covers the inner halves; there is no lower seam.
        ShapePath {
            strokeColor: "#08101e1d"; strokeWidth: 22; fillColor: "transparent"
            capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.shoulder() }
        }
        ShapePath {
            strokeColor: "#0c101e1d"; strokeWidth: 18; fillColor: "transparent"
            capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.shoulder() }
        }
        ShapePath {
            strokeColor: "#14101e1d"; strokeWidth: 14; fillColor: "transparent"
            capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.shoulder() }
        }
        ShapePath {
            strokeColor: "#24101e1d"; strokeWidth: 10; fillColor: "transparent"
            capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.shoulder() }
        }
        ShapePath {
            strokeColor: "#44101e1d"; strokeWidth: 6; fillColor: "transparent"
            capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.shoulder() }
        }
        ShapePath {
            strokeColor: "#70101e1d"; strokeWidth: 3; fillColor: "transparent"
            capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.shoulder() }
        }
        ShapePath {
            // Fill joins both the sidewall and footer; neither gets a seam.
            strokeColor: "transparent"
            fillGradient: LinearGradient {
                x1: 0; y1: -root.y; x2: 0; y2: Theme.viewportHeight - root.y
                GradientStop { position: 0; color: Theme.chassisCrown }
                GradientStop { position: 0.035; color: Theme.chassisTop }
                GradientStop { position: 0.24; color: Theme.chassisTop }
                GradientStop { position: 0.75; color: Theme.chassis }
                GradientStop { position: 0.94; color: Theme.chassis }
                GradientStop { position: 1; color: Theme.chassisFoot }
            }
            PathSvg {
                path: root.shoulder() + " V " + root.height + " H " + root.leftJoin + " Z"
            }
        }
    }
        Shape {
            anchors.fill: parent
            ShapePath {
                strokeColor: "#966c28"; strokeWidth: 1
                fillGradient: LinearGradient {
                    x1: 0; y1: 8; x2: 0; y2: 46
                    GradientStop { position: 0; color: Qt.lighter(Theme.yellow, 1.2) }
                    GradientStop { position: 1; color: Theme.yellow }
                }
                startX: root.leftJoin + 20; startY: 8
                PathLine { x: root.width - 44; y: 8 }
                PathQuad { x: root.width - 37; y: 11; controlX: root.width - 40; controlY: 8 }
                PathLine { x: root.width - 14; y: 34 }
                PathQuad { x: root.width - 11; y: 41; controlX: root.width - 11; controlY: 37 }
                PathQuad { x: root.width - 16; y: 46; controlX: root.width - 11; controlY: 46 }
                PathLine { x: root.leftJoin + 13; y: 46 }
                PathQuad { x: root.leftJoin + 7; y: 40; controlX: root.leftJoin + 7; controlY: 46 }
                PathLine { x: root.leftJoin + 13; y: 14 }
                PathQuad { x: root.leftJoin + 20; y: 8; controlX: root.leftJoin + 14; controlY: 8 }
            }
        }

    }
    Item {
        id: toggle
        objectName: root.toggleName
        width: root.width; height: Theme.adventureTabHeight
        Accessible.role: Accessible.Button; Accessible.name: root.button+" "+root.label
        Accessible.onPressAction: root.toggled()
        Text { x: root.mirrored ? parent.width-60 : 44; y: 12; text: root.button; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 21; font.weight: Font.DemiBold }
        Text { x: root.mirrored ? parent.width-246 : 66; y: 15; width: 180; horizontalAlignment: root.mirrored ? Text.AlignRight : Text.AlignLeft; text: root.label; color: Theme.ink; font.pixelSize: 17; font.weight: Font.DemiBold }
        Item {
            x: root.mirrored ? 44 : 306; y: 18; width: Math.max(0,toggle.width-350); height: 20; clip: true
            Text { width: Math.max(0,root.expandedWidth-350); horizontalAlignment: root.mirrored ? Text.AlignLeft : Text.AlignRight; text: root.legend; color: Theme.ink; font.pixelSize: 12 }
        }
        MouseArea { anchors.fill: parent; onClicked: root.toggled() }
    }
    Item {
        id: tray
        x: 14; y: 58; width: root.width-28; height: Math.max(0,root.height-64); clip: true
        visible: root.expanded || root.height>60
        enabled: root.expanded
    }
}
