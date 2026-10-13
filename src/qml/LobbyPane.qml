import QtQuick

Item {
    id: root
    required property var social
    required property bool takesFocus
    readonly property var selected: social.lobbyRows[social.lobbyFocus] || ({})
    readonly property bool own: selected.kind === "own"
    readonly property var room: own ? social.lobbyState : selected
    readonly property var game: room.game || ({})
    readonly property var members: own ? (room.members || []) : []
    objectName: "social-lobby"
    Rectangle { anchors.fill: parent; color: "#fffbf0" }
    Row {
        id: toolbar; x: 16; y: 8; spacing: 8; width: parent.width - 32
        CapButton { width: parent.width - 384; height: 33; textSize: 12; claimsFocus: false; tint: Theme.blue
            label: root.social.lobbySearch.length ? root.social.lobbySearch : "X · Find a lobby"
            onActivated: root.social.lobbyAction("search") }
        CapButton { width: 144; height: 33; textSize: 12; claimsFocus: false; tint: Theme.pink
            label: root.social.lobbyTopic.length ? root.social.lobbyTopic : "All topics"
            onActivated: root.social.lobbyAction("topic") }
        CapButton { width: 124; height: 33; textSize: 12; claimsFocus: false; tint: root.social.lobbyFreeOnly ? Theme.green : Theme.yellow
            label: root.social.lobbyFreeOnly ? "Free seats only" : "All lobbies"
            onActivated: root.social.lobbyAction("free") }
        CapButton { width: 92; height: 33; textSize: 12; claimsFocus: false; label: "Y · Refresh"; tint: Theme.green
            enabled: !root.social.lobbyDirectory.busy; onActivated: root.social.lobbyAction("refresh") }
    }
    Item {
        id: catalogue; x: 16; y: 55; width: parent.width * 0.43 - 24; height: parent.height - 83
        CapButton {
            id: mine; width: parent.width; height: 55; textSize: 16; claimsFocus: false; tint: Theme.green
            label: (root.social.lobbyRows[0] || {}).title || "Create lobby"
            detail: (root.social.lobbyRows[0] || {}).detail || ""
            selected: root.takesFocus && !root.social.lobbyDetailFocused && root.social.lobbyFocus === 0
            onActivated: root.social.selectLobby(0)
        }
        ListView {
            id: rooms; x: -4; y: 69; width: parent.width + 8; height: parent.height - y; clip: true; spacing: 10
            model: root.social.lobbyRows.slice(1); currentIndex: root.social.lobbyFocus - 1
            onCurrentIndexChanged: if(currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)
            delegate: CapButton {
                required property var modelData; required property int index
                x: 4; width: rooms.width - 8; height: 59; textSize: 14; claimsFocus: false
                label: modelData.title || ""; detail: modelData.detail || ""
                tint: modelData.kind === "invitation" ? Theme.yellow : index % 2 ? Theme.pink : Theme.blue
                selected: root.takesFocus && !root.social.lobbyDetailFocused && root.social.lobbyFocus === index + 1
                onActivated: root.social.selectLobby(index + 1)
            }
        }
    }
    Rectangle {
        id: detail; x: parent.width * 0.43 + 8; y: 55; width: parent.width - x - 16; height: parent.height - 83
        radius: 13; color: "#edf3de"; border.color: "#b8c6a6"
        Text { id: title; x: 16; y: 12; width: parent.width - 32; height: 31; text: root.selected.title || "Lobby"
            color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 23; textFormat: Text.PlainText; elide: Text.ElideRight }
        Text { id: subtitle; x: 16; y: 47; width: parent.width - 32; height: 20; elide: Text.ElideRight
            text: root.own ? (root.room.phase === "joining" ? "Waiting for the host…" : root.game.title || "Choose your next game together") : root.selected.detail || "Invite friends, then choose a game together."
            font.pixelSize: 13; color: Theme.muted; textFormat: Text.PlainText }
        ListView {
            id: people; x: 16; y: 75; width: parent.width - 32
            height: Math.max(0, actions.y - y - 5); model: root.members; clip: true; spacing: 5
            delegate: Rectangle {
                required property var modelData; required property int index
                width: people.width; height: 26; radius: 7; color: index % 2 ? "#f5e4ee" : "#e0edf4"
                Text { x: 10; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 116
                    text: modelData.name || "Player"; color: Theme.ink; font.pixelSize: 13; elide: Text.ElideRight; textFormat: Text.PlainText }
                Text { anchors { right: parent.right; rightMargin: 9; verticalCenter: parent.verticalCenter }
                    text: !modelData.connected ? "Reconnecting" : modelData.ready ? "Ready" : modelData.spectator ? "Watching" : modelData.seat ? "P" + modelData.seat : "Choose a seat"
                    font.pixelSize: 11; color: modelData.ready ? "#386b39" : Theme.muted; textFormat: Text.PlainText }
            }
        }
        Grid {
            id: actions; x: 16; width: parent.width - 32; anchors.bottom: parent.bottom; anchors.bottomMargin: 12; spacing: 6; columns: 2
            Repeater {
                model: root.social.lobbyActions
                CapButton {
                    required property var modelData; required property int index
                    width: (actions.width - 6) / 2; height: 30; textSize: 12; claimsFocus: false
                    label: modelData.label; enabled: modelData.enabled; tint: modelData.id === "leave" ? Theme.pink : Theme.yellow
                    selected: root.takesFocus && root.social.lobbyDetailFocused && root.social.lobbyActionFocus === index
                    onActivated: root.social.lobbyAction(modelData.id)
                }
            }
        }
    }
    Text { x: 16; anchors.bottom: parent.bottom; anchors.bottomMargin: 5; width: parent.width - 32; height: 18
        text: root.social.lobbyDirectory.status || "A · Open     B · Back     Select · Filters"
        font.pixelSize: 11; color: Theme.muted; elide: Text.ElideRight; textFormat: Text.PlainText }
}
