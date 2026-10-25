import QtQuick
Item {
    id: root
    required property var shell
    readonly property var choice: shell.homeGame
    readonly property real unit: 1
    readonly property bool compact: height < 180
    clip: true
    FontMetrics { id: summaryMetrics; font: summary.font }
    Column {
        x: 5*root.unit; y: root.compact ? 2 : 8; width: parent.width-10*root.unit; spacing: root.compact ? 3 : 8
        Row {
            width: parent.width; height: 18; spacing: 9
            Text { anchors.verticalCenter: parent.verticalCenter; width: Math.min(implicitWidth,Math.max(0,parent.width-marks.width-parent.spacing)); text: root.choice.system || "WELCOME HOME"; color: Theme.textColor("#d4e6c5"); font.pixelSize: 12; font.letterSpacing: 1; elide: Text.ElideRight }
            HomeConnectionMarks { id: marks; modes: typeof runtimeMultiplayer !== "undefined" ? runtimeMultiplayer.gameModes[root.choice.id] || [] : [] }
        }
        Text { width: parent.width; text: root.choice.title || "A world of adventures"; color: Theme.textColor("#fff4ce"); font.family: Theme.displayFamily; font.pixelSize: root.compact ? 24 : 29; wrapMode: Text.WordWrap; maximumLineCount: root.compact && root.height < 110 ? 1 : 2; elide: Text.ElideRight }
        Text { width: parent.width; visible: !root.choice.id || (!root.compact && text.length > 0); text: root.choice.id ? root.choice.time || "" : "Your games, your friends, your stories.\nChoose a game and make yourself at home."; color: Theme.textColor("#e1ebd4"); font.pixelSize: 14*root.unit; wrapMode: Text.WordWrap }
        Text { id: summary; objectName: "home-summary"; width: parent.width; visible: !!root.choice.id && text.length > 0; text: root.choice.description || ""; color: Theme.textColor("#d2e1cc"); font.pixelSize: 13*root.unit; wrapMode: Text.WordWrap; lineHeightMode: Text.FixedHeight; lineHeight: 16; maximumLineCount: Math.max(1,Math.min(root.compact ? 5 : 6,1+Math.floor((root.height-parent.y-y-summaryMetrics.height)/16))); elide: Text.ElideRight }
    }
}
