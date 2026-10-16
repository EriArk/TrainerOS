import QtQuick
import QtQuick.Shapes

Item {
    id: root
    required property var shell
    objectName: "quick-apps-drawer"
    readonly property bool expanded: shell.quickAppsOpen
    width: Theme.screenBounds.width; height: 178; clip: true
    // The folded handle stays on the left bezel while the dock rises at center.
    ChassisDrawer {
        id: handle
        objectName: "quick-apps-folded"
        visible: shelf.height<1
        enabled: !root.expanded
        anchors.left: parent.left; anchors.bottom: parent.bottom
        button: "X"; label: "Quick Apps"; toggleName: "quick-apps-handle"
        onToggled: root.shell.toggleQuickApps()
    }
    Item {
        id: shelf
        objectName: "quick-apps-shelf"
        readonly property int count: root.shell.apps.rail.length
        width: Math.min(root.width-48, count ? Math.max(174,count*62+76) : 300)
        height: root.expanded ? 68 : 0
        x: (root.width-width)/2
        y: root.height-height
        visible: height>0
        clip: true
        enabled: root.expanded
        Behavior on height { NumberAnimation { duration: Theme.motion(220); easing.type: Easing.OutCubic } }
        Behavior on width { NumberAnimation { duration: Theme.motion(160); easing.type: Easing.InOutCubic } }
        // The whole button panel grows in height from the fixed bezel base.
        // Keys stay inside its body; there is no separate shelf beneath them.
        function crown() {
            return "M 0 " + height + " C 15 " + height + " 19 " + (height-6) + " 26 " + (height-22)
                + " L 43 18 Q 48 7 60 7 H " + (width-60)
                + " Q " + (width-48) + " 7 " + (width-43) + " 18 L " + (width-26)
                + " " + (height-22) + " C " + (width-19) + " " + (height-6) + " " + (width-15) + " " + height + " " + width + " " + height;
        }
        Shape {
            anchors.fill: parent
            ShapePath {
                strokeColor: Theme.screenContact; strokeWidth: 4; fillColor: "transparent"
                joinStyle: ShapePath.RoundJoin
                PathSvg { path: shelf.crown() }
            }
            ShapePath {
                strokeColor: "transparent"
                fillGradient: LinearGradient {
                    x1: 0; y1: 0; x2: 0; y2: shelf.height
                    GradientStop { position: 0; color: Theme.chassisCrown }
                    GradientStop { position: .12; color: Theme.chassisTop }
                    GradientStop { position: 1; color: Theme.chassis }
                }
                PathSvg { path: shelf.crown() + " Z" }
            }
            ShapePath {
                strokeColor: Theme.rim; strokeWidth: 1; fillColor: "transparent"
                PathSvg { path: shelf.crown() }
            }
        }
        MouseArea { anchors.fill: parent }
        AppsDock {
            anchors.horizontalCenter: parent.horizontalCenter
            width: shelf.count ? Math.min(parent.width-76,shelf.count*62) : parent.width-76
            y: 10; height: 54
            flow: root.shell.apps
            enabled: root.expanded && !root.shell.apps.menu.length
        }
    }
}
