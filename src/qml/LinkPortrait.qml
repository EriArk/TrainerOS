import QtQuick

Rectangle {
    id: root
    property var member: ({})
    property string emotion: "Normal"
    property color tint: Theme.blue
    property bool selected: false
    readonly property var portrait: member.portraits ? (member.portraits[emotion] || member.portraits.Normal || ({})) : ({})
    radius: Math.max(6, width * 0.12)
    color: Qt.lighter(tint, 1.2)
    border.width: selected ? 3 : 2
    border.color: selected ? Theme.focusGlow : Qt.darker(tint, 1.65)
    Rectangle { anchors.fill: parent; anchors.margins: 4; radius: parent.radius - 3; color: root.tint; opacity: 0.4 }
    Image {
        anchors.fill: parent; anchors.margins: 6
        source: root.portrait.url || (root.member.art ? root.member.art.url || "" : "")
        fillMode: Image.PreserveAspectFit; smooth: !root.portrait.url; asynchronous: true
    }
    Rectangle { x: 7; y: 3; width: Math.max(0,parent.width-14); height: 1; color: "#95ffffff" }
}
