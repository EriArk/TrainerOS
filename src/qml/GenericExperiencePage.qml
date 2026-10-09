import QtQuick

Item {
    id: root
    required property var shell
    readonly property var game: shell.homeGame
    readonly property bool history: shell.experienceView === "game-history"
    PageHeader {
        id: heading; width: parent.width
        title: root.history ? "Play history" : (root.game.title || "Your games")
        subtitle: root.history ? (root.game.title || "Your recorded sessions") : (root.game.system || "Choose a game in Collections")
    }
    MountedPanel {
        x: 0; y: heading.height + 12; width: parent.width; height: parent.height - y
        color: "#d9e5dd"
        visible: !root.history
        Rectangle {
            id: art; x: 20; y: 20; width: parent.width * 0.29; height: parent.height - 40
            radius: 12; color: "#bdcfd0"; clip: true
            Image { anchors.fill: parent; anchors.margins: 10; source: root.game.preview || ""; asynchronous: true; fillMode: Image.PreserveAspectFit }
            Text { anchors.centerIn: parent; visible: !root.game.preview; text: root.game.system || "Games"; color: Theme.ink; font.pixelSize: 23 }
        }
        Column {
            x: art.x + art.width + 24; y: 24; width: parent.width - x - 24; spacing: 14
            Text { width: parent.width; text: root.game.title || "Pick your next game"; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 27 }
            Text { width: parent.width; text: root.game.description || "Open game actions for information, artwork and your collections. Your recorded sessions are in History."; textFormat: Text.PlainText; wrapMode: Text.WordWrap; maximumLineCount: 5; elide: Text.ElideRight; color: Theme.muted; font.pixelSize: 17 }
            Text { text: root.game.time || "No recorded play time"; color: Theme.ink; font.pixelSize: 17 }
            CapButton { width: Math.min(parent.width,260); height: 45; label: root.game.id ? "Game actions" : "Open Collections"; selected: root.visible && !root.shell.serviceOpen && !root.shell.menuOpen; onActivated: root.shell.activate(0,"experience") }
        }
    }
    ListView {
        id: sessions
        x: 0; y: heading.height + 12; width: parent.width; height: parent.height - y
        visible: root.history; clip: true; spacing: 10
        model: root.shell.gameHistory
        currentIndex: root.shell.focusIndex
        onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
        delegate: MountedPanel {
            required property var modelData
            required property int index
            width: sessions.width; height: 74
            color: sessions.currentIndex === index ? "#f1e5b9" : "#d9e5dd"
            Text { x: 22; y: 13; width: parent.width - 200; text: modelData.time; color: Theme.ink; font.pixelSize: 19; elide: Text.ElideRight }
            Text { x: 22; y: 42; text: modelData.status; color: Theme.muted; font.pixelSize: 14 }
            Text { anchors.right: parent.right; anchors.rightMargin: 22; y: 25; text: modelData.duration; color: Theme.ink; font.pixelSize: 17 }
            MouseArea { anchors.fill: parent; onClicked: root.shell.activate(index,"experience-history") }
        }
        Text { anchors.centerIn: parent; visible: sessions.count === 0; text: "No recorded sessions yet"; color: Theme.muted; font.pixelSize: 22 }
    }
}
