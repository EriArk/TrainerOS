import QtQuick
import QtQuick.Shapes

Rectangle {
    id: root
    property color surface: Theme.paper
    property bool patterned: true
    radius: 15
    color: Theme.chassisDark
    border.color: Theme.rim
    border.width: 1
    Rectangle {
        anchors.fill: parent
        anchors.margins: 9
        radius: 6
        color: root.surface
        border.color: "#aabfb3"
        border.width: 1
        ShellBackgroundPattern { anchors.fill: parent; anchors.margins: 2; visible: root.patterned }
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 1 }
            height: 2; color: "#b3ffffff"
        }
        Rectangle {
            anchors { top: parent.top; left: parent.left; right: parent.right; margins: 1 }
            height: 6
            radius: 5
            gradient: Gradient {
                GradientStop { position: 0; color: "#243f3d38" }
                GradientStop { position: 1; color: "#003f3d38" }
            }
        }
    }
    function contour(inset) {
        const l = inset, t = inset, r = width - inset, b = height - inset;
        const c = Math.max(1, radius - inset);
        return "M " + (l + c) + " " + t + " H " + (r - c)
            + " Q " + r + " " + t + " " + r + " " + (t + c)
            + " V " + (b - c) + " Q " + r + " " + b + " " + (r - c) + " " + b
            + " H " + (l + c) + " Q " + l + " " + b + " " + l + " " + (b - c)
            + " V " + (t + c) + " Q " + l + " " + t + " " + (l + c) + " " + t + " Z";
    }
    // Only the rim is foreground geometry. Its open center leaves content and
    // input untouched, while mounted fills cannot square off the inner corners.
    Shape {
        anchors.fill: parent; z: 10
        ShapePath {
            strokeColor: "transparent"; fillColor: Theme.chassisDark
            fillRule: ShapePath.OddEvenFill
            PathSvg { path: root.contour(0) + root.contour(9) }
        }
        ShapePath {
            strokeColor: "transparent"; fillRule: ShapePath.OddEvenFill
            fillGradient: LinearGradient {
                x1: 0; y1: 0; x2: 0; y2: root.height
                GradientStop { position: 0; color: Theme.edgeShadow }
                GradientStop { position: 0.5; color: Theme.chassis }
                GradientStop { position: 1; color: Theme.chassisTop }
            }
            PathSvg { path: root.contour(2) + root.contour(9) }
        }
        ShapePath {
            strokeColor: Theme.rim; strokeWidth: 1; fillColor: "transparent"
            PathSvg { path: root.contour(0.5) }
        }
        ShapePath {
            strokeColor: "#aabfb3"; strokeWidth: 1; fillColor: "transparent"
            PathSvg { path: root.contour(9.5) }
        }
    }
}
