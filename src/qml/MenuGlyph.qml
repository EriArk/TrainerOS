import QtQuick
import QtQuick.Shapes

Item {
    id: root
    property string glyph: ""
    property color ink: Theme.ink
    readonly property var paths: ({
        "chevron": "M8 4 L16 12 L8 20",
        "settings": "M9 2 H15 V5 L18 7 L21 6 L23 11 L20 13 V16 L22 18 L18 22 L15 20 H12 L10 22 L5 19 L6 16 L4 13 L1 12 L3 7 H6 L9 5 Z M8 12 A4 4 0 1 0 16 12 A4 4 0 1 0 8 12",
        "power": "M12 2 V11 M6 5 A9 9 0 1 0 18 5",
        "desktop": "M2 3 H22 V17 H2 Z M8 22 H16 M12 17 V22",
        "sun": "M8 12 A4 4 0 1 0 16 12 A4 4 0 1 0 8 12 M12 1 V4 M12 20 V23 M1 12 H4 M20 12 H23 M4 4 L6 6 M18 18 L20 20 M4 20 L6 18 M18 6 L20 4",
        "wifi": "M2 8 Q12 -1 22 8 M5 12 Q12 5 19 12 M8 16 Q12 12 16 16 M11 20 H13",
        "bluetooth": "M12 1 L19 7 L5 18 M12 23 L19 17 L5 6 M12 1 V23",
        "airplane": "M10 2 L14 2 L15 9 L23 14 V17 L15 14 V20 L18 22 H6 L9 20 V14 L1 17 V14 L9 9 Z",
        "download": "M12 2 V16 M6 10 L12 16 L18 10 M3 18 V22 H21 V18",
        "battery": "M2 6 H20 V18 H2 Z M22 10 V14 M5 9 H14 V15 H5 Z",
        "clock": "M2 12 A10 10 0 1 0 22 12 A10 10 0 1 0 2 12 M12 5 V12 L17 15",
        "storage": "M5 2 H19 V22 H5 Z M8 5 H16 M8 17 H10 M8 12 H16",
        "pictures": "M2 5 H19 V22 H2 Z M6 5 V1 H23 V18 H19 M5 18 L9 12 L12 16 L15 13 L18 18",
        "close": "M6 6 L18 18 M18 6 L6 18",
        "send": "M3 3 L22 12 L3 21 L7 12 Z M7 12 H16",
        "game-controller": "M7 6 H17 Q20 6 21 10 L23 17 Q23 21 19 19 L16 16 H8 L5 19 Q1 21 1 17 L3 10 Q4 6 7 6 Z M7 9 V15 M4 12 H10 M16 10 V11 M19 13 V14",
        "file": "M6 2 H14 L20 8 V22 H6 Z M14 2 V8 H20 M9 12 H17 M9 16 H17",
        "chat": "M5 3 H19 Q22 3 22 6 V15 Q22 18 19 18 H10 L5 22 V18 Q2 18 2 15 V6 Q2 3 5 3 Z M6 9 H18 M6 13 H14",
        "expand": "M9 3 H3 V9 M3 3 L10 10 M15 21 H21 V15 M21 21 L14 14",
        "collapse": "M3 3 L10 10 M4 10 H10 V4 M21 21 L14 14 M14 20 V14 H20",
        "microphone": "M9 5 A3 3 0 0 1 15 5 V12 A3 3 0 0 1 9 12 Z M6 10 V12 Q6 18 12 18 Q18 18 18 12 V10 M12 18 V22 M8 22 H16",
        "speaker": "M3 9 H7 L13 4 V20 L7 15 H3 Z M16 8 Q20 12 16 16 M19 5 Q25 12 19 19",
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
