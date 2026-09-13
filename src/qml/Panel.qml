import QtQuick
import QtQuick.Shapes

Rectangle {
    id: root
    property color surface: Theme.paper
    property bool patterned: true
    radius: 15
    color: Theme.chassisDark
    border.color: Theme.edgeShadow
    border.width: 1
    Rectangle {
        anchors.fill: parent
        anchors.margins: 9
        radius: 6
        color: root.surface
        border.color: Theme.screenContact
        border.width: 1
        ShellBackgroundPattern { anchors.fill: parent; anchors.margins: 2; visible: root.patterned }
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
            fillColor: Theme.screenSlope
            PathSvg { path: root.contour(2) + root.contour(9) }
        }
        ShapePath {
            strokeColor: Theme.edgeShadow; strokeWidth: 1; fillColor: "transparent"
            PathSvg { path: root.contour(0.5) }
        }
        ShapePath {
            strokeColor: Theme.screenContact; strokeWidth: 1; fillColor: "transparent"
            PathSvg { path: root.contour(9.5) }
        }
    }
}
