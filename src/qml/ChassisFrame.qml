import QtQuick
import QtQuick.Shapes

Rectangle {
    id: root
    objectName: "chassis-frame"
    gradient: Gradient {
        GradientStop { position: 0; color: Theme.chassisCrown }
        GradientStop { position: 0.035; color: Theme.chassisTop }
        GradientStop { position: 0.24; color: Theme.chassisTop }
        GradientStop { position: 0.75; color: Theme.chassis }
        GradientStop { position: 0.94; color: Theme.chassis }
        GradientStop { position: 1; color: Theme.chassisFoot }
    }
    border.color: Theme.edgeShadow

    function perimeter(inset) {
        const l = inset, r = width - inset, t = inset, b = height - inset;
        return "M " + (l + 3) + " " + t + " H " + (r - 3)
            + " Q " + r + " " + t + " " + r + " " + (t + 3)
            + " V " + (b - 3) + " Q " + r + " " + b + " " + (r - 3) + " " + b
            + " H " + (l + 3) + " Q " + l + " " + b + " " + l + " " + (b - 3)
            + " V " + (t + 3) + " Q " + l + " " + t + " " + (l + 3) + " " + t + " Z";
    }

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
        // A single rounded perimeter catches light above/left and rolls into
        // shade below/right. The screen aperture remains a separate recess.
        ShapePath {
            strokeColor: "transparent"; fillRule: ShapePath.OddEvenFill
            fillGradient: LinearGradient {
                x1: 0; y1: 0; x2: root.width; y2: root.height
                GradientStop { position: 0; color: Theme.edgeLight }
                GradientStop { position: 0.35; color: Theme.chassisTop }
                GradientStop { position: 1; color: Theme.edgeShadow }
            }
            PathSvg { path: root.perimeter(1) + root.perimeter(4) }
        }
        ShapePath {
            strokeColor: Theme.rim; strokeWidth: 1
            fillColor: Theme.chassisDark
            PathSvg { path: root.aperture(0) }
        }
        ShapePath {
            strokeColor: "transparent"
            fillGradient: LinearGradient {
                x1: Theme.screenBounds.x; y1: Theme.screenBounds.y
                x2: Theme.screenBounds.x + Theme.screenBounds.width * 0.4
                y2: Theme.screenBounds.y + Theme.screenBounds.height
                GradientStop { position: 0; color: Theme.edgeShadow }
                GradientStop { position: 0.25; color: Theme.chassisDark }
                GradientStop { position: 0.65; color: Qt.lighter(Theme.chassis, 1.3) }
                GradientStop { position: 1; color: Theme.edgeLight }
            }
            PathSvg { path: root.aperture(2) }
        }
        ShapePath {
            strokeColor: "transparent"
            fillGradient: LinearGradient {
                x1: 0; y1: Theme.screenBounds.y
                x2: 0; y2: Theme.screenBounds.y + Theme.screenBounds.height
                GradientStop { position: 0; color: "#9dafaa" }
                GradientStop { position: 0.25; color: "#d1dcd3" }
                GradientStop { position: 1; color: Theme.paper }
            }
            PathSvg { path: root.aperture(9) }
        }
        ShapePath {
            strokeColor: "transparent"
            fillColor: Theme.paper
            PathSvg { path: root.aperture(11) }
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
