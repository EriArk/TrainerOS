import QtQuick
import QtQuick.Shapes

// The fixed upper body overlaps the keys; their geometry stays underneath it.
Item {
    id: root
    objectName: "upper-chassis-rim"
    // Overlap the first key's outer shadow where this lip merges into the title.
    x: Theme.brandWidth - 3; y: 0
    width: Theme.viewportWidth - x; height: Theme.topRimHeight
    Rectangle {
        x: Theme.brandWidth - root.x; y: root.height
        width: Theme.screenBounds.x + Theme.screenBounds.width - Theme.screenBevel - Theme.brandWidth
        height: 8
        gradient: Gradient {
            GradientStop { position: 0; color: "#b8101e1d" }
            GradientStop { position: 0.3; color: "#70101e1d" }
            GradientStop { position: 0.65; color: "#28101e1d" }
            GradientStop { position: 1; color: "#00101e1d" }
        }
    }
    Rectangle { width: root.width; height: 1; color: Theme.edgeShadow }
    Shape {
        anchors.fill: parent
        ShapePath {
            strokeColor: "transparent"
            // Match the chassis' global crown gradient at the title/side joins.
            fillGradient: LinearGradient {
                x1: 0; y1: 0; x2: 0; y2: Theme.viewportHeight * 0.035
                GradientStop { position: 0; color: Theme.chassisCrown }
                GradientStop { position: 1; color: Theme.chassisTop }
            }
            PathSvg { path: "M 0 1 H " + (root.width - 1) + " V " + root.height + " H 0 Z" }
        }
        ShapePath {
            strokeColor: "transparent"
            fillGradient: LinearGradient {
                x1: -root.x; y1: 0; x2: Theme.viewportWidth - root.x; y2: Theme.viewportHeight
                GradientStop { position: 0; color: Theme.edgeLight }
                GradientStop { position: 0.35; color: Theme.chassisTop }
                GradientStop { position: 1; color: Theme.edgeShadow }
            }
            PathSvg {
                path: "M 0 1 H " + (root.width - 1) + " V " + root.height
                    + " H " + (root.width - 4) + " V 4 H 0 Z"
            }
        }
        ShapePath {
            strokeColor: Theme.edgeShadow; strokeWidth: 1; fillColor: "transparent"
            PathSvg {
                path: "M " + (Theme.brandWidth - root.x) + " " + root.height + " H "
                    + (Theme.screenBounds.x + Theme.screenBounds.width - Theme.screenBevel - root.x)
            }
        }
    }
}
