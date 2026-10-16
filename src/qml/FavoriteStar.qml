import QtQuick
import QtQuick.Shapes

Item {
    id: root
    width: 36; height: 36
    property bool marked: false
    property bool active: true
    signal activated()
    Accessible.role: Accessible.CheckBox
    Accessible.name: marked ? "Remove from favorites" : "Add to favorites"
    Accessible.checkable: true
    Accessible.checked: marked
    Accessible.onPressAction: if (active) activated()
    opacity: active ? 1 : 0.55
    Shape {
        x: 6; y: 7; width: 24; height: 24
        ShapePath {
            strokeColor: "#b8ffffff"; strokeWidth: 3.5
            fillColor: "#70263732"
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: "M12 1 L15.4 8 L23 9 L17.5 14.4 L18.8 22 L12 18.4 L5.2 22 L6.5 14.4 L1 9 L8.6 8 Z" }
        }
        ShapePath {
            strokeColor: root.marked ? "#946422" : "#586960"; strokeWidth: 1.3
            fillColor: root.marked ? "#ffd569" : "#e9f0e4"
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: "M12 1 L15.4 8 L23 9 L17.5 14.4 L18.8 22 L12 18.4 L5.2 22 L6.5 14.4 L1 9 L8.6 8 Z" }
        }
    }
    scale: touch.pressed ? 0.92 : 1
    Behavior on scale { NumberAnimation { duration: Theme.motion(80) } }
    MouseArea { id: touch; anchors.fill: parent; enabled: root.active; onClicked: root.activated() }
}
