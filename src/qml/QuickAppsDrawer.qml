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
        width: Math.min(root.width-48, count ? Math.max(174,count*82+40) : 340)
        height: root.expanded ? (count ? 138 : 96) : 0
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
            return "M 0 " + height + " Q 5 " + height + " 10 " + (height-14)
                + " L 18 13 Q 21 3 31 3 H " + (width-31)
                + " Q " + (width-21) + " 3 " + (width-18) + " 13 L " + (width-10)
                + " " + (height-14) + " Q " + (width-5) + " " + height + " " + width + " " + height;
        }
        Shape {
            anchors.fill: parent
            ShapePath {
                strokeColor: "#20101e1d"; strokeWidth: 5; fillColor: "transparent"
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
                strokeColor: Theme.rim; strokeWidth: .8; fillColor: "transparent"
                PathSvg { path: "M 18 13 Q 21 3 31 3 H " + (shelf.width-31) + " Q " + (shelf.width-21) + " 3 " + (shelf.width-18) + " 13" }
            }
        }
        MouseArea { anchors.fill: parent }
        AppsDock {
            anchors.horizontalCenter: parent.horizontalCenter
            width: shelf.count ? Math.min(parent.width-40,shelf.count*82) : parent.width-40
            y: 18; height: shelf.count ? 104 : 60
            flow: root.shell.apps
            enabled: root.expanded && !root.shell.apps.menu.length
        }
    }
}
