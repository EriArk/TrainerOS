import QtQuick
Item {
    id: root
    required property var shell
    readonly property var choice: shell.homeGame
    readonly property real unit: 1
    readonly property bool compact: height < 180
    clip: true
    Column {
        x: 5*root.unit; y: 8*root.unit; width: parent.width-10*root.unit; spacing: root.compact ? 6 : 8
        Text { width: parent.width; visible: root.height >= 95; text: root.choice.system || "WELCOME HOME"; color: "#d4e6c5"; font.pixelSize: 12*root.unit; font.letterSpacing: 1; elide: Text.ElideRight }
        Text { width: parent.width; text: root.choice.title || "A world of adventures"; color: "#fff4ce"; font.family: Theme.displayFamily; font.pixelSize: root.compact ? 24 : 29; wrapMode: Text.WordWrap; maximumLineCount: root.compact && root.height < 110 ? 1 : 2; elide: Text.ElideRight }
        Text { width: parent.width; visible: !root.choice.id || (!root.compact && text.length > 0); text: root.choice.id ? root.choice.time || "" : "Your games, your friends, your stories.\nChoose a game and make yourself at home."; color: "#e1ebd4"; font.pixelSize: 14*root.unit; wrapMode: Text.WordWrap }
        Text { objectName: "home-summary"; width: parent.width; visible: !!root.choice.id && text.length > 0; text: root.choice.description || ""; color: "#d2e1cc"; font.pixelSize: 13*root.unit; wrapMode: Text.WordWrap; lineHeightMode: Text.FixedHeight; lineHeight: 16; maximumLineCount: Math.max(1,Math.min(root.compact ? 2 : 3,Math.floor((root.height-parent.y-y)/16))); elide: Text.ElideRight }
    }
}
