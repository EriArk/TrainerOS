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
        opacity: 1-shelf.opacity
        visible: opacity>0
        enabled: !root.expanded
        anchors.left: parent.left; anchors.bottom: parent.bottom
        button: "X"; label: "Quick Apps"; toggleName: "quick-apps-handle"
        onToggled: root.shell.toggleQuickApps()
    }
    Item {
        id: shelf
        objectName: "quick-apps-shelf"
        readonly property int count: root.shell.apps.rail.length
        width: Math.min(root.width-48, count ? Math.max(160,count*72+40) : 340)
        height: count ? 126 : 84
        x: (root.width-width)/2
        y: root.height-height + (root.expanded ? 0 : height+12)
        opacity: root.expanded ? 1 : 0
        visible: opacity>0
        enabled: root.expanded
        Behavior on y { NumberAnimation { duration: Theme.motion(210); easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: Theme.motion(150) } }
        Behavior on width { NumberAnimation { duration: Theme.motion(160); easing.type: Easing.InOutCubic } }
        // A raised part of the shell: the front blends directly into the bezel.
        // Only the top plane is lit; there is no glass slab or floating lower edge.
        function crown() {
            return "M 0 " + height + " Q 5 " + height + " 11 " + (height-15)
                + " L 20 " + (height-38) + " Q 23 " + (height-45) + " 31 " + (height-45)
                + " H " + (width-31) + " Q " + (width-23) + " " + (height-45) + " " + (width-20)
                + " " + (height-38) + " L " + (width-11) + " " + (height-15)
                + " Q " + (width-5) + " " + height + " " + width + " " + height;
        }
        Shape {
            anchors.fill: parent
            ShapePath {
                strokeColor: "#20101e1d"; strokeWidth: 6; fillColor: "transparent"
                joinStyle: ShapePath.RoundJoin
                PathSvg { path: shelf.crown() }
            }
            ShapePath {
                strokeColor: "transparent"
                fillGradient: LinearGradient {
                    x1: 0; y1: shelf.height-45; x2: 0; y2: shelf.height
                    GradientStop { position: 0; color: Theme.chassisCrown }
                    GradientStop { position: .65; color: Theme.screenSlope }
                    GradientStop { position: .72; color: Theme.chassisTop }
                    GradientStop { position: 1; color: Theme.chassis }
                }
                PathSvg { path: shelf.crown() + " Z" }
            }
            ShapePath {
                strokeColor: Theme.rim; strokeWidth: .8; fillColor: "transparent"
                PathSvg { path: "M 20 " + (shelf.height-38) + " Q 23 " + (shelf.height-45) + " 31 " + (shelf.height-45) + " H " + (shelf.width-31) + " Q " + (shelf.width-23) + " " + (shelf.height-45) + " " + (shelf.width-20) + " " + (shelf.height-38) }
            }
        }
        MouseArea { anchors.fill: parent }
        AppsDock {
            anchors.horizontalCenter: parent.horizontalCenter
            width: shelf.count ? Math.min(parent.width-40,shelf.count*72) : parent.width-40
            height: parent.height
            flow: root.shell.apps
            enabled: root.expanded && !root.shell.apps.menu.length
        }
    }
}
