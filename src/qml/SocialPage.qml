import QtQuick

FocusScope {
    id: root
    required property var shell
    readonly property int faceIndex: ["chats","communities","friends"].indexOf(shell.socialFace)
    readonly property var social: shell.social
    readonly property var account: social.account
    readonly property bool connected: account.state === "connected" || account.state === "connecting"
    readonly property bool takesFocus: visible && !shell.serviceOpen && !shell.menuOpen && !shell.drawerOpen && !shell.keyboard.open && shell.notice.length === 0
    property point menuAnchor: Qt.point(width - 310, 44)
    function anchorMenu(item) { menuAnchor = item.mapToItem(root, 0, item.height) }
    objectName: "social-empty"
    onTakesFocusChanged: if (takesFocus) forceActiveFocus()
    Component.onCompleted: if (takesFocus) forceActiveFocus()
    PageHeader {
        id: heading
        visible: !root.connected || root.faceIndex === 2
        height: visible ? implicitHeight : 0
        title: [root.social.contacts ? "Friends" : "Messages", "Communities", "Discover"][root.faceIndex]
        subtitle: root.connected ? (root.account.name || "") + " · " + (root.faceIndex === 1 && root.account.communityStatus ? root.account.communityStatus : root.account.status || "") : "A little closer, wherever you are."
        Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering;
            anchors { right: parent.right; rightMargin: Theme.pageMargin; bottom: parent.bottom }
            text: "Powered by Fluxer"; color: Theme.muted; font.pixelSize: 9
        }
    }
    Item {
        anchors { top: heading.bottom; bottom: parent.bottom; left: parent.left; right: parent.right }
        visible: !root.connected
        Rectangle { anchors.fill: parent; color: "#edf2e4" }
        Rectangle { width: parent.width * 0.36; height: parent.height; color: "#d3e5dd"; border.color: "#b0c6b9" }
        Item {
            x: 34; width: parent.width * 0.36 - 68; height: 194; anchors.verticalCenter: parent.verticalCenter
            Rectangle { x: 4; y: 7; width: parent.width; height: 150; radius: 24; color: "#6684947b" }
            Rectangle {
                width: parent.width; height: 150; radius: 24; color: Theme.blue; border.color: "#668f9f"; border.width: 2
                Rectangle { x: 7; y: 7; width: parent.width - 14; height: parent.height - 14; radius: 19; color: "#bde3ec"; border.color: "#e9f6f4"; border.width: 2 }
                Rectangle { x: 18; y: 22; width: parent.width - 36; height: 97; radius: 12; color: "#587d76" }
                Rectangle { x: 24; y: 28; width: parent.width - 48; height: 84; radius: 9; color: "#e6f2c9"; border.color: "#7ba490"; border.width: 2
                    Row { anchors.centerIn: parent; spacing: 21
                        Rectangle { width: 8; height: 15; radius: 4; color: Theme.ink }
                        Rectangle { width: 8; height: 15; radius: 4; color: Theme.ink }
                    }
                    Rectangle { anchors.horizontalCenter: parent.horizontalCenter; y: 58; width: 19; height: 3; radius: 2; color: Theme.ink }
                    Rectangle { x: 21; y: 50; width: 16; height: 7; radius: 4; color: Theme.pink }
                    Rectangle { anchors.right: parent.right; anchors.rightMargin: 21; y: 50; width: 16; height: 7; radius: 4; color: Theme.pink }
                }
                Row { x: 22; y: 129; spacing: 5; Repeater { model: 4; Rectangle { width: 16; height: 3; radius: 1; color: "#719ea5" } } }
                Rectangle { anchors.right: parent.right; anchors.rightMargin: 22; y: 126; width: 10; height: 10; radius: 5; color: Theme.yellow; border.color: "#927e3f" }
            }
            Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; y: 168; width: parent.width; text: "HELLO, FRIEND!"; horizontalAlignment: Text.AlignHCenter; font.family: Theme.brandFamily; font.pixelSize: 21; color: Theme.ink }
        }
        Column {
            x: parent.width * 0.36 + 32; width: parent.width * 0.64 - 64
            anchors.verticalCenter: parent.verticalCenter; spacing: 14
            Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; width: parent.width; text: root.account.state === "authorizing" ? "Let's get you connected" : "Your friends, along for the ride"; wrapMode: Text.WordWrap; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 26 }
            Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; width: parent.width; text: root.account.state === "authorizing" ? (root.account.status || "Connecting...") : "Chat with your friends from your handheld."; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: 16; textFormat: Text.PlainText }
            Rectangle { visible: (root.account.code || "").length > 0; width: parent.width; height: 60; radius: 10; color: "#fff4c6"; border.color: "#c5af66"; border.width: 2
                Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; anchors.centerIn: parent; text: root.account.code || ""; font.family: Theme.brandFamily; font.pixelSize: 27; color: Theme.ink; textFormat: Text.PlainText }
            }
            CapButton { visible: root.account.state !== "authorizing"; width: parent.width; height: 54; label: root.account.state === "restoring" ? "Signing in..." : "Sign in"; tint: Theme.yellow; selected: root.takesFocus; enabled: !!root.account.available && root.account.state !== "restoring"; claimsFocus: false; onActivated: root.social.login() }
            Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; width: parent.width; visible: root.account.state !== "authorizing" && !!root.account.status && root.account.status !== "Sign in"; text: root.account.status || ""; color: Theme.muted; font.pixelSize: 13; wrapMode: Text.WordWrap; textFormat: Text.PlainText }
        }
    }
    Item {
        id: conversationPage
        visible: root.connected && root.faceIndex !== 2
        anchors { top: heading.bottom; bottom: parent.bottom; left: parent.left; right: parent.right }
        Rectangle { anchors.fill: parent; color: "#fffbf0" }
        Rectangle { width: people.width + 20; height: parent.height; color: "#f4e4de" }
        Rectangle { x: people.width + 20; width: 1; height: parent.height; color: "#d8beb7" }
        Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; x: 16; y: 12; width: people.width - 72
            text: root.faceIndex === 1 ? "Communities" : root.social.contacts ? "Friends" : "Messages"
            font.family: Theme.displayFamily; font.pixelSize: 19; color: Theme.ink; elide: Text.ElideRight }
        SocialIconButton { x: people.width - 56; y: 5; tint: Theme.blue; icon: "users"; label: root.social.contacts ? "Messages" : "Friends and requests"
            visible: root.faceIndex === 0; onClicked: root.social.showFriends() }
        SocialIconButton { x: people.width - 22; y: 5; tint: Theme.yellow; icon: "plus"; label: "Create and manage"
            onClicked: { root.anchorMenu(this); root.social.collectionOptions() } }
        ListView {
            id: people
            x: 8; y: 45; width: parent.width * 0.28; height: parent.height - y - 45
            clip: true; spacing: 3; model: root.social.rows; currentIndex: root.social.focusIndex
            onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
            delegate: Rectangle {
                required property var modelData
                required property int index
                width: people.width; height: 49; radius: 8
                color: index === people.currentIndex ? "#fff0b8" : mouse.containsMouse ? "#faeee7" : "transparent"
                border.width: index === people.currentIndex && !root.social.reading && !root.social.menu.length ? 1 : 0
                border.color: "#c5a253"
                MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; onClicked: root.social.activate(index) }
                Rectangle { x: 7; y: 9; width: 30; height: 30; radius: 10; color: modelData.kind === "groups" ? Theme.tabColors[4] : Theme.blue
                    Image { id: avatarImage; anchors.fill: parent; anchors.margins: 1; source: root.visible ? (modelData.avatar || "") : ""; sourceSize: Qt.size(60,60); asynchronous: true; fillMode: Image.PreserveAspectFit; visible: status === Image.Ready }
                    Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; visible: avatarImage.status !== Image.Ready; anchors.centerIn: parent; text: modelData.name.substring(0,1).toUpperCase(); color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 18; textFormat: Text.PlainText }
                }
                Column { x: 46; y: 7; width: parent.width - x - 32; spacing: 3
                    Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; width: parent.width; text: modelData.name; color: Theme.ink; font.pixelSize: 14; font.bold: !!modelData.unread; elide: Text.ElideRight; textFormat: Text.PlainText }
                    Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; width: parent.width; text: (modelData.kind === "groups" ? "Group · " : "") + (modelData.detail || ""); color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight; textFormat: Text.PlainText }
                }
                SocialIconButton { anchors { right: parent.right; verticalCenter: parent.verticalCenter } width: 30; icon: "dots-three"; label: "Options for " + modelData.name
                    onClicked: { root.anchorMenu(this); root.social.contextRow(index) } }
            }
        }
        Rectangle { x: 8; y: parent.height - 41; width: people.width; height: 1; color: "#d8beb7" }
        SocialIconButton { x: 8; y: parent.height - 37; tint: Theme.tabColors[4]; icon: "user"; label: "My profile"; onClicked: root.social.profile() }
        Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; x: 44; y: parent.height - 34; width: people.width - 70; text: root.account.name || ""; font.pixelSize: 12; color: Theme.ink; elide: Text.ElideRight; textFormat: Text.PlainText
            MouseArea { anchors.fill: parent; onClicked: root.social.profile() } }
        Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; x: 44; y: parent.height - 18; text: "Powered by Fluxer"; font.pixelSize: 9; color: Theme.muted }
        Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; x: people.x + 8; y: 90; width: people.width - 16; visible: !people.count
            text: root.faceIndex === 1 ? "Your communities will appear here." : "Your conversations will appear here."
            wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; color: Theme.muted; font.pixelSize: 14 }
        Item {
            id: conversationPane
            anchors { left: people.right; leftMargin: 26; right: parent.right; rightMargin: 12; top: parent.top; bottom: parent.bottom }
            visible: root.social.conversation
            Rectangle { id: chatHeading; width: parent.width; height: 45; radius: 9; color: "#e6dbf0"
                Rectangle { x: 0; y: 8; width: 28; height: 28; radius: 9; color: Theme.pink
                    Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; anchors.centerIn: parent; text: root.social.conversationName.substring(0,1).toUpperCase(); font.pixelSize: 17; color: Theme.ink }
                    Image { anchors.fill: parent; source: root.social.conversationInfo.avatar || ""; sourceSize: Qt.size(56,56); asynchronous: true }
                }
                Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; x: 38; y: 7; width: parent.width - 112; text: root.social.conversationName; font.family: Theme.displayFamily; font.pixelSize: 18; color: Theme.ink; elide: Text.ElideRight; textFormat: Text.PlainText }
                Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; x: 38; y: 28; width: parent.width - 112; text: root.account.historyBusy ? "Loading messages..." : root.social.conversationInfo.kind === "groups" ? "Group conversation" : root.social.conversationInfo.detail || ""; font.pixelSize: 10; color: Theme.muted; elide: Text.ElideRight; textFormat: Text.PlainText }
                MouseArea { width: parent.width - 74; height: parent.height; onClicked: root.social.conversationProfile() }
                SocialIconButton { anchors.right: options.left; y: 6; tint: Theme.green; icon: "phone"; label: "Call"
                    visible: !root.social.conversationInfo.guild
                    enabled: !!root.account.voice && (root.account.voice.available || !!root.account.voice.channel)
                    onClicked: { root.anchorMenu(this); root.social.call() } }
                SocialIconButton { id: options; anchors.right: parent.right; y: 6; icon: "dots-three"; label: "Conversation options"
                    onClicked: { root.anchorMenu(this); root.social.contextRow() } }
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#c6b4d6" }
            }
            Rectangle { id: activityStrip; y: chatHeading.height; width: parent.width; height: visible ? 25 : 0; radius: 5; color: "#deedcb"
                readonly property var party: root.social.gameParty
                readonly property var voice: root.account.voice || ({})
                visible: !!party.party || !!party.joining || !!voice.channel
                Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; x: 8; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 16; text: (activityStrip.party.party ? ((activityStrip.party.game || {}).label || "Game party") : activityStrip.party.joining ? "Join request sent" : "") + (activityStrip.voice.channel ? "  Call · " + (activityStrip.voice.name || "Connected") : ""); font.pixelSize: 11; color: Theme.ink; elide: Text.ElideRight; textFormat: Text.PlainText }
                MouseArea { anchors.fill: parent; onClicked: { root.anchorMenu(parent); root.social.together() } }
            }
            Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; id: connectionStatus; y: activityStrip.y + activityStrip.height; width: parent.width; height: visible ? 22 : 0
                visible: !!root.account.status && root.account.status !== "Connected"
                text: root.account.status || ""; color: "#80542a"; font.pixelSize: 11; elide: Text.ElideRight; textFormat: Text.PlainText }
            ListView {
                id: log; objectName: "social-message-log"
                anchors { top: connectionStatus.bottom; topMargin: 7; bottom: composer.top; bottomMargin: 8; left: parent.left; right: parent.right }
                clip: true; spacing: 2; model: root.social.messages; currentIndex: root.social.messageIndex
                onMovementEnded: {
                    const first = indexAt(42, contentY + 8)
                    if(first >= 0) root.social.retainMessagePosition(first, atYEnd)
                    if(atYBeginning && root.account.historyMore) root.social.earlierMessages()
                }
                function restorePosition() {
                    if (moving || dragging) return
                    forceLayout()
                    if (currentIndex >= count - 1) positionViewAtEnd()
                    else positionViewAtIndex(currentIndex,ListView.Contain)
                }
                onCurrentIndexChanged: Qt.callLater(restorePosition)
                onModelChanged: Qt.callLater(restorePosition)
                onHeightChanged: Qt.callLater(restorePosition)
                Timer {
                    interval: 1000; repeat: true
                    running: root.visible && root.connected && root.faceIndex !== 2 && root.social.conversation && (log.atYEnd || log.contentHeight <= log.height) && root.social.surfaceAvailable
                    onTriggered: if (root.account.readTail) root.social.presented(root.account.channel, root.account.readTail)
                }
                header: Item { width: log.width; height: root.account.historyMore ? 25 : 0
                    Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; anchors.centerIn: parent; visible: parent.height > 0; text: root.account.historyBusy ? "Loading..." : "Earlier messages"; color: Theme.muted; font.pixelSize: 11 }
                    MouseArea { anchors.fill: parent; onClicked: root.social.earlierMessages() }
                }
                delegate: Item {
                    id: messageRow
                    required property var modelData
                    required property int index
                    readonly property var previous: index > 0 ? root.social.messages[index-1] : null
                    readonly property bool grouped: !!previous && !modelData.system && !previous.system && !!modelData.author && previous.author === modelData.author && !!modelData.timestamp && Math.abs(Date.parse(modelData.timestamp)-Date.parse(previous.timestamp)) < 300000
                    width: log.width; height: content.height + (grouped ? 2 : 12)
                    Rectangle { anchors.fill: parent; radius: 5; color: root.social.reading && index === log.currentIndex ? "#fff0bd" : "transparent" }
                    MouseArea { anchors.fill: parent; onClicked: root.social.selectMessage(index); onPressAndHold: { root.social.selectMessage(index); root.anchorMenu(messageRow); root.social.options() } }
                    Rectangle { x: 2; y: 6; width: 28; height: 28; radius: 9; visible: !messageRow.grouped && !modelData.system; color: modelData.mine ? Theme.pink : Theme.blue
                        Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; anchors.centerIn: parent; text: modelData.name.substring(0,1).toUpperCase(); font.pixelSize: 16; color: Theme.ink; textFormat: Text.PlainText }
                        Image { anchors.fill: parent; source: modelData.avatar || ""; sourceSize: Qt.size(56,56); asynchronous: true }
                        MouseArea { anchors.fill: parent; onClicked: root.social.profile(modelData.author) }
                    }
                    Column { id: content; x: modelData.system ? 8 : 40; y: messageRow.grouped ? 0 : 5; width: parent.width - x - 8; spacing: 3
                        Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; visible: !modelData.system && !messageRow.grouped; width: parent.width; text: modelData.name + (modelData.timestamp ? "   " + Qt.formatDateTime(new Date(modelData.timestamp), "ddd HH:mm") : ""); font.pixelSize: 12; font.bold: true; color: Theme.muted; elide: Text.ElideRight; textFormat: Text.PlainText
                            MouseArea { anchors.fill: parent; onClicked: root.social.profile(modelData.author) } }
                        Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; width: parent.width; visible: !!modelData.text || modelData.system; text: (modelData.text || (modelData.system ? "Conversation updated" : "")) + (modelData.edited ? " (edited)" : ""); font.pixelSize: modelData.system ? 12 : 15; color: modelData.system ? Theme.muted : Theme.ink; wrapMode: Text.Wrap; textFormat: Text.PlainText }
                        Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; visible: !!modelData.callDetail; width: parent.width; text: modelData.callDetail || ""; font.pixelSize: 11; color: Theme.muted; wrapMode: Text.Wrap; textFormat: Text.PlainText }
                        Repeater { model: modelData.attachments || []
                            Rectangle { required property var modelData; width: content.width; height: 34; radius: 6; color: "#deedf3"
                                Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; anchors { fill: parent; margins: 8 } text: modelData.content_type && modelData.content_type.startsWith("audio/") ? "Voice message · " + (modelData.duration || 0) + " s" : "Picture"; font.pixelSize: 13; color: Theme.ink; textFormat: Text.PlainText }
                            }
                        }
                        Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; visible: !!modelData.delivery; width: parent.width; text: modelData.delivery || ""; font.pixelSize: 11; color: "#80542a"; wrapMode: Text.Wrap; textFormat: Text.PlainText }
                    }
                }
            }
            Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; anchors.centerIn: log; width: log.width - 40; visible: log.count === 0
                text: root.account.historyBusy ? "Loading messages..." : "Say hello!"; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; font.pixelSize: 18; color: Theme.muted }
            Rectangle { visible: !!root.account.historyPast; anchors { right: log.right; bottom: log.bottom; margins: 5 } width: 85; height: 28; radius: 8; color: "#e7ddf1"; border.color: "#bdacce"
                Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; anchors.centerIn: parent; text: "↓ Latest"; color: Theme.ink; font.pixelSize: 12 }
                MouseArea { anchors.fill: parent; onClicked: root.social.latest() }
            }
            Rectangle { id: composer; anchors { left: parent.left; right: parent.right; bottom: parent.bottom; bottomMargin: 9 } height: 40; radius: 9; color: Theme.paper; border.color: "#d1b8a0"
                SocialIconButton { id: attach; x: 3; anchors.verticalCenter: parent.verticalCenter; tint: Theme.blue; icon: "plus"; label: "Attach a picture"; onClicked: { root.anchorMenu(this); root.social.attachPicture() } }
                Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; anchors { left: attach.right; right: emoji.left; margins: 7; verticalCenter: parent.verticalCenter } text: root.social.draft || "Write a message..."; font.pixelSize: 14; color: root.social.draft ? Theme.ink : Theme.muted; elide: Text.ElideRight; textFormat: Text.PlainText
                    MouseArea { anchors.fill: parent; anchors.topMargin: -10; anchors.bottomMargin: -10; onClicked: root.social.compose() } }
                SocialIconButton { id: emoji; anchors { right: mic.left; verticalCenter: parent.verticalCenter } tint: Theme.yellow; icon: "smiley"; label: "Emoji"; onClicked: { root.anchorMenu(this); root.social.emoji() } }
                SocialIconButton { id: mic; anchors { right: send.left; verticalCenter: parent.verticalCenter } tint: Theme.pink; icon: "microphone"; label: "Record voice message"; enabled: !root.account.voice || !root.account.voice.channel; onClicked: { root.anchorMenu(this); root.social.recordVoice() } }
                SocialIconButton { id: send; objectName: "social-send"; anchors { right: parent.right; rightMargin: 3; verticalCenter: parent.verticalCenter } tint: Theme.green; icon: "paper-plane-right"; label: "Send"; highlighted: enabled
                    enabled: root.account.state === "connected" && root.social.draft.trim().length > 0; onClicked: root.social.sendDraft() }
            }
        }
        Column { visible: !root.social.conversation; x: people.width + 50; width: parent.width - x - 30; anchors.verticalCenter: parent.verticalCenter; spacing: 12
            Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; width: parent.width; text: root.faceIndex === 1 ? "A place for your people" : "No conversations yet"; font.family: Theme.displayFamily; font.pixelSize: 25; color: Theme.ink; horizontalAlignment: Text.AlignHCenter }
            Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; width: parent.width; text: "Find friends in Discover, or use + to create a group."; font.pixelSize: 15; color: Theme.muted; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap }
        }
    }
    Item {
        id: searchPage
        visible: root.connected && root.faceIndex === 2
        anchors { top: heading.bottom; bottom: parent.bottom; left: parent.left; right: parent.right }
        Rectangle { anchors.fill: parent; color: "#eaf0e3" }
        Row {
            id: categories; x: 16; y: 10; spacing: 10
            Repeater {
                model: [{kind:"people",title:"People",tint:Theme.pink}, {kind:"communities",title:"Communities",tint:Theme.green}, {kind:"traineros",title:"TrainerOS",tint:Theme.yellow}, {kind:"invite",title:"Invite link",tint:Theme.blue}]
                CapButton { required property var modelData; width: (searchPage.width - 62) / 4; height: 38
                    label: modelData.title; tint: modelData.tint; claimsFocus: false
                    selected: root.social.searchKind === modelData.kind && root.social.searchFocus < 0
                    onActivated: root.social.setSearchKind(modelData.kind)
                }
            }
        }
        CapButton {
            id: searchBar; x: 16; y: categories.y + categories.height + 12; width: parent.width - 32; height: 48
            label: root.social.query || (root.social.searchKind === "people" ? "username#1234" : root.social.searchKind === "invite" ? "Paste or enter an invitation" : "Search communities...")
            tint: Theme.paper; claimsFocus: false; selected: false
            onActivated: root.social.editSearch()
        }
        Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; id: searchStatus; x: 20; y: searchBar.y + searchBar.height + 9; width: parent.width - 40
            text: root.account.searchStatus || ""; font.pixelSize: 13; color: Theme.muted; textFormat: Text.PlainText; elide: Text.ElideRight
        }
        GridView {
            id: results; x: 16; y: searchStatus.y + searchStatus.height + 10; width: parent.width - 24; height: parent.height - y - 12
            clip: true; cellWidth: width / 2; cellHeight: 121
            model: root.social.searchResults; currentIndex: root.social.searchFocus
            onCurrentIndexChanged: if(currentIndex >= 0) positionViewAtIndex(currentIndex, GridView.Contain)
            delegate: Item {
                required property var modelData; required property int index
                width: results.cellWidth; height: results.cellHeight
                Rectangle { x: 0; y: 4; width: parent.width - 10; height: parent.height - 10; radius: 12; color: "#617b6959" }
                Rectangle { width: parent.width - 10; height: parent.height - 10; radius: 12
                    color: index % 3 === 0 ? "#e1efca" : index % 3 === 1 ? "#d8ebf1" : "#f3dfd4"
                    border.width: results.currentIndex === index ? 3 : 1; border.color: results.currentIndex === index ? "#e7b327" : "#a2b4a3"
                    Column { x: 14; y: 10; width: parent.width - 28; spacing: 4
                        Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; width: parent.width; text: modelData.name; font.family: Theme.displayFamily; font.pixelSize: 20; color: Theme.ink; textFormat: Text.PlainText; elide: Text.ElideRight }
                        Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; width: parent.width; text: modelData.description; font.pixelSize: 13; color: Theme.ink; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight; textFormat: Text.PlainText }
                    }
                    Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; x: 14; anchors.bottom: parent.bottom; anchors.bottomMargin: 10; width: parent.width - 130; text: modelData.detail; font.pixelSize: 12; color: Theme.muted; elide: Text.ElideRight; textFormat: Text.PlainText }
                    Rectangle { anchors { right: parent.right; bottom: parent.bottom; margins: 9 } width: 96; height: 25; radius: 8; color: Theme.yellow; border.color: "#b4a36a"
                        Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; anchors.centerIn: parent; text: modelData.action; color: Theme.ink; font.pixelSize: 13; font.bold: true; textFormat: Text.PlainText }
                    }
                    MouseArea { anchors.fill: parent; onClicked: root.social.activateSearch(index) }
                }
            }
        }
        Column {
            visible: !results.count; anchors.horizontalCenter: parent.horizontalCenter; y: results.y + 22; spacing: 13
            Item { width: 76; height: 69; anchors.horizontalCenter: parent.horizontalCenter
                Rectangle { x: 44; y: 42; width: 31; height: 12; radius: 5; rotation: 45; color: "#648983" }
                Rectangle { x: 0; y: 0; width: 55; height: 55; radius: 28; color: "#d1e8ef"; border.color: "#648983"; border.width: 6 }
                Rectangle { x: 14; y: 12; width: 23; height: 9; radius: 5; rotation: -30; color: "#f4ffff" }
            }
            Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; text: root.account.searching ? "Looking around..." : root.social.searchKind === "people" ? "Make a new friend" : root.social.searchKind === "invite" ? "A place for your people" : "Find your next community"; font.family: Theme.displayFamily; font.pixelSize: 24; color: Theme.ink; anchors.horizontalCenter: parent.horizontalCenter }
        }
    }
    Rectangle {
        visible: root.social.menu.length > 0; anchors.fill: parent; color: "#6630433b"
        MouseArea { anchors.fill: parent; onClicked: root.social.back() }
        Rectangle {
            width: Math.min(root.social.profileVisible || root.social.mediaPreview ? 350 : 310, parent.width - 24)
            x: root.social.profileVisible || root.social.mediaPreview ? (parent.width-width)/2 : Math.max(12,Math.min(root.menuAnchor.x,parent.width-width-12))
            y: root.social.profileVisible || root.social.mediaPreview ? (parent.height-height)/2 : Math.max(8,Math.min(root.menuAnchor.y,parent.height-height-8))
            height: Math.min(parent.height - 24, menuHeading.height + (root.social.emojiVisible ? 78 : menuList.contentHeight) + 32)
            radius: 10; color: "#fffaf2"; border.color: "#c6b4d6"; border.width: 1
            MouseArea { anchors.fill: parent }
            SocialIconButton { anchors { right: parent.right; top: parent.top; margins: 5 } z: 2; icon: "x"; label: "Close"; onClicked: root.social.back() }
            Column {
                id: menuHeading; x: 14; y: 12; width: parent.width - 28; spacing: 6
                Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; width: parent.width - 38; text: root.social.menuTitle || "Social"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 17; textFormat: Text.PlainText; elide: Text.ElideRight }
                Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; visible: text.length > 0; width: parent.width; text: root.social.menuDetail; color: Theme.muted; font.pixelSize: 13; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight; textFormat: Text.PlainText }
                Column { visible: root.social.profileVisible; width: parent.width; spacing: 7
                    readonly property var person: root.social.person
                    Row { spacing: 12; width: parent.width
                        Rectangle { width: 46; height: 46; radius: 13; color: Theme.pink
                            Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; anchors.centerIn: parent; text: (root.social.person.name || "?").substring(0,1); color: Theme.ink; font.pixelSize: 24 }
                            Image { anchors.fill: parent; source: root.social.person.avatar || ""; sourceSize: Qt.size(92,92); asynchronous: true }
                        }
                        Column { width: parent.width - 58; spacing: 4
                            Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; width: parent.width; text: root.social.person.name || ""; color: Theme.ink; font.pixelSize: 18; elide: Text.ElideRight; textFormat: Text.PlainText }
                            Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; width: parent.width; text: root.social.person.tag || ""; color: Theme.muted; font.pixelSize: 12; elide: Text.ElideRight; textFormat: Text.PlainText }
                            Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; visible: !!text; text: root.social.person.pronouns || ""; color: Theme.muted; font.pixelSize: 11; textFormat: Text.PlainText }
                        }
                    }
                    Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; visible: !!text; width: parent.width; text: root.social.person.bio || ""; color: Theme.ink; font.pixelSize: 13; wrapMode: Text.Wrap; maximumLineCount: 5; elide: Text.ElideRight; textFormat: Text.PlainText }
                    Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; visible: !!text; width: parent.width; text: root.social.person.status || ""; color: Theme.muted; font.pixelSize: 12; wrapMode: Text.Wrap; textFormat: Text.PlainText }
                    Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; visible: !!root.social.person.loaded; width: parent.width; text: (root.social.person.mutualFriends || 0) + " mutual friends · " + (root.social.person.mutualCommunities || 0) + " mutual communities"; color: Theme.muted; font.pixelSize: 11; wrapMode: Text.Wrap }
                }
                CapButton { visible: root.social.canCreateGroup; width: parent.width; height: visible ? 40 : 0; label: "Create group"; tint: Theme.yellow; claimsFocus: false; onActivated: root.social.createSelectedGroup() }
                Image { visible: root.social.mediaPreview && root.social.media.picture.toString().length > 0; width: parent.width; height: visible ? 165 : 0; source: visible ? root.social.media.picture : ""; sourceSize: Qt.size(740,330); fillMode: Image.PreserveAspectFit; asynchronous: true }
                Rectangle { visible: root.social.mediaPreview && root.social.media.state === "recording"; width: parent.width; height: visible ? 52 : 0; radius: 10; color: "#f1c8c4"
                    Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; anchors.centerIn: parent; text: "●  " + root.social.media.seconds + " s / 120 s"; font.family: Theme.displayFamily; font.pixelSize: 22; color: "#9d3942" }
                }
                Item { visible: root.social.mediaPreview && root.social.media.state === "voice"; width: parent.width; height: visible ? 45 : 0
                    Row { anchors.centerIn: parent; spacing: 2
                        Repeater { model: root.social.media.levels
                            Rectangle { required property int modelData; required property int index; width: 3; height: 5 + modelData / 8; anchors.verticalCenter: parent.verticalCenter; radius: 2; color: index / Math.max(1,root.social.media.levels.length) <= root.social.media.progress ? "#ba8d16" : "#779fa7" }
                        }
                    }
                }
            }
            ListView {
                id: menuList; x: 15; y: menuHeading.y + menuHeading.height + 8
                visible: !root.social.emojiVisible
                width: parent.width - 30; height: parent.height - y - 8; clip: true; spacing: 1
                model: root.social.menu; currentIndex: root.social.menuIndex
                onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)
                delegate: Rectangle {
                    required property string modelData; required property int index
                    width: menuList.width; height: Math.max(30, actionText.implicitHeight + 12); radius: 5
                    color: index === root.social.menuIndex ? "#fbe6a1" : actionMouse.containsMouse ? "#eee3f4" : "transparent"
                    Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; id: actionText; x: 9; y: 6; width: parent.width - 18; text: modelData; font.pixelSize: 14; font.family: Theme.displayFamily; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight; textFormat: Text.PlainText
                        color: modelData.startsWith("Delete") || modelData.startsWith("Leave") || modelData.startsWith("Block") || modelData.startsWith("Remove") ? "#983e45" : Theme.ink }
                    MouseArea { id: actionMouse; anchors.fill: parent; hoverEnabled: true; onClicked: root.social.selectMenu(index) }
                }
            }
            Column { visible: root.social.emojiVisible; x: 15; y: menuHeading.y + menuHeading.height + 8; width: parent.width - 30; spacing: 4
                Row { width: parent.width
                    Repeater { model: root.social.emojiVisible ? root.social.menu.slice(0,5) : []
                        Rectangle { required property string modelData; required property int index
                            width: 56; height: 40; radius: 7; color: root.social.menuIndex === index ? "#fbe6a1" : "transparent"
                            Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; anchors.centerIn: parent; text: modelData; font.pixelSize: 22 }
                            MouseArea { anchors.fill: parent; onClicked: root.social.selectMenu(index) }
                        }
                    }
                }
                Rectangle { width: parent.width; height: 30; radius: 5; color: root.social.menuIndex === 5 ? "#fbe6a1" : "transparent"
                    Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; x: 9; anchors.verticalCenter: parent.verticalCenter; text: "More emoji..."; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 14 }
                    MouseArea { anchors.fill: parent; onClicked: root.social.selectMenu(5) }
                }
            }
        }
    }
}
