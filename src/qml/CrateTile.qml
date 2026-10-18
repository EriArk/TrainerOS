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
    readonly property real lip: featured ? 32 : 26
    readonly property real depth: featured ? 14 : 10
    readonly property real frontWidth: width-depth
    readonly property real inset: featured ? 20 : 16
    readonly property real labelHeight: featured ? 48 : 36
    // Layered contact shadows and bevels, without a full-screen shader pass.
    Rectangle { x: 2; y: 22; width: parent.width-4; height: parent.height-17; radius: 15; color: "#130f2725" }
    Rectangle { x: 3; y: 22; width: parent.width-6; height: parent.height-20; radius: 14; color: "#20102725" }
    Rectangle { x: -4; y: -1; width: root.width+8; height: root.height+4; radius: 18; color: "transparent"; border.width: 4; border.color: "#36ffcc61"; visible: root.selected }
    Rectangle { x: -1; y: 1; width: root.width+2; height: root.height; radius: 15; color: "transparent"; border.width: 2; border.color: Theme.focus; visible: root.selected }
    // A visible return face gives the case real thickness, not a card outline.
    Shape {
        anchors.fill: parent
        ShapePath {
            strokeWidth: 1; strokeColor: Qt.darker(root.material,1.8)
            fillGradient: LinearGradient {
                x1: root.frontWidth; y1: 0; x2: root.width; y2: 0
                GradientStop { position: 0; color: Qt.darker(root.material,1.55) }
                GradientStop { position: 1; color: Qt.darker(root.material,1.18) }
            }
            PathSvg { path: "M"+root.frontWidth+" 30 L"+root.width+" 19 L"+root.width+" "+(root.height-17)+" Q"+root.width+" "+(root.height-11)+" "+(root.width-6)+" "+(root.height-7)+" L"+(root.frontWidth-5)+" "+(root.height-2)+" Z" }
        }
    }
    Rectangle {
        id: body
        x: 1; y: 23; width: root.frontWidth-2; height: root.height-26; radius: 9
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.lighter(root.material, 1.22) }
            GradientStop { position: 0.6; color: root.material }
            GradientStop { position: 1; color: Qt.darker(root.material, 1.19) }
        }
        border.width: 1; border.color: Qt.darker(root.material,1.6)
        Rectangle { anchors.fill: parent; anchors.margins: 3; radius: 7; color: "transparent"; border.color: "#6affffdd" }
        Rectangle { x: 8; y: parent.height-4; width: parent.width-16; height: 3; radius: 1; color: Qt.darker(root.material,1.38); border.color: "#40ffffdd" }
        Rectangle {
            id: artSlot
            x: root.inset; y: root.lip-2; width: parent.width-2*x; height: parent.height-y-root.labelHeight
            radius: 9; color: Qt.darker(root.material,1.35)
            // Inset image has a narrow cast edge; the lid overlaps its top.
            CrateIllustration { anchors.fill: parent; anchors.margins: 3; art: root.entry.art; clip: true }
            Rectangle { anchors.fill: parent; radius: 8; color: "transparent"; border.width: 3; border.color: Qt.lighter(root.material,1.18) }
            Rectangle { x: 3; y: 3; width: parent.width-6; height: 2; color: "#450e2928" }
        }
        CrateEmblem { x: root.inset; y: parent.height-root.labelHeight+6; width: root.featured ? 35 : 24; height: width; art: root.entry.art; accent: root.lidColor }
        Text {
            x: root.inset+(root.featured ? 44 : 32); y: parent.height-root.labelHeight+5
            width: parent.width-x-root.inset; height: root.featured ? 24 : 25
            text: root.entry.title; color: Theme.ink
            font.family: Theme.displayFamily; font.pixelSize: root.featured ? 24 : 17
            font.weight: Font.Medium; fontSizeMode: Text.Fit; minimumPixelSize: 13; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
        }
        Text {
            visible: root.featured; x: root.inset+44; y: parent.height-23
            width: parent.width-x-root.inset; text: root.entry.subtitle
            font.pixelSize: 13; color: Qt.darker(root.material,2.4); elide: Text.ElideRight
        }
        // Reinforced corner rails, with individual inset rivets.
        Repeater {
            model: 2
            Rectangle {
                required property int index
                x: index===0 ? 4 : body.width-14; y: root.lip-7
                width: 10; height: body.height-y-7; radius: 3
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: Qt.darker(root.material,1.38) }
                    GradientStop { position: 0.3; color: Qt.lighter(root.material,1.22) }
                    GradientStop { position: 1; color: Qt.darker(root.material,1.18) }
                }
                border.color: Qt.darker(root.material,1.4)
                Rectangle { anchors.horizontalCenter: parent.horizontalCenter; y: 5; width: 3; height: 3; radius: 1.5; color: Qt.darker(root.material,1.8); border.color: "#60fff1d0" }
                Rectangle { anchors.horizontalCenter: parent.horizontalCenter; y: parent.height-9; width: 3; height: 3; radius: 1.5; color: Qt.darker(root.material,1.8); border.color: "#60fff1d0" }
            }
        }
    }
    Item {
        id: lid
        x: 0; y: root.opening ? -8 : 0; width: root.width; height: root.lip+18
        rotation: root.opening ? -2 : 0
        Behavior on y { NumberAnimation { duration: Theme.motion(150); easing.type: Easing.OutCubic } }
        Behavior on rotation { NumberAnimation { duration: Theme.motion(150) } }
        Rectangle { x: 1; y: 24; width: root.frontWidth-2; height: root.lip; radius: 7; color: "#650c2729" }
        Shape {
            width: parent.width; height: 27
            ShapePath {
                strokeColor: Qt.darker(root.lidColor,1.45); strokeWidth: 1
                fillGradient: LinearGradient { x1: 0; y1: 0; x2: 0; y2: 27; GradientStop { position: 0; color: Qt.lighter(root.lidColor,1.5) } GradientStop { position: 1; color: Qt.lighter(root.lidColor,1.1) } }
                PathSvg { path: "M"+(root.depth+6)+" 1 L"+(lid.width-6)+" 1 Q"+lid.width+" 1 "+lid.width+" 7 L"+root.frontWidth+" 27 L0 27 L"+root.depth+" 5 Q"+(root.depth+2)+" 1 "+(root.depth+6)+" 1 Z" }
            }
        }
        // Recessed panel on the lid's horizontal plane.
        Shape {
            anchors.fill: parent
            ShapePath {
                strokeColor: "#80fff5d8"; strokeWidth: 1; fillColor: Qt.darker(root.lidColor,1.06)
                PathSvg { path: "M"+(root.depth+24)+" 7 L"+(lid.width-25)+" 7 L"+(root.frontWidth-19)+" 18 L20 18 Z" }
            }
            ShapePath {
                strokeWidth: 1; strokeColor: Qt.darker(root.lidColor,1.6); fillColor: Qt.darker(root.lidColor,1.28)
                PathSvg { path: "M"+root.frontWidth+" 25 L"+lid.width+" 7 L"+lid.width+" "+(root.lip+9)+" L"+root.frontWidth+" "+(root.lip+22)+" Z" }
            }
        }
        Rectangle {
            x: 0; y: 23; width: root.frontWidth; height: root.lip-8; radius: 5
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
                    x: index===0 ? 8 : root.frontWidth-21; y: 2; width: 13; height: parent.height-4; radius: 2
                    gradient: Gradient { GradientStop { position: 0; color: Qt.lighter(root.lidColor,1.2) } GradientStop { position: 1; color: Qt.darker(root.lidColor,1.18) } }
                    border.color: "#49606553"
                }
            }
        }
        Rectangle {
            x: (root.frontWidth-width)/2; y: root.lip+4
            width: root.featured ? 33 : 25; height: root.featured ? 27 : 23; radius: 4
            color: Qt.darker(root.lidColor,1.15); border.width: 2; border.color: Qt.lighter(root.lidColor,1.3)
            Rectangle { anchors.centerIn: parent; width: parent.width-14; height: 9; radius: 3; color: Qt.darker(root.material,2.2); border.color: "#66fff0d0" }
            Rectangle { anchors.horizontalCenter: parent.horizontalCenter; y: 3; width: parent.width-10; height: 3; radius: 1; color: Qt.lighter(root.lidColor,1.5) }
        }
    }
    MouseArea { anchors.fill: parent; onClicked: root.activated(); cursorShape: Qt.PointingHandCursor }
    Accessible.role: Accessible.Button
    Accessible.name: entry.title
    Accessible.onPressAction: root.activated()
}
