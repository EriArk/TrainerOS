import QtQuick
import QtQuick.Shapes

Item {
    id: root
    property string glyph: ""
    property color ink: Theme.ink
    readonly property var paths: ({
        "guide": "M3 4 Q8 2 12 5 Q16 2 21 4 V20 Q16 18 12 21 Q8 18 3 20 Z M12 5 V21",
        "party": "M8 3 A3 3 0 1 0 8 9 A3 3 0 1 0 8 3 M16 5 A3 3 0 1 0 16 11 A3 3 0 1 0 16 5 M2 21 V16 Q8 10 13 16 V21 M14 14 Q20 12 22 18 V21",
        "boxes": "M3 3 H21 V21 H3 Z M3 9 H21 M3 15 H21 M9 3 V21 M15 3 V21",
        "care": "M9 3 H15 V9 H21 V15 H15 V21 H9 V15 H3 V9 H9 Z",
        "play": "M6 3 L21 12 L6 21 Z",
        "shop": "M3 9 L5 3 H19 L21 9 V12 H3 Z M5 12 V21 H19 V12 M10 21 V15 H15 V21",
        "profile": "M3 4 H21 V20 H3 Z M8 8 A2 2 0 1 0 8 12 A2 2 0 1 0 8 8 M5 17 Q8 12 11 17 M15 8 H18 M15 12 H18 M15 16 H18",
        "journey": "M3 5 L9 3 L15 6 L21 3 V19 L15 22 L9 19 L3 21 Z M9 3 V19 M15 6 V22",
        "trophy": "M7 3 H17 V10 Q17 16 12 16 Q7 16 7 10 Z M7 5 H3 V9 Q3 12 8 12 M17 5 H21 V9 Q21 12 16 12 M12 16 V21 M7 21 H17",
        "companion": "M4 7 Q2 1 8 4 Q12 1 16 4 Q22 1 20 7 V16 Q12 23 4 16 Z M8 11 V13 M16 11 V13"
    })
    Shape {
        width: 24; height: 24
        scale: Math.min(root.width,root.height)/24; transformOrigin: Item.TopLeft
        ShapePath {
            strokeColor: root.ink; strokeWidth: 1.7; fillColor: "transparent"
            joinStyle: ShapePath.RoundJoin; capStyle: ShapePath.RoundCap
            PathSvg { path: root.paths[root.glyph] || "M4 4 H20 V20 H4 Z M8 8 H16 V16 H8 Z" }
        }
    }
}
