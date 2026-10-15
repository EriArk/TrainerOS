import QtQuick
Item {
    id: root
    required property var shell
    readonly property var choice: shell.homeGame
    clip: true
    Image { anchors.fill: parent; source: root.choice.preview || "qrc:/series/multiverse.png"; asynchronous: true; fillMode: Image.PreserveAspectCrop; opacity: .22 }
    Column {
        x: 16; y: 18; width: parent.width-32; spacing: 13
        Text { width: parent.width; text: root.choice.title || "Your next adventure"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 30; wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight }
        Text { width: parent.width; text: root.choice.system || "Choose a game from your library"; color: Theme.muted; font.pixelSize: 17; wrapMode: Text.WordWrap }
        Text { width: parent.width; text: root.choice.time || "No recorded play time"; color: Theme.ink; font.pixelSize: 16; wrapMode: Text.WordWrap }
        Text { width: parent.width; text: root.choice.description || ""; color: Theme.muted; font.pixelSize: 14; wrapMode: Text.WordWrap; maximumLineCount: 5; elide: Text.ElideRight }
    }
}
