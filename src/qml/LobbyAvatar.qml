import QtQuick
import QtQuick.Effects

Item {
    id: root
    required property var person
    property bool selected: false
    property bool small: false
    property bool compact: false
    property color accent: Theme.blue
    signal activated()
    readonly property real diameter: Math.min(width - 16, height - (small ? 30 : compact ? 46 : 54))
    Accessible.role: Accessible.Button
    Accessible.name: (person.name || "Free place") + " · " + (person.status || "Invite")
    Accessible.onPressAction: activated()
    Rectangle {
        id: circle; width: Math.max(20,root.diameter); height: width; radius: width/2
        anchors.horizontalCenter: parent.horizontalCenter; y: 5
        gradient: Gradient { GradientStop { position: 0; color: Qt.lighter(Theme.rim,1.35) } GradientStop { position: 1; color: Theme.chassisDark } }
        Rectangle { x: -2; y: 4; width: parent.width+4; height: parent.height; radius: width/2; z: -1; color: "#24193f3b" }
        Rectangle { anchors.fill: parent; anchors.margins: -4; radius: width/2; color: "transparent"
            border.width: root.selected ? 2 : root.person.speaking ? 3 : 0; border.color: root.selected ? Theme.focusGlow : "#63b877"
            SequentialAnimation on opacity { running: !!root.person.speaking && root.visible && !Theme.reducedMotion; loops: Animation.Infinite
                NumberAnimation { from: 1; to: .5; duration: 500 } NumberAnimation { from: .5; to: 1; duration: 500 } }
        }
        Rectangle { anchors.fill: parent; anchors.margins: 3; radius: width/2; color: Theme.paper }
        Rectangle { id: face; anchors.fill: parent; anchors.margins: 5; radius: width/2
            gradient: Gradient { GradientStop { position: 0; color: Qt.lighter(root.accent,1.16) } GradientStop { position: 1; color: root.accent } }
            opacity: root.person.empty ? .45 : 1
            Text { anchors.centerIn: parent; visible: !root.person.empty; text: (root.person.name || "?").substring(0,1).toUpperCase()
                color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: parent.height * .48 }
            Item { anchors.centerIn: parent; width: parent.width * .36; height: width; visible: !!root.person.empty
                Rectangle { anchors.centerIn: parent; width: parent.width; height: Math.max(3,parent.width*.13); radius: height/2; color: Theme.ink }
                Rectangle { anchors.centerIn: parent; height: parent.height; width: Math.max(3,parent.height*.13); radius: width/2; color: Theme.ink }
            }
            Image { id: photo; anchors.fill: parent; visible: false; asynchronous: true; source: root.person.avatar || ""
                sourceSize: Qt.size(160,160); fillMode: Image.PreserveAspectCrop }
            Rectangle { id: mask; anchors.fill: parent; radius: width/2; visible: false; layer.enabled: true; color: "white" }
            MultiEffect { anchors.fill: parent; source: photo; maskEnabled: true; maskSource: mask; visible: photo.status===Image.Ready }
        }
        Rectangle { visible: !!root.person.seat && !root.small; anchors.horizontalCenter: parent.horizontalCenter; y: parent.height-12
            width: 32; height: 18; radius: 8; color: root.person.host ? Theme.yellow : Theme.paper; border.color: Theme.rim
            Text { anchors.centerIn: parent; text: "P"+ (root.person.seat || ""); font.family: Theme.displayFamily; font.pixelSize: 12; color: Theme.ink }
        }
        Rectangle { visible: !!root.person.ready || (!!root.person.host && !root.person.seat); x: parent.width-18; y: 0; width: 22; height: 22; radius: 11
            color: root.person.ready ? Theme.green : Theme.yellow; border.width: 2; border.color: Theme.paper
            Text { anchors.centerIn: parent; text: root.person.ready ? "✓" : "★"; font.pixelSize: 13; color: Theme.ink }
        }
    }
    Text { x: 1; y: circle.y+circle.height+(root.compact ? 7 : 10); width: parent.width-2; horizontalAlignment: Text.AlignHCenter
        text: root.person.name || "Invite"; font.family: Theme.displayFamily; font.pixelSize: root.small ? 11 : root.compact ? 15 : 19; color: Theme.ink
        elide: Text.ElideRight; textFormat: Text.PlainText }
    Text { visible: !root.small; y: circle.y+circle.height+(root.compact ? 26 : 34); width: parent.width; horizontalAlignment: Text.AlignHCenter
        text: root.person.status || (root.person.seat ? "Free place" : "Invite friends"); font.pixelSize: root.compact ? 9 : 11; color: root.person.ready ? "#467747" : Theme.muted
        elide: Text.ElideRight; textFormat: Text.PlainText }
    MouseArea { anchors.fill: parent; onClicked: root.activated() }
}
