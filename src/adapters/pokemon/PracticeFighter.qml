import QtQuick
import TrainerOS

Item {
    id: root
    property var fighter: ({})
    property bool opponent: false
    property bool playing: false
    property bool striking: false
    readonly property int effectBeat: fighter.effectBeat || 0
    readonly property string effectKind: fighter.effectKind || ""
    readonly property color effectColor: ({Fire:"#ff773f",Water:"#65caff",Electric:"#ffe34e",Grass:"#9ce065",Ice:"#bdffff",Psychic:"#ff85be",Ghost:"#b896f2",Poison:"#c17bdb",Fighting:"#f1b771",Ground:"#ddab62",Rock:"#d2bd81",Flying:"#c9e9ff",Dragon:"#ad9dff",Dark:"#9698bd",Steel:"#c2e7e9",Bug:"#c1db66"})[fighter.effectElement] || "#fff2af"
    onEffectBeatChanged: Qt.callLater(function() { if (effectBeat > 0 && playing && effectKind !== "move" && effectKind.length) impact.restart() })
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
    Item {
        id: burst
        anchors.centerIn: creature; width: Math.min(creature.width,180); height: width
        property real phase: 1
        opacity: 0
        visible: root.effectKind !== "move" && root.effectKind.length > 0
        readonly property bool healing: root.effectKind === "heal" || root.effectKind === "switch"
        readonly property color tint: healing ? "#a2f4ac" : root.effectKind === "status" ? "#dba5f2" : root.effectColor
        Rectangle {
            anchors.centerIn: parent; width: parent.width*(0.25+burst.phase*0.8); height: width; radius: width/2
            color: "transparent"; border.color: burst.tint; border.width: 5*(1-burst.phase)+1
            visible: root.effectKind !== "miss" && root.effectKind !== "immune"
        }
        Repeater {
            model: 10
            Rectangle {
                required property int index
                readonly property real angle: index*Math.PI/5
                x: burst.width/2+Math.cos(angle)*burst.width*0.45*burst.phase-width/2
                y: burst.height/2+Math.sin(angle)*burst.height*0.45*burst.phase-height/2
                width: 7+13*(1-burst.phase); height: width*(index%2 ? 2 : 1); rotation: index*36+45
                color: index%2 ? "#fffdf1" : burst.tint
                visible: root.effectKind !== "miss" && root.effectKind !== "immune"
            }
        }
        Text {
            anchors.centerIn: parent; text: root.effectKind === "miss" ? "MISS" : root.effectKind === "immune" ? "NO EFFECT" : burst.healing ? "+" : "✦"
            color: "#fffdf1"; style: Text.Outline; styleColor: Qt.darker(burst.tint,1.5)
            font.family: Theme.displayFamily; font.bold: true; font.pixelSize: burst.healing ? 64 : root.effectKind === "miss" || root.effectKind === "immune" ? 24 : 76
            scale: 0.6+burst.phase*0.5
        }
        SequentialAnimation {
            id: impact
            PropertyAction { target: burst; property: "phase"; value: 0 }
            PropertyAction { target: burst; property: "opacity"; value: 1 }
            ParallelAnimation {
                NumberAnimation { target: burst; property: "phase"; to: 1; duration: Theme.motion(650); easing.type: Easing.OutCubic }
                SequentialAnimation { PauseAnimation { duration: Theme.motion(210) } NumberAnimation { target: burst; property: "opacity"; to: 0; duration: Theme.motion(440) } }
            }
        }
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
