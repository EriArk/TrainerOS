import QtQuick
import QtQuick.Shapes

Rectangle {
    id: root
    objectName: "chassis-frame"
    gradient: Gradient {
        GradientStop { position: 0; color: Theme.chassisTop }
        GradientStop { position: 1; color: Theme.chassis }
    }
    border.color: Theme.rim

    // One closed screen aperture, including the diagonal under the title.
    // Its inset contours form a bevel in the body, not four separate rails.
    function aperture(inset) {
        const s = Theme.screenBounds;
        const left = s.x + inset, right = s.x + s.width - inset;
        const top = s.y + inset, bottom = s.y + s.height - inset;
        const radius = Math.max(5, 15 - inset);
        const brandBottom = Theme.tabBaseline + Theme.activeTabOverlap + inset;
        const brandRight = Theme.brandWidth + inset;
        const diagonalOffset = inset * (Math.SQRT2 - 1);
        const diagonalTop = Theme.tabBaseline + Theme.activeTabOverlap - Theme.brandBevel + diagonalOffset;
        const diagonalLeft = Theme.brandWidth - Theme.brandBevel + diagonalOffset;
        return "M " + brandRight + " " + top
            + " H " + (right - radius) + " Q " + right + " " + top + " " + right + " " + (top + radius)
            + " V " + (bottom - radius) + " Q " + right + " " + bottom + " " + (right - radius) + " " + bottom
            + " H " + (left + radius) + " Q " + left + " " + bottom + " " + left + " " + (bottom - radius)
            + " V " + (brandBottom + radius) + " Q " + left + " " + brandBottom + " " + (left + radius) + " " + brandBottom
            + " H " + (diagonalLeft - 3) + " Q " + diagonalLeft + " " + brandBottom + " " + (diagonalLeft + 2) + " " + (brandBottom - 2)
            + " L " + (brandRight - 2) + " " + (diagonalTop + 2)
            + " Q " + brandRight + " " + diagonalTop + " " + brandRight + " " + (diagonalTop - 3) + " Z";
    }

    Shape {
        anchors.fill: parent
        ShapePath {
            strokeColor: Theme.rim; strokeWidth: 1
            fillColor: Theme.chassisDark
            PathSvg { path: root.aperture(0) }
        }
        ShapePath {
            strokeColor: "transparent"
            fillGradient: LinearGradient {
                x1: 0; y1: Theme.screenBounds.y
                x2: 0; y2: Theme.screenBounds.y + Theme.screenBounds.height
                GradientStop { position: 0; color: Qt.darker(Theme.chassisDark, 1.12) }
                GradientStop { position: 0.5; color: Qt.lighter(Theme.chassis, 1.2) }
                GradientStop { position: 1; color: Qt.lighter(Theme.rim, 1.08) }
            }
            PathSvg { path: root.aperture(2) }
        }
        ShapePath {
            strokeColor: "#aabfb3"; strokeWidth: 1
            fillColor: Theme.paper
            PathSvg { path: root.aperture(9) }
        }
    }
    ShellBackgroundPattern {
        // Keep the ornament within the clear area below the title and tabs.
        x: Theme.screenBounds.x + 10
        y: Theme.screenBounds.y + Theme.contentTopInset
        width: Theme.screenBounds.width - 20
        height: Theme.screenBounds.y + Theme.screenBounds.height - 10 - y
        clip: true
    }
}
