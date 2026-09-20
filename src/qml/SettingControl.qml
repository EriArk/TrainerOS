import QtQuick

Item {
    id: root
    property string title: ""
    property string detail: ""
    property string kind: "action"
    property bool selected: false
    property bool checked: false
    property int level: -1
    property bool muted: false
    property bool compact: false
    readonly property bool slider: kind === "volume" || kind === "brightness"
    signal activated()
    signal levelRequested(int value)
    implicitHeight: compact ? 43 : 78
    Accessible.role: slider ? Accessible.Slider : Accessible.Button
    Accessible.name: title + ", " + (slider ? level < 0 ? "Unavailable" : level + "%" : detail)
    onSelectedChanged: if(selected && visible) forceActiveFocus(Qt.OtherFocusReason)
    onVisibleChanged: if(selected && visible) forceActiveFocus(Qt.OtherFocusReason)
    Component.onCompleted: if(selected && visible) forceActiveFocus(Qt.OtherFocusReason)
    Rectangle { anchors.fill: parent; radius: 10; color: root.selected ? "#fff4ce" : "#eaf0e6"; border.color: root.selected ? Theme.focus : "#bdcdbd"; border.width: root.selected ? 3 : 1 }
    Rectangle { anchors.fill: parent; anchors.margins: -3; radius: 13; color: "transparent"; visible: root.selected; border.width: 3; border.color: Theme.focusGlow; opacity: 0.45 }
    Text { x: 13; y: root.compact ? 4 : 12; text: root.title; color: root.kind === "unavailable" ? Theme.muted : Theme.ink; font.pixelSize: root.compact ? 13 : 19; font.bold: !root.compact }
    Text {
        anchors.right: parent.right; anchors.rightMargin: 14; y: root.compact ? 4 : 12
        text: root.slider ? root.level < 0 ? "Unavailable" : (root.muted ? "Muted · " : "") + root.level + "%" : root.kind === "theme" ? Theme.themeId : ""
        color: Theme.muted; font.pixelSize: root.compact ? 12 : 15
    }
    Text { x: 13; y: 42; width: parent.width-85; text: root.detail; visible: !root.compact && !root.slider && root.kind !== "theme"; font.pixelSize: 14; color: Theme.muted; elide: Text.ElideRight }
    Rectangle {
        id: track; x: 14; y: root.compact ? 27 : 51; width: parent.width-28; height: root.compact ? 7 : 9; radius: height/2
        visible: root.slider; color: "#b2c4b8"
        Rectangle { width: parent.width*Math.min(100,Math.max(0,root.level))/100; height: parent.height; radius: parent.radius; color: root.level<0 ? "transparent" : Theme.chassis }
        Rectangle { x: (parent.width-width)*Math.min(100,Math.max(0,root.level))/100; y: (parent.height-height)/2; width: root.compact ? 14 : 20; height: width; radius: width/2; color: "#fffdf0"; border.color: Theme.chassis; border.width: 2; visible: root.level>=0 }
        MouseArea { anchors.fill: parent; anchors.margins: -7; enabled: root.level>=0; onPressed: mouse => root.levelRequested(Math.round(Math.max(0,Math.min(1,(mouse.x-7)/track.width))*20)*5); onPositionChanged: mouse => {if(pressed) root.levelRequested(Math.round(Math.max(0,Math.min(1,(mouse.x-7)/track.width))*20)*5)} }
    }
    Row {
        x: 14; y: root.compact ? 23 : 41; spacing: root.compact ? 12 : 19; visible: root.kind === "theme"
        Repeater {
            model: ["turquoise","red","green","blue","orange"]
            Rectangle { required property string modelData; width: root.compact ? 15 : 24; height: width; radius: width/2; color: Theme.palettes[modelData][0]; border.width: modelData===Theme.themeId ? 3 : 1; border.color: modelData===Theme.themeId ? Theme.focus : "#759084" }
        }
    }
    Rectangle { anchors.right: parent.right; anchors.rightMargin: 18; anchors.verticalCenter: parent.verticalCenter; width: 48; height: 26; radius: 13; visible: root.kind==="toggle"; color: root.checked ? Theme.chassis : "#acbdb0"; Rectangle { x: root.checked ? 25 : 3; y: 3; width: 20; height: 20; radius: 10; color: "#fffdf0" } }
    Text { anchors.right: parent.right; anchors.rightMargin: 20; anchors.verticalCenter: parent.verticalCenter; text: root.kind==="action" ? "›" : "—"; visible: root.kind==="action" || root.kind==="unavailable"; color: Theme.muted; font.pixelSize: 24 }
    // Keep slider dragging above the row activation surface.
    TapHandler { enabled: !root.slider; onTapped: root.activated() }
}
