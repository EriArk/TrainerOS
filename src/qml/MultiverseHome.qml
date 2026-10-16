import QtQuick
Item {
    id: root
    required property var shell
    readonly property var choice: shell.homeGame
    readonly property real unit: Math.max(1, Math.min(1.65, width/410, height/250))
    clip: true
    Column {
        x: 5*root.unit; y: 8*root.unit; width: parent.width-10*root.unit; spacing: 12*root.unit
        Text { width: parent.width; text: root.choice.system || "WELCOME HOME"; color: "#d4e6c5"; font.pixelSize: 12*root.unit; font.letterSpacing: 1; elide: Text.ElideRight }
        Text { width: parent.width; text: root.choice.title || "A world of adventures"; color: "#fff4ce"; font.family: Theme.displayFamily; font.pixelSize: 29*root.unit; wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight }
        Text { width: parent.width; text: root.choice.id ? root.choice.time || "" : "Your games, your friends, your stories.\nChoose a game and make yourself at home."; color: "#e1ebd4"; font.pixelSize: 14*root.unit; wrapMode: Text.WordWrap }
        Text { width: parent.width; visible: !!root.choice.id && text.length > 0; text: root.choice.description || ""; color: "#d2e1cc"; font.pixelSize: 13*root.unit; wrapMode: Text.WordWrap; maximumLineCount: 4; elide: Text.ElideRight }
    }
}
