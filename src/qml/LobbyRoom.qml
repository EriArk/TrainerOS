import QtQuick

Item {
    id: root
    required property var social
    required property bool takesFocus
    signal menuAnchored(var item)
    readonly property var room: social.lobbyState
    readonly property var game: social.lobbyRoomGame
    readonly property var players: social.lobbyPlayers
    readonly property var spectators: social.lobbySpectators
    readonly property bool expanded: social.lobbyChatExpanded
    readonly property bool focused: takesFocus && !social.lobbyChatFocused
    readonly property real margin: 18
    readonly property bool compact: height < 340
    objectName: "lobby-room"
    function selected(key) { return focused && social.lobbyRoomFocus === key }
    function indexOf(rows,key) { for(let i=0;i<rows.length;i++)if(rows[i].key===key)return i;return -1 }
    Item {
        id: gathering; visible: !root.expanded; x: root.margin; y: 10
        width: root.width*.655-root.margin*2; height: root.height-22
        Text { id: title; width: parent.width-37; height: 30; text: root.room.title || "Our lobby"
            font.family: Theme.displayFamily; font.pixelSize: 24; color: Theme.ink; elide: Text.ElideRight; textFormat: Text.PlainText }
        SocialIconButton { objectName: "lobby-room-menu"; anchors.right: parent.right; y: 0; icon: "dots-three"; label: "Room options"
            onClicked: { root.menuAnchored(this); root.social.lobbyAction("room-menu") } }
        Item {
            id: showcase; y: root.compact ? 30 : 36; width: parent.width; height: root.compact ? 50 : 64
            objectName: "lobby-room-game"
            Rectangle { anchors.fill: parent; radius: 10; color: Theme.rim; opacity: .09 }
            Rectangle { anchors.fill: parent; radius: 10; color: "transparent"; border.width: root.selected("game") ? 1 : 0; border.color: Theme.focusGlow }
            Item { id: cover; x: 10; y: 8; height: parent.height-16; width: height*.70; rotation: -5
                Rectangle { x: 3; y: 4; width: parent.width; height: parent.height; radius: 5; color: "#30193f3b" }
                Rectangle { anchors.fill: parent; radius: 5; color: root.game.id ? Theme.blue : Theme.yellow; border.color: Theme.paper; border.width: 3 }
                Image { anchors.fill: parent; anchors.margins: 3; source: root.game.cover || ""; asynchronous: true; fillMode: Image.PreserveAspectFit }
                MenuGlyph { anchors.centerIn: parent; width: 29; height: 29; glyph: "game-controller"; visible: !root.game.cover }
            }
            Column { x: cover.x+cover.width+14; y: root.compact ? 3 : root.game.id ? 5 : 11; width: parent.width-x-14; spacing: root.compact ? 1 : 2
                Text { width: parent.width; text: root.game.title || "Choosing Together"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: root.compact ? 17 : 20; elide: Text.ElideRight; textFormat: Text.PlainText }
                Text { width: parent.width; visible: !!root.game.id; text: (root.game.platform || "")+"  ·  "+root.game.modeLabel
                    color: Theme.muted; font.pixelSize: root.compact ? 9 : 11; elide: Text.ElideRight; textFormat: Text.PlainText }
                Text { width: parent.width; text: root.game.id ? root.game.status : "Choose a game ›"
                    color: Theme.muted; font.pixelSize: root.compact ? 9 : 10; elide: Text.ElideRight; textFormat: Text.PlainText }
            }
            MouseArea { anchors.fill: parent; onClicked: root.social.activateLobbyRoom("game") }
        }
        Text { id: time; y: showcase.y+showcase.height+4; width: parent.width; height: visible ? (root.compact ? 12 : 16) : 0
            visible: !!root.room.startsAt || !!root.room.connection
            text: root.room.connection || ("Gathering · "+Qt.formatDateTime(new Date((root.room.startsAt || 0)*1000),"ddd d MMM, HH:mm"))
            font.pixelSize: root.compact ? 9 : 11; color: Theme.muted; elide: Text.ElideRight; textFormat: Text.PlainText }
        Item { id: stage; objectName: "lobby-player-stage"; x: 0; y: time.y+time.height+5; width: parent.width
            height: Math.max(80,(disclosure.visible ? disclosure.y : actions.y)-y-9)
            ListView { id: seats; anchors.centerIn: parent; width: parent.width-16; height: Math.min(parent.height,170); orientation: ListView.Horizontal; spacing: 8; clip: true
                model: root.players; currentIndex: root.indexOf(root.players,root.social.lobbyRoomFocus)
                readonly property real cellWidth: Math.min(130,(width-24)/Math.min(4,Math.max(2,count)))
                readonly property real sideSpace: Math.max(0,(width-count*cellWidth-Math.max(0,count-1)*spacing)/2)
                header: Item { width: seats.sideSpace; height: 1 }
                footer: Item { width: seats.sideSpace; height: 1 }
                onCurrentIndexChanged: if(currentIndex>=0)positionViewAtIndex(currentIndex,ListView.Contain)
                onModelChanged: Qt.callLater(function(){if(seats.currentIndex>=0)seats.positionViewAtIndex(seats.currentIndex,ListView.Contain)})
                delegate: LobbyAvatar { required property var modelData; required property int index
                    objectName: "lobby-player-"+index; width: seats.cellWidth; height: seats.height; person: modelData; compact: root.compact
                    accent: [Theme.blue,Theme.pink,Theme.green,Theme.yellow][index%4]; selected: root.selected(modelData.key)
                    onActivated: root.social.activateLobbyRoom(modelData.key) }
            }
            Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: "‹"; color: Theme.muted; visible: !seats.atXBeginning; font.pixelSize: 22 }
            Text { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: "›"; color: Theme.muted; visible: !seats.atXEnd; font.pixelSize: 22 }
        }
        Text { id: disclosure; objectName: "lobby-progress-notice"; width: parent.width; anchors.bottom: actions.top; anchors.bottomMargin: 8; height: visible ? implicitHeight : 0
            readonly property var profile: root.game.runtime || ({})
            visible: !!profile.privateProgress || !!profile.temporaryProgress
            text: profile.temporaryProgress ? "This session starts fresh. Progress isn't kept." : "Save copies are shared with players. Only your own progress returns."
            font.pixelSize: 10; color: Theme.muted; wrapMode: Text.WordWrap; textFormat: Text.PlainText }
        Row { id: actions; anchors.bottom: parent.bottom; anchors.bottomMargin: root.compact ? 12 : 0; anchors.right: parent.right; height: 34; spacing: 9
            Repeater { model: root.social.lobbyRoomActions
                Item { required property var modelData; required property int index
                    readonly property bool voiceControl: modelData.command==="microphone" || modelData.command==="audio"
                    objectName: "lobby-room-action-"+modelData.key
                    width: voiceControl ? 38 : 132; height: 34; enabled: modelData.enabled
                    CapButton { anchors.fill: parent; textSize: 13; centered: true; claimsFocus: false; label: parent.voiceControl ? "" : modelData.label
                        tint: modelData.command==="ready" ? Theme.yellow : Theme.green; selected: root.selected(modelData.key)
                        onActivated: root.social.activateLobbyRoom(modelData.key) }
                    MenuGlyph { anchors.centerIn: parent; width: 21; height: 21; visible: parent.voiceControl
                        glyph: modelData.command==="microphone" ? "microphone" : "speaker" }
                    Rectangle { visible: parent.voiceControl && modelData.label.indexOf("off")>=0; anchors.centerIn: parent; width: 27; height: 2; rotation: -45; color: "#9a444f" }
                    Accessible.role: Accessible.Button; Accessible.name: modelData.label
                    Accessible.onPressAction: root.social.activateLobbyRoom(modelData.key)
                }
            }
        }
    }
    Rectangle { visible: !root.expanded; x: root.width*.655; y: 18; width: 1; height: root.height-45; color: Theme.rim; opacity: .40 }
    Item { id: chat; x: root.expanded ? root.margin : root.width*.655+root.margin; y: 10; width: root.width-x-root.margin; height: root.height-32
        Text { text: root.social.lobbySpectatorChat ? "Spectator chat" : "Room chat"; font.family: Theme.displayFamily; font.pixelSize: 20; color: Theme.ink }
        Item { objectName: "lobby-expand-chat"; anchors.right: parent.right; width: 32; height: 32
            enabled: root.social.lobbyChatVisible; opacity: enabled ? 1 : .35
            MenuGlyph { anchors.centerIn: parent; width: 20; height: 20; glyph: root.expanded ? "collapse" : "expand" }
            Accessible.role: Accessible.Button; Accessible.name: root.expanded ? "Back to room" : "Expand chat"
            Accessible.onPressAction: root.social.expandLobbyChat()
            MouseArea { anchors.fill: parent; onClicked: root.social.expandLobbyChat() } }
        Rectangle { x: -7; y: 33; width: parent.width+14; height: parent.height-31; radius: 9; color: "transparent"
            border.width: root.takesFocus && root.social.lobbyChatFocused ? 1 : 0; border.color: Theme.focusGlow }
        ConversationPane { x: 0; y: 37; width: parent.width; height: spectatorStrip.y-y-4
            visible: root.social.lobbyChatVisible; active: root.visible && visible; embeddedRoom: true
            filteredFeed: true; readOnlyChat: root.social.lobbySpectatorChat; streamChat: readOnlyChat; messageRows: root.social.lobbyRoomMessages
            social: root.social; onMenuAnchored: item => root.menuAnchored(item); onFocusRequested: root.social.focusLobbyChat() }
        Item { id: spectatorStrip; objectName: "lobby-spectator-strip"; width: parent.width; height: 24; anchors.bottom: parent.bottom
            Rectangle { width: parent.width; height: 1; color: Theme.rim; opacity: .45 }
            Text { x: 3; anchors.verticalCenter: parent.verticalCenter; text: "Spectators · "+root.spectators.length; color: Theme.muted; font.pixelSize: 11 }
            Text { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: root.social.lobbySpectatorChat ? "‹ Players" : "Messages ›"; color: Theme.muted; font.pixelSize: 11 }
            Accessible.role: Accessible.Button; Accessible.name: "Spectators · "+root.spectators.length
            Accessible.onPressAction: root.social.toggleLobbySpectatorChat()
            MouseArea { anchors.fill: parent; enabled: root.social.lobbyChatVisible; onClicked: root.social.toggleLobbySpectatorChat() }
        }
        Column { x: 10; width: parent.width-20; anchors.verticalCenter: parent.verticalCenter; visible: !root.social.lobbyChatVisible; spacing: 16
            MenuGlyph { anchors.horizontalCenter: parent.horizontalCenter; width: 42; height: 42; glyph: "chat"; ink: Theme.rim }
            Text { width: parent.width; text: root.social.lobbyChatStatus || "Invite a friend to start the conversation."
                font.pixelSize: 14; color: Theme.muted; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; textFormat: Text.PlainText }
            CapButton { anchors.horizontalCenter: parent.horizontalCenter; width: Math.min(170,parent.width); height: 30; textSize: 12; claimsFocus: false; label: "Retry conversation"
                visible: root.social.lobbyChatStatus.length>0; onActivated: root.social.lobbyAction("chat-retry") }
        }
    }
}
