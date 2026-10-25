import QtQuick

Flow {
    id: root
    required property var game
    property bool compact: false
    spacing: 6
    height: implicitHeight
    readonly property var facts: {
        const rows = []
        if (game.playInfo && game.playInfo.label) rows.push({label:game.playInfo.label,icon:"players",tint:Theme.green})
        if (game.year) rows.push({label:game.year,icon:"",tint:Theme.yellow})
        if (game.genre && !root.compact) rows.push({label:game.genre,icon:"",tint:Theme.pink})
        return rows
    }
    Repeater {
        model: root.facts
        Rectangle {
            required property var modelData
            height: 26; radius: 9
            width: Math.min(modelData.icon ? 116 : 110, caption.implicitWidth+(modelData.icon ? 39 : 18))
            color: modelData.tint; border.color: Qt.darker(modelData.tint,1.25)
            Rectangle { x: 2; y: 2; width: parent.width-4; height: 1; color: Theme.surfaceColor("#b0ffffff"); radius: 1 }
            Image { x: 8; y: 5; width: 18; height: 18; visible: !!modelData.icon; source: modelData.icon ? "qrc:/home-icons/"+modelData.icon+".svg" : "" }
            Text { id: caption; x: modelData.icon ? 31 : 9; anchors.verticalCenter: parent.verticalCenter; width: parent.width-x-8; text: modelData.label; color: Theme.ink; font.pixelSize: 12; elide: Text.ElideRight }
            Accessible.role: Accessible.StaticText; Accessible.name: modelData.label
        }
    }
}
