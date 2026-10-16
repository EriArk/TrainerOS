import QtQuick
import QtQuick.Shapes

// Native vector version of the quiet sky/sun/path placeholder approved by the
// owner. All five themes share the drawing; their curated colours change live.
Rectangle {
    id: root
    readonly property string themeId: Theme.themeId
    radius: 15
    color: Theme.homeLandscape[0]
    opacity: .25
    Rectangle {
        x: parent.width*.82-width/2; y: parent.height*.2-height/2
        width: Math.min(parent.width*.12,parent.height*.15); height: width
        radius: width/2; color: Theme.homeLandscape[3]
    }
    Shape {
        anchors.fill: parent
        ShapePath {
            strokeWidth: 0; strokeColor: "transparent"; fillColor: Theme.homeLandscape[1]
            PathSvg { path: "M 0 " + root.height*.58 + " H " + root.width
                + " V " + (root.height-root.radius) + " Q " + root.width + " " + root.height + " " + (root.width-root.radius) + " " + root.height
                + " H " + root.radius + " Q 0 " + root.height + " 0 " + (root.height-root.radius) + " Z" }
        }
    }
    Rectangle {
        x: parent.width*.32; y: parent.height*.61
        width: parent.width*.28; height: parent.height-y
        color: Theme.homeLandscape[2]
    }
}
