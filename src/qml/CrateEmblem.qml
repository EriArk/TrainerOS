import QtQuick
import QtQuick.Shapes

Rectangle {
    id: root
    property int art: 0
    property color accent: Theme.rim
    radius: width/2; color: Qt.darker(accent,1.25)
    border.width: 1; border.color: Qt.lighter(accent,1.65)
    Rectangle { anchors.fill: parent; anchors.margins: 4; radius: width/2; color: root.art===1||root.art===5 ? Qt.lighter(root.accent,1.5) : Theme.ink; border.color: "#88ffe8b2" }
    Item {
        anchors.centerIn: parent; width: 24; height: 24; scale: (root.width-14)/24
        Image { anchors.fill: parent; visible: root.art===1||root.art===5; source: root.art===1?"qrc:/social-icons/wrench.svg":"qrc:/social-icons/gear.svg" }
        Shape {
            anchors.fill: parent; visible: root.art!==1&&root.art!==5
            ShapePath {
                fillColor: "#f8e7b1"; strokeColor: "transparent"
                PathSvg { path: root.art===0 ? "M12 1 Q13 10 23 12 Q13 14 12 23 Q10 14 1 12 Q10 10 12 1 Z" : root.art===2 ? "M1 21 L9 4 L14 14 L17 8 L24 21 Z" : root.art===3 ? "M5 6 Q2 6 1 17 Q1 21 5 19 L8 16 L16 16 L19 19 Q23 21 23 17 L21 8 Q20 6 17 6 Z" : "M5 2 L8 2 L8 6 L11 6 L11 2 L14 2 L14 6 L17 6 L17 2 L20 2 L20 9 L17 12 L17 18 L20 20 L20 23 L5 23 L5 20 L8 18 L8 12 L5 9 Z" }
            }
        }
        Rectangle { visible: root.art===3; x: 5; y: 10; width: 6; height: 2; color: Theme.ink }
        Rectangle { visible: root.art===3; x: 7; y: 8; width: 2; height: 6; color: Theme.ink }
        Rectangle { visible: root.art===3; x: 17; y: 9; width: 3; height: 3; radius: 2; color: Theme.ink }
    }
}
