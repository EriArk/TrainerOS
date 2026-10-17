import QtQuick

Item {
    id: root
    required property var social
    required property bool takesFocus
    readonly property var rows: social.lobbyRows.slice(1)
    readonly property var categories: ["", "racing", "fighting", "adventure", "party-games", "competitive", "other"]
    readonly property var categoryNames: ["All rooms", "Racing", "Fighting", "Adventure", "Party Games", "Competitive", "Other games"]
    readonly property var accents: [Theme.blue, Theme.yellow, Theme.pink, Theme.tabColors[4], Theme.pink, Theme.yellow, Theme.blue]
    objectName: "lobby-catalogue"
    function timeLabel(seconds) {
        if(!seconds)return ""
        const date=new Date(seconds*1000), today=new Date()
        return Qt.formatDateTime(date, date.toDateString()===today.toDateString() ? "'Today' HH:mm" : "ddd d MMM, HH:mm")
    }
    Item {
        id: rail; x: 16; y: 10; width: Math.min(174,root.width*.22); height: parent.height-22
        Text { text: "Find your people"; font.family: Theme.displayFamily; font.pixelSize: 19; color: Theme.ink }
        CapButton {
            y: 34; width: parent.width-10; height: 30; textSize: 12; claimsFocus: false; tint: Theme.blue
            label: root.social.lobbySearch || "Search rooms…"
            onActivated: root.social.lobbyAction("search")
        }
        Column {
            y: 72; width: parent.width-9; spacing: 2
            Repeater {
                model: root.categories
                delegate: Item {
                    required property string modelData; required property int index
                    width: rail.width-9; height: Math.max(16,Math.min(29,(rail.height-150)/root.categories.length))
                    Rectangle { anchors.fill: parent; radius: 8; color: root.accents[index]; opacity: root.social.lobbyTopic===modelData ? .30 : 0 }
                    Rectangle { x: 7; anchors.verticalCenter: parent.verticalCenter; width: 6; height: 6; radius: 3; color: root.accents[index]; border.color: Qt.darker(color,1.18) }
                    Text { x: 22; anchors.verticalCenter: parent.verticalCenter; width: parent.width-28; text: root.categoryNames[index]
                        font.family: Theme.displayFamily; font.pixelSize: parent.height<23 ? 11 : 13; color: Theme.ink; elide: Text.ElideRight }
                    MouseArea { anchors.fill: parent; onClicked: root.social.lobbyAction("category:"+modelData) }
                }
            }
        }
        Rectangle { x: 4; y: parent.height-65; width: parent.width-20; height: 1; color: Theme.muted; opacity: .18 }
        Text {
            x: 5; y: parent.height-53; width: parent.width-14
            text: (root.social.lobbyFreeOnly ? "✓  " : "○  ")+"Free seats"; color: Theme.ink; font.pixelSize: 12
            MouseArea { anchors.fill: parent; anchors.margins: -6; onClicked: root.social.lobbyAction("free") }
        }
        Text {
            x: 5; y: parent.height-25; width: parent.width-14; text: "Filters  ·  Select  ›"; font.pixelSize: 12; color: Theme.muted
            MouseArea { anchors.fill: parent; anchors.margins: -6; onClicked: root.social.lobbyAction("filters") }
        }
    }
    Rectangle { x: rail.x+rail.width+3; y: 17; width: 1; height: parent.height-38; color: Theme.muted; opacity: .18 }
    Item {
        id: field; x: rail.x+rail.width+19; y: 8; width: parent.width-x-16; height: parent.height-12
        Text { x: 3; y: 3; text: "Online lobbies"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 22 }
        Text { x: 4; y: 31; width: parent.width-225; text: root.social.lobbyPlatform || root.social.lobbyMode ? [root.social.lobbyPlatform,root.social.lobbyMode].filter(Boolean).join(" · ") : "A place to start something together"
            color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight }
        CapButton {
            anchors.right: parent.right; y: 3; width: Math.min(166,parent.width*.29); height: 34; claimsFocus: false; textSize: 13; tint: Theme.green
            label: root.social.lobbyState.id ? "My lobby  ›" : "+  Create lobby"
            selected: root.takesFocus && root.social.lobbyFocus===0
            onActivated: root.social.selectLobby(0)
        }
        Grid {
            id: cards; x: 0; y: 59; width: parent.width; height: parent.height-y-30; columns: 2; columnSpacing: 12; rowSpacing: 12
            Repeater {
                model: root.rows.slice(root.social.lobbyPage*6,root.social.lobbyPage*6+6)
                delegate: Item {
                    id: card
                    required property var modelData
                    required property int index
                    readonly property int rowIndex: root.social.lobbyPage*6+index+1
                    readonly property bool focused: root.takesFocus && root.social.lobbyFocus===rowIndex
                    readonly property color accent: root.accents[Math.max(0,root.categories.indexOf(modelData.topic || ""))]
                    readonly property var game: modelData.game || ({})
                    readonly property bool compact: height<86
                    width: (cards.width-12)/2; height: (cards.height-24)/3
                    objectName: "lobby-card-"+index
                    Rectangle { x: 1; y: 3; width: parent.width-2; height: parent.height; radius: 13; color: "#25243e38" }
                    Rectangle {
                        anchors.fill: parent; radius: 13; border.width: card.focused ? 2 : 1; border.color: card.focused ? Theme.yellow : Qt.darker(card.accent,1.12)
                        gradient: Gradient { GradientStop { position: 0; color: Qt.tint(Theme.paper,Qt.rgba(card.accent.r,card.accent.g,card.accent.b,.24)) }
                            GradientStop { position: 1; color: Qt.tint(Theme.paper,Qt.rgba(card.accent.r,card.accent.g,card.accent.b,.49)) } }
                        Rectangle { anchors.fill: parent; anchors.margins: 3; radius: 10; color: "transparent"; border.color: "#b3ffffff"; border.width: 1 }
                    }
                    Item {
                        id: host; x: 7; width: Math.min(70,card.width*.24); height: parent.height-(card.compact ? 8 : 14); anchors.verticalCenter: parent.verticalCenter
                        AccountAvatar {
                            id: portrait; anchors.horizontalCenter: parent.horizontalCenter; y: Math.max(0,(parent.height-height-19)/2); width: Math.min(48,parent.height-18)
                            name: card.modelData.name || (card.modelData.kind==="community" ? "+" : "?")
                            source: card.modelData.avatar ? "https://fluxerusercontent.com/avatars/"+card.modelData.owner+"/"+card.modelData.avatar+".png?size=64" : ""
                        }
                        Text { y: portrait.y+portrait.height+4; width: parent.width; height: 14; text: card.modelData.name || "TrainerOS"; color: Theme.muted; font.pixelSize: card.compact ? 8 : 10
                            horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight; textFormat: Text.PlainText }
                    }
                    Column {
                        x: host.x+host.width+8; y: card.compact ? 5 : 10; width: parent.width-x-12; spacing: card.compact ? 1 : 4
                        Text { width: parent.width; text: card.modelData.title || "Lobby"; font.family: Theme.displayFamily; font.pixelSize: card.compact ? 13 : 17; color: Theme.ink
                            textFormat: Text.PlainText; elide: Text.ElideRight }
                        Text { width: parent.width; text: card.game.title || (card.modelData.kind==="community" ? "Browse public rooms" : "Choosing Together")
                            color: Theme.muted; font.pixelSize: card.compact ? 10 : 12; textFormat: Text.PlainText; elide: Text.ElideRight }
                    }
                    Row {
                        x: host.x+host.width+8; anchors.bottom: parent.bottom; anchors.bottomMargin: card.compact ? 5 : 11; spacing: 8
                        Text { text: (card.modelData.seats ? (card.modelData.seats-card.modelData.available)+" / "+card.modelData.seats+(card.compact ? "" : " seats") : (card.modelData.members ? card.modelData.members+" together" : "Game not chosen"))+(card.compact && card.modelData.startsAt ? " · "+root.timeLabel(card.modelData.startsAt) : "")
                            font.pixelSize: card.compact ? 9 : 11; color: Theme.ink }
                        Text { text: card.modelData.access==="approval" ? "Approval" : card.modelData.kind==="invitation" ? "Invited" : "Open"
                            font.pixelSize: card.compact ? 9 : 10; color: Theme.muted; visible: !card.compact || !card.modelData.startsAt }
                    }
                    Text { x: host.x+host.width+8; anchors.bottom: parent.bottom; anchors.bottomMargin: 26; width: parent.width-x-10
                        visible: !!card.modelData.startsAt && !card.compact; text: root.timeLabel(card.modelData.startsAt); font.pixelSize: 10; color: Theme.muted; elide: Text.ElideRight }
                    MouseArea { anchors.fill: parent; onClicked: root.social.selectLobby(card.rowIndex) }
                }
            }
        }
        Column {
            visible: root.rows.length===0; anchors.centerIn: cards; width: cards.width*.75; spacing: 13
            Row { anchors.horizontalCenter: parent.horizontalCenter; spacing: 12
                Repeater { model: [Theme.blue,Theme.pink,Theme.green]
                    Rectangle { required property color modelData; required property int index; width: 46; height: 46; radius: 23; color: modelData; rotation: index===1 ? 9 : -8
                        Text { anchors.centerIn: parent; text: ["+","♡","✦"][index]; font.pixelSize: 24; color: Theme.ink } }
                }
            }
            Text { width: parent.width; text: root.social.lobbyDirectory.busy ? "Looking for your next party…" : "Room for a new adventure"
                font.family: Theme.displayFamily; font.pixelSize: 23; color: Theme.ink; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap }
            Text { width: parent.width; text: root.social.lobbyDirectory.needsJoin ? root.social.lobbyDirectory.status : root.social.lobbySearch || root.social.lobbyTopic || root.social.lobbyFreeOnly || root.social.lobbyPlatform || root.social.lobbyMode ? "Try another category or clear your filters." : "Create a lobby and invite your friends. You can choose a game together."
                font.pixelSize: 13; color: Theme.muted; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap }
        }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; spacing: 17; height: 24
            Text { width: 24; text: "‹"; font.pixelSize: 25; color: Theme.ink; opacity: root.social.lobbyPage>0 ? 1 : .2
                MouseArea { anchors.fill: parent; enabled: root.social.lobbyPage>0; onClicked: root.social.turnLobbyPage(-1) } }
            Text { y: 5; text: (root.social.lobbyPage+1)+" / "+root.social.lobbyPageCount; color: Theme.muted; font.pixelSize: 11 }
            Text { width: 24; text: "›"; font.pixelSize: 25; color: Theme.ink; opacity: root.social.lobbyPage+1<root.social.lobbyPageCount ? 1 : .2
                MouseArea { anchors.fill: parent; enabled: root.social.lobbyPage+1<root.social.lobbyPageCount; onClicked: root.social.turnLobbyPage(1) } }
        }
        Text { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.bottomMargin: 4; text: root.social.lobbyDirectory.busy ? "Refreshing…" : "↻ Refresh"; color: Theme.muted; font.pixelSize: 11
            MouseArea { anchors.fill: parent; anchors.margins: -5; enabled: !root.social.lobbyDirectory.busy; onClicked: root.social.lobbyAction("refresh") } }
    }
}
