import QtQuick
import QtQuick.Shapes

Item {
    id: root
    required property var entry
    property bool featured: false
    property bool selected: false
    property bool opening: false
    signal activated()
    readonly property color material: entry.color || Theme.rim
    readonly property color lidColor: entry.lid || material
    readonly property real lip: featured ? 29 : 23
    readonly property real inset: featured ? 14 : 10
    readonly property real labelHeight: featured ? 48 : 33
    // Layered contact shadows and bevels, without a full-screen shader pass.
    Rectangle { x: 2; y: 22; width: parent.width-4; height: parent.height-17; radius: 15; color: "#130f2725" }
    Rectangle { x: 3; y: 22; width: parent.width-6; height: parent.height-20; radius: 14; color: "#20102725" }
    Rectangle { x: -4; y: -1; width: root.width+8; height: root.height+4; radius: 18; color: "transparent"; border.width: 4; border.color: "#36ffcc61"; visible: root.selected }
    Rectangle { x: -1; y: 1; width: root.width+2; height: root.height; radius: 15; color: "transparent"; border.width: 2; border.color: Theme.focus; visible: root.selected }
    Rectangle {
        id: body
        x: 1; y: 17; width: root.width-2; height: root.height-20; radius: 14
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.lighter(root.material, 1.22) }
            GradientStop { position: 0.6; color: root.material }
            GradientStop { position: 1; color: Qt.darker(root.material, 1.19) }
        }
        border.width: 1; border.color: Qt.darker(root.material,1.6)
        Rectangle { anchors.fill: parent; anchors.margins: 3; radius: 11; color: "transparent"; border.color: "#6affffdd" }
        Rectangle {
            id: artSlot
            x: root.inset; y: root.lip-4; width: parent.width-2*x; height: parent.height-y-root.labelHeight
            radius: 9; color: Qt.darker(root.material,1.35)
            // Inset image has a narrow cast edge; the lid overlaps its top.
            CrateIllustration { anchors.fill: parent; anchors.margins: 3; art: root.entry.art; clip: true }
            Rectangle { anchors.fill: parent; radius: 8; color: "transparent"; border.width: 3; border.color: Qt.lighter(root.material,1.18) }
            Rectangle { x: 3; y: 3; width: parent.width-6; height: 2; color: "#450e2928" }
        }
        CrateEmblem { x: root.inset; y: parent.height-root.labelHeight+6; width: root.featured ? 35 : 24; height: width; art: root.entry.art; accent: root.lidColor }
        Text {
            x: root.inset+(root.featured ? 44 : 32); y: parent.height-root.labelHeight+5
            width: parent.width-x-root.inset; height: root.featured ? 24 : 23
            text: root.entry.title; color: Theme.ink
            font.family: Theme.displayFamily; font.pixelSize: root.featured ? 24 : 17
            font.weight: Font.Medium; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
        }
        Text {
            visible: root.featured; x: root.inset+44; y: parent.height-21
            width: parent.width-x-root.inset; text: root.entry.subtitle
            font.pixelSize: 13; color: Qt.darker(root.material,2.4); elide: Text.ElideRight
        }
        // Small molded corner ribs, rather than an extra frame around the tile.
        Repeater {
            model: 2
            Rectangle {
                required property int index
                x: index===0 ? 4 : body.width-9; y: root.lip+10
                width: 5; height: Math.max(12,artSlot.height-18); radius: 2
                color: Qt.darker(root.material,1.12); border.color: "#36ffffff"
            }
        }
    }
    Item {
        id: lid
        x: 0; y: root.opening ? -8 : 0; width: root.width; height: root.lip+18
        rotation: root.opening ? -2 : 0
        Behavior on y { NumberAnimation { duration: Theme.motion(150); easing.type: Easing.OutCubic } }
        Behavior on rotation { NumberAnimation { duration: Theme.motion(150) } }
        Rectangle { x: 1; y: 11; width: parent.width-2; height: root.lip+6; radius: 10; color: "#550c2729" }
        Shape {
            width: parent.width; height: 19
            ShapePath {
                strokeColor: Qt.darker(root.lidColor,1.45); strokeWidth: 1
                fillGradient: LinearGradient { x1: 0; y1: 0; x2: 0; y2: 19; GradientStop { position: 0; color: Qt.lighter(root.lidColor,1.45) } GradientStop { position: 1; color: root.lidColor } }
                PathSvg { path: "M16 0 L"+(lid.width-16)+" 0 Q"+(lid.width-9)+" 0 "+(lid.width-6)+" 6 L"+lid.width+" 19 L0 19 L6 6 Q9 0 16 0 Z" }
            }
        }
        Rectangle {
            x: 0; y: 12; width: parent.width; height: root.lip-2; radius: 7
            gradient: Gradient {
                GradientStop { position: 0; color: Qt.lighter(root.lidColor,1.34) }
                GradientStop { position: 0.2; color: Qt.lighter(root.lidColor,1.16) }
                GradientStop { position: 0.64; color: root.lidColor }
                GradientStop { position: 1; color: Qt.darker(root.lidColor,1.23) }
            }
            border.width: 1; border.color: Qt.darker(root.lidColor,1.5)
            Rectangle { x: 8; y: 3; width: parent.width-16; height: 2; radius: 1; color: "#aaffefd1" }
            Rectangle { x: 14; y: 9; width: parent.width-28; height: 5; radius: 2; color: "#13000000"; border.color: "#33ffffff" }
            Repeater {
                model: 2
                Rectangle {
                    required property int index
                    x: index===0 ? 18 : lid.width-29; y: 5; width: 11; height: parent.height-7; radius: 3
                    gradient: Gradient { GradientStop { position: 0; color: Qt.lighter(root.lidColor,1.2) } GradientStop { position: 1; color: Qt.darker(root.lidColor,1.18) } }
                    border.color: "#49606553"
                }
            }
        }
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter; y: root.lip-9
            width: root.featured ? 42 : 30; height: root.featured ? 29 : 24; radius: 6
            color: Qt.darker(root.lidColor,1.15); border.width: 2; border.color: Qt.lighter(root.lidColor,1.3)
            Rectangle { anchors.centerIn: parent; width: parent.width-14; height: 9; radius: 3; color: Qt.darker(root.material,2.2); border.color: "#66fff0d0" }
        }
    }
    MouseArea { anchors.fill: parent; onClicked: root.activated(); cursorShape: Qt.PointingHandCursor }
    Accessible.role: Accessible.Button
    Accessible.name: entry.title
    Accessible.onPressAction: root.activated()
}
