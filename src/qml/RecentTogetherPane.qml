import QtQuick

Item {
    id: root
    objectName: "social-recent-together"
    required property var social
    property bool takesFocus: false
    readonly property var selected: social.recentPeople[social.recentFocus] || ({})
    readonly property bool online: social.account.state === "connected"
    Rectangle { anchors.fill: parent; color: "#fffaf0" }
    Rectangle { width: people.width + 24; height: parent.height; color: "#e2ece3" }
    ListView {
        id: people
        x: 12; y: 12; width: parent.width * 0.46 - 24; height: parent.height - 24
        clip: true; spacing: 5; model: root.social.recentPeople; currentIndex: root.social.recentFocus
        onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)
        delegate: Rectangle {
            required property var modelData
            required property int index
            width: people.width; height: 62; radius: 9
            color: index === people.currentIndex ? "#fff0b8" : "#f5f8ed"
            border.color: index === people.currentIndex ? "#b79747" : "#c4d2c5"
            border.width: root.takesFocus && index === people.currentIndex ? 2 : 1
            Rectangle {
                x: 9; y: 12; width: 36; height: 36; radius: 12
                color: [Theme.pink, Theme.blue, Theme.green][index % 3]
                Text { anchors.centerIn: parent; text: (modelData.name || "?").slice(0, 1).toUpperCase(); color: Theme.ink; font.pixelSize: 20; textFormat: Text.PlainText }
            }
            Column {
                x: 55; y: 10; width: parent.width - 65; spacing: 4
                Text { width: parent.width; text: modelData.name || "Player"; color: Theme.ink; font.pixelSize: 16; font.bold: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                Text { width: parent.width; text: Qt.formatDateTime(new Date(modelData.last * 1000), "d MMM yyyy · hh:mm"); color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight }
            }
            MouseArea { anchors.fill: parent; onClicked: root.social.selectRecent(index) }
        }
    }
    Column {
        x: parent.width * 0.46 + 20; width: parent.width * 0.54 - 40
        anchors.verticalCenter: parent.verticalCenter; spacing: 13
        Text { width: parent.width; text: people.count ? root.selected.name || "Player" : "Meet here again"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 26; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight; textFormat: Text.PlainText }
        Text { width: parent.width; text: people.count ? "People from your shared lobbies, saved on this device." : "People you join in a lobby will appear here. You can add them as friends later."; color: Theme.muted; font.pixelSize: 14; wrapMode: Text.Wrap }
        CapButton { visible: people.count > 0; width: parent.width; height: 40; label: "View profile"; tint: Theme.blue; claimsFocus: false; enabled: root.online; onActivated: root.social.recentAction("profile") }
        CapButton { objectName: "recent-friend-action"; visible: people.count > 0; width: parent.width; height: 40; label: root.selected.friendLabel || "Add friend"; tint: Theme.yellow; claimsFocus: false; enabled: !!root.selected.canAdd; onActivated: root.social.recentAction("friend") }
        Text { visible: !!root.selected.contactStatus; width: parent.width; text: root.selected.contactStatus || ""; color: Theme.muted; font.pixelSize: 12; wrapMode: Text.Wrap; textFormat: Text.PlainText }
        CapButton { visible: people.count > 0; width: parent.width; height: 34; label: "Remove from this list"; tint: Theme.paper; claimsFocus: false; onActivated: root.social.recentAction("forget") }
        Text { visible: !root.online && people.count > 0; width: parent.width; text: "Connect to view profiles or add friends."; color: Theme.muted; font.pixelSize: 12; wrapMode: Text.Wrap }
    }
}
