import QtQuick

Item {
    id: root
    property var fighter: ({})
    property bool opponent: false
    property bool playing: false
    property bool striking: false
    readonly property int attackBeat: fighter.attackBeat || 0
    onAttackBeatChanged: if (attackBeat > 0 && playing) strike.restart()
    property bool showHealth: true
    visible: !!fighter.name
    readonly property var idle: fighter.clips && fighter.clips.Idle ? fighter.clips.Idle : fighter.sprite || ({})
    onStrikingChanged: if (striking) strike.restart()
    Rectangle {
        x: parent.width * 0.10; y: parent.height - 21
        width: parent.width * 0.80; height: 28; radius: width / 2
        color: "#9bba6e"; border.color: "#d3e99a"; border.width: 5
        Rectangle { anchors.fill: parent; anchors.margins: 10; radius: width / 2; color: "#688e53"; opacity: 0.35 }
    }
    Item {
        id: creature
        x: 0; y: 68; width: parent.width; height: Math.max(35, parent.height - y - 7)
        opacity: root.fighter.maxHp && root.fighter.battleHp === 0 ? 0.35 : 1
        Behavior on opacity { NumberAnimation { duration: Theme.motion(300) } }
        SpritePreview {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: Math.min(parent.height, cellHeight * pixelScale)
            asset: root.idle; playing: root.playing
            pixelScale: 6; trimTransparentMargins: true; visible: !!root.idle.url
        }
        Image { anchors.fill: parent; source: root.idle.url ? "" : (root.fighter.art && root.fighter.art.url ? root.fighter.art.url : ""); fillMode: Image.PreserveAspectFit; verticalAlignment: Image.AlignBottom; asynchronous: true }
    }
    SequentialAnimation {
        id: strike
        NumberAnimation { target: creature; property: "x"; to: root.opponent ? -22 : 22; duration: Theme.motion(95) }
        NumberAnimation { target: creature; property: "x"; to: 0; duration: Theme.motion(170) }
    }
    Rectangle {
        anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width, 290); height: root.showHealth ? 67 : 58
        radius: 7; color: "#f5f4d9"; border.width: 2; border.color: "#54785d"
        visible: !!root.fighter.name
        LinkPortrait { x: 5; y: 5; width: 53; height: 53; member: root.fighter; tint: root.opponent ? Theme.pink : Theme.blue; emotion: root.fighter.maxHp && root.fighter.battleHp === 0 ? "Pain" : "Normal" }
        Text { x: 66; y: 9; width: parent.width - 122; text: root.fighter.name || ""; color: Theme.ink; font.pixelSize: 15; font.bold: true; elide: Text.ElideRight }
        Text { anchors.right: parent.right; anchors.rightMargin: 9; y: 10; text: root.fighter.level ? "Lv. " + root.fighter.level : ""; color: Theme.muted; font.pixelSize: 12 }
        Rectangle {
            x: 66; y: 37; width: parent.width - 136; height: 9; radius: 4; color: "#c5d1ba"; visible: root.showHealth
            Rectangle { width: parent.width * Math.max(0, Math.min(1, root.fighter.hpRatio || 0)); height: parent.height; radius: 4; color: root.fighter.hpRatio > 0.5 ? "#67b768" : root.fighter.hpRatio > 0.2 ? "#edbd48" : "#df6a62"; Behavior on width { NumberAnimation { duration: Theme.motion(300) } } }
        }
        Text { anchors.right: parent.right; anchors.rightMargin: 9; y: 35; text: (root.fighter.battleHp || 0) + "/" + (root.fighter.maxHp || 0); color: Theme.ink; font.pixelSize: 11; visible: root.showHealth }
        Text { anchors.top: parent.bottom; anchors.topMargin: 3; anchors.right: parent.right; text: (root.fighter.battleStatus || "").toUpperCase(); color: "#73354e"; font.pixelSize: 13; font.bold: true }
    }
}
