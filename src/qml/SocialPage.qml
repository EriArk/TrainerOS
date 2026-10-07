import QtQuick

FocusScope {
    id: root
    required property var shell
    readonly property int faceIndex: ["chats","communities","friends"].indexOf(shell.socialFace)
    readonly property var social: shell.social
    readonly property var account: social.account
    readonly property bool connected: account.state === "connected" || account.state === "connecting"
    readonly property bool takesFocus: visible && !shell.serviceOpen && !shell.menuOpen && !shell.drawerOpen && !shell.keyboard.open && shell.notice.length === 0
    objectName: "social-empty"
    onTakesFocusChanged: if (takesFocus) forceActiveFocus()
    Component.onCompleted: if (takesFocus) forceActiveFocus()
    PageHeader {
        id: heading
        title: [root.social.contacts ? "Friends" : "Messages", "Communities", "Discover"][root.faceIndex]
        subtitle: root.connected ? (root.account.name || "") + " · " + (root.faceIndex === 1 && root.account.communityStatus ? root.account.communityStatus : root.account.status || "") : "A little closer, wherever you are."
        Text {
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
            Text { y: 168; width: parent.width; text: "HELLO, FRIEND!"; horizontalAlignment: Text.AlignHCenter; font.family: Theme.brandFamily; font.pixelSize: 21; color: Theme.ink }
        }
        Column {
            x: parent.width * 0.36 + 32; width: parent.width * 0.64 - 64
            anchors.verticalCenter: parent.verticalCenter; spacing: 14
            Text { width: parent.width; text: root.account.state === "authorizing" ? "Let's get you connected" : "Your friends, along for the ride"; wrapMode: Text.WordWrap; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 26 }
            Text { width: parent.width; text: root.account.state === "authorizing" ? (root.account.status || "Connecting...") : "Chat with your friends from your handheld."; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: 16; textFormat: Text.PlainText }
            Rectangle { visible: (root.account.code || "").length > 0; width: parent.width; height: 60; radius: 10; color: "#fff4c6"; border.color: "#c5af66"; border.width: 2
                Text { anchors.centerIn: parent; text: root.account.code || ""; font.family: Theme.brandFamily; font.pixelSize: 27; color: Theme.ink; textFormat: Text.PlainText }
            }
            CapButton { visible: root.account.state !== "authorizing"; width: parent.width; height: 54; label: root.account.state === "restoring" ? "Signing in..." : "Sign in"; tint: Theme.yellow; selected: root.takesFocus; enabled: !!root.account.available && root.account.state !== "restoring"; claimsFocus: false; onActivated: root.social.login() }
            Text { width: parent.width; visible: root.account.state !== "authorizing" && !!root.account.status && root.account.status !== "Sign in"; text: root.account.status || ""; color: Theme.muted; font.pixelSize: 13; wrapMode: Text.WordWrap; textFormat: Text.PlainText }
        }
    }
    Item {
        visible: root.connected && root.faceIndex !== 2
        anchors { top: heading.bottom; bottom: parent.bottom; left: parent.left; right: parent.right }
        Rectangle { width: people.width + 30; height: parent.height; color: "#dce9d9"; border.color: "#b9cebd" }
        Text { x: 20; y: 10; visible: root.faceIndex === 1; width: people.width
            text: (root.account.communityOnly ? "TrainerOS communities" : "All communities") + (root.account.communityChecking ? " · Checking..." : "")
            font.family: Theme.displayFamily; font.pixelSize: 14; color: Theme.ink; elide: Text.ElideRight }
        CapButton {
            x: 14; y: 7; width: parent.width * 0.31 - 5; height: 35
            visible: root.faceIndex === 0; label: root.social.contacts ? "Messages" : "Friends & requests"
            tint: Theme.yellow; claimsFocus: false
            onActivated: root.social.showFriends()
        }
        ListView {
            id: people
            x: 14; y: 52; width: parent.width * 0.31; height: parent.height - y - 14
            clip: true; spacing: 10; model: root.social.rows; currentIndex: root.social.focusIndex
            onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
            delegate: CapButton {
                required property var modelData
                required property int index
                width: people.width - 5; height: 65; claimsFocus: false; contentInset: 52
                label: modelData.name; detail: (modelData.kind === "groups" ? "Group · " : "") + modelData.detail
                tint: modelData.type === 3 ? Theme.yellow : index % 3 === 0 ? Theme.blue : index % 3 === 1 ? Theme.green : Theme.pink
                selected: index === people.currentIndex && !root.social.reading && !root.social.partyFocused && !root.social.menu.length
                onActivated: root.social.activate(index)
                Rectangle { x: 10; y: 13; width: 32; height: 32; radius: 11; color: "#dcfffdf0"; border.color: "#738f7c"
                    Image { id: avatarImage; anchors.fill: parent; anchors.margins: 1; source: root.visible ? (modelData.avatar || "") : ""; sourceSize.width: 64; sourceSize.height: 64; asynchronous: true; fillMode: Image.PreserveAspectFit; visible: status === Image.Ready }
                    Text { visible: avatarImage.status !== Image.Ready; anchors.centerIn: parent; text: modelData.name.substring(0,1).toUpperCase(); color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 22; textFormat: Text.PlainText }
                }
            }
        }
        Text { x: people.x + 6; y: 108; width: people.width - 16; visible: !people.count
            text: root.faceIndex === 1 ? "Your communities will appear here." : "Your conversations will appear here."
            wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; color: Theme.muted; font.pixelSize: 17
        }
        Item {
            anchors { left: people.right; leftMargin: 30; right: parent.right; rightMargin: 18; top: parent.top; bottom: parent.bottom }
            visible: root.social.conversation
            Text { id: chatHeading; y: 10; width: parent.width; text: root.social.conversationName + (root.account.historyBusy ? " · Loading..." : ""); font.family: Theme.displayFamily; font.pixelSize: 23; color: Theme.ink; elide: Text.ElideRight; textFormat: Text.PlainText }
            Rectangle { y: 42; width: parent.width; height: 1; color: "#b4c6b8" }
            CapButton {
                id: togetherStrip; objectName: "social-together"
                y: 48; width: parent.width; height: 63; claimsFocus: false; tint: Theme.yellow
                label: "Y · Together"
                readonly property var party: root.social.gameParty
                readonly property var offer: root.social.gameActivity
                readonly property var voice: root.account.voice || ({})
                detail: (party.joining ? "Join request sent" : party.party ? ((party.game || {}).label || "Game party") + " · " + (party.running ? "Playing" : "Gathering players")
                    : root.social.companyParties.length ? root.social.companyParties.length + (root.social.companyParties.length === 1 ? " game party" : " game parties")
                    : offer.joinable ? ((offer.game || {}).label || "Game") + " · Ask to join" : "Calls, games & activities")
                    + (voice.channel ? "  ·  Call: " + (voice.name || voice.status || "Connected") : "")
                onActivated: root.social.together()
            }
            CapButton {
                id: latestButton; anchors { right: parent.right; top: togetherStrip.bottom; topMargin: 6 }
                width: 148; height: visible ? 30 : 0; visible: !!root.account.historyPast
                label: "Latest"; tint: Theme.blue; claimsFocus: false
                onActivated: root.social.latest()
            }
            CapButton {
                id: earlierButton; anchors { left: parent.left; top: togetherStrip.bottom; topMargin: 6 }
                width: 164; height: visible ? 30 : 0; visible: !!root.account.historyMore
                enabled: !root.account.historyBusy; label: "Earlier"; tint: Theme.blue; claimsFocus: false
                onActivated: root.social.earlierMessages()
            }
            ListView {
                id: log
                anchors { top: latestButton.visible ? latestButton.bottom : earlierButton.visible ? earlierButton.bottom : togetherStrip.bottom; topMargin: 10; bottom: composer.top; bottomMargin: 9; left: parent.left; right: parent.right }
                clip: true; spacing: 10; model: root.social.messages; currentIndex: root.social.messageIndex
                onMovementEnded: {
                    const first = indexAt(10, contentY + 10)
                    if(first >= 0) root.social.retainMessagePosition(first, atYEnd)
                }
                function restorePosition() {
                    forceLayout()
                    if (currentIndex >= count - 1) positionViewAtEnd()
                    else positionViewAtIndex(currentIndex,ListView.Contain)
                }
                onCurrentIndexChanged: Qt.callLater(restorePosition)
                onModelChanged: Qt.callLater(restorePosition)
                onContentHeightChanged: Qt.callLater(restorePosition)
                onHeightChanged: Qt.callLater(restorePosition)
                Timer {
                    interval: 1000; repeat: true
                    running: root.visible && root.connected && root.faceIndex !== 2 && root.social.conversation && (log.atYEnd || log.contentHeight <= log.height) && root.social.surfaceAvailable
                    onTriggered: if (root.account.readTail) root.social.presented(root.account.channel, root.account.readTail)
                }
                delegate: Item {
                    required property var modelData
                    required property int index
                    width: log.width; height: bubble.height + 3
                    MouseArea { anchors.fill: parent; z: 1; onClicked: root.social.selectMessage(index) }
                    Rectangle {
                        id: bubble
                        x: modelData.mine && !modelData.system ? 26 : 2; width: parent.width - 30
                        height: content.height + (modelData.system ? 10 : 18); radius: 11
                        color: modelData.callEvent ? (modelData.missedCall ? "#f3dfd4" : "#e0ebdf") : modelData.system ? "transparent" : modelData.mine ? "#e5efbe" : "#d5eaf1"
                        border.color: root.social.reading && index === log.currentIndex ? "#c09220" : modelData.mine ? "#a7be81" : "#a1c1c8"
                        border.width: root.social.reading && index === log.currentIndex ? 2 : modelData.system ? 0 : 1
                        Column { id: content; x: 12; y: modelData.system ? 4 : 8; width: parent.width - 24; spacing: 3
                            Text { visible: !modelData.system; width: parent.width; text: modelData.name + (modelData.edited ? " (edited)" : ""); font.family: Theme.displayFamily; font.pixelSize: 13; color: Theme.muted; elide: Text.ElideRight; textFormat: Text.PlainText }
                            Text { width: parent.width; visible: !!modelData.text || modelData.system; text: modelData.text || (modelData.system ? "Conversation updated" : ""); font.pixelSize: modelData.system ? 13 : 16; color: modelData.system ? Theme.muted : Theme.ink; wrapMode: Text.Wrap; textFormat: Text.PlainText }
                            Text { visible: !!modelData.callDetail; width: parent.width; text: modelData.callDetail || ""; font.pixelSize: 11; color: Theme.muted; wrapMode: Text.Wrap; textFormat: Text.PlainText }
                            Repeater {
                                model: modelData.attachments || []
                                Rectangle {
                                    required property var modelData
                                    width: content.width; height: 44; radius: 9; color: modelData.content_type && modelData.content_type.startsWith("audio/") ? "#e8dfed" : "#fff0c2"; border.color: "#a8b9a4"
                                    Text { anchors { fill: parent; margins: 10 } text: modelData.content_type && modelData.content_type.startsWith("audio/") ? "♪  Voice message · " + (modelData.duration || 0) + " s" : "▧  Picture"; font.family: Theme.displayFamily; font.pixelSize: 17; color: Theme.ink; textFormat: Text.PlainText }
                                }
                            }
                            Text { visible: modelData.delivery.length > 0; width: parent.width; text: modelData.delivery; font.pixelSize: 11; color: "#80542a"; wrapMode: Text.Wrap; textFormat: Text.PlainText }
                        }
                    }
                }
            }
            Text {
                anchors.centerIn: log; width: log.width - 40; visible: log.count === 0
                text: root.account.historyBusy ? "Loading messages..." : root.account.historyMore ? "Earlier messages are above." : "Say hello!"
                horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                font.family: Theme.displayFamily; font.pixelSize: 22; color: Theme.muted
            }
            Item { id: composer; anchors { left: parent.left; right: parent.right; bottom: parent.bottom; bottomMargin: 12 }
                height: 92
                Row { spacing: 8
                    CapButton { width: 112; height: 38; label: "+ Picture"; tint: Theme.blue; claimsFocus: false; onActivated: root.social.attachPicture() }
                    CapButton { width: 106; height: 38; label: "● Voice"; tint: Theme.pink; claimsFocus: false; enabled: !root.account.voice || !root.account.voice.channel; onActivated: root.social.recordVoice() }
                    CapButton { width: 102; height: 38; label: "☎ Call"; tint: Theme.green; claimsFocus: false; enabled: !!root.account.voice && (root.account.voice.available || !!root.account.voice.channel); onActivated: root.social.call() }
                }
                CapButton { anchors.right: parent.right; width: 48; height: 38; label: "···"; centered: true; tint: Theme.paper; claimsFocus: false; Accessible.name: "Conversation options"; onActivated: root.social.options() }
                CapButton {
                    y: 46; width: parent.width-85; height: 46; claimsFocus: false; tint: Theme.paper
                    label: root.social.draft || "X · Write a message..."
                    onActivated: root.social.compose()
                }
                CapButton {
                    objectName: "social-send"; y: 46; anchors.right: parent.right; width: 76; height: 46
                    label: "➤ Send"; textSize: 15; contentInset: 7; centered: true; tint: Theme.yellow; claimsFocus: false
                    enabled: root.account.state === "connected" && root.social.draft.trim().length > 0
                    onActivated: root.social.sendDraft()
                }
            }
        }
        Column { visible: !root.social.conversation; x: people.width + 62; width: parent.width - x - 30; anchors.verticalCenter: parent.verticalCenter; spacing: 16
            Text { width: parent.width; text: ["No conversations yet", "A place for your people"][root.faceIndex] || ""; font.family: Theme.displayFamily; font.pixelSize: 29; color: Theme.ink; horizontalAlignment: Text.AlignHCenter }
            Text { width: parent.width; text: root.faceIndex === 1 ? "Create a community, or find one in Discover." : "Find friends in Discover, or create a group through Options."; font.pixelSize: 17; color: Theme.muted; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap }
            CapButton { anchors.horizontalCenter: parent.horizontalCenter; width: 170; height: 42; label: "Options"; centered: true; tint: Theme.yellow; claimsFocus: false; onActivated: root.social.options() }
            Row { anchors.horizontalCenter: parent.horizontalCenter; spacing: 12
                Repeater { model: [Theme.blue,Theme.pink,Theme.yellow]
                    Rectangle { required property color modelData; width: 48; height: 38; radius: 12; color: modelData; border.color: Qt.darker(modelData,1.35)
                        Text { anchors.centerIn: parent; text: ":)"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 20 }
                    }
                }
            }
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
        Text { id: searchStatus; x: 20; y: searchBar.y + searchBar.height + 9; width: parent.width - 40
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
                        Text { width: parent.width; text: modelData.name; font.family: Theme.displayFamily; font.pixelSize: 20; color: Theme.ink; textFormat: Text.PlainText; elide: Text.ElideRight }
                        Text { width: parent.width; text: modelData.description; font.pixelSize: 13; color: Theme.ink; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight; textFormat: Text.PlainText }
                    }
                    Text { x: 14; anchors.bottom: parent.bottom; anchors.bottomMargin: 10; width: parent.width - 130; text: modelData.detail; font.pixelSize: 12; color: Theme.muted; elide: Text.ElideRight; textFormat: Text.PlainText }
                    Rectangle { anchors { right: parent.right; bottom: parent.bottom; margins: 9 } width: 96; height: 25; radius: 8; color: Theme.yellow; border.color: "#b4a36a"
                        Text { anchors.centerIn: parent; text: modelData.action; color: Theme.ink; font.pixelSize: 13; font.bold: true; textFormat: Text.PlainText }
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
            Text { text: root.account.searching ? "Looking around..." : root.social.searchKind === "people" ? "Make a new friend" : root.social.searchKind === "invite" ? "A place for your people" : "Find your next community"; font.family: Theme.displayFamily; font.pixelSize: 24; color: Theme.ink; anchors.horizontalCenter: parent.horizontalCenter }
        }
    }
    Rectangle {
        visible: root.social.menu.length > 0; anchors.fill: parent; color: "#6630433b"
        MouseArea { anchors.fill: parent; onClicked: root.social.back() }
        HomeMenuCard {
            visible: root.social.runtimeSurface
            anchors.centerIn: parent
            heading: root.social.menuTitle
            caption: root.social.menuDetail
            actions: root.social.runtimeActions
            currentIndex: root.social.menuIndex
            onChosen: index => root.social.selectMenu(index)
        }
        Rectangle {
            visible: !root.social.runtimeSurface
            anchors.centerIn: parent; width: Math.min(410, parent.width - 40)
            height: Math.min(parent.height - 24, menuHeading.height + menuList.contentHeight + 44)
            radius: 16; color: Theme.paper; border.color: Theme.chassis; border.width: 3
            MouseArea { anchors.fill: parent }
            CapButton {
                anchors { right: parent.right; top: parent.top; margins: 10 }
                z: 2; width: 40; height: 34; label: "×"; centered: true
                tint: Theme.paper; claimsFocus: false; Accessible.name: "Close"
                onActivated: root.social.back()
            }
            Column {
                id: menuHeading; x: 18; y: 15; width: parent.width - 36; spacing: 6
                Text { width: parent.width - 38; text: root.social.menuTitle || "Social"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 23; textFormat: Text.PlainText; elide: Text.ElideRight }
                Text { width: parent.width; text: root.social.menuDetail; color: Theme.muted; font.pixelSize: 13; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight; textFormat: Text.PlainText }
                CapButton { visible: root.social.canCreateGroup; width: parent.width; height: visible ? 40 : 0; label: "Create group"; tint: Theme.yellow; claimsFocus: false; onActivated: root.social.createSelectedGroup() }
                Image { visible: root.social.mediaPreview && root.social.media.picture.toString().length > 0; width: parent.width; height: visible ? 165 : 0; source: visible ? root.social.media.picture : ""; sourceSize: Qt.size(740,330); fillMode: Image.PreserveAspectFit; asynchronous: true }
                Rectangle { visible: root.social.mediaPreview && root.social.media.state === "recording"; width: parent.width; height: visible ? 52 : 0; radius: 10; color: "#f1c8c4"
                    Text { anchors.centerIn: parent; text: "●  " + root.social.media.seconds + " s / 120 s"; font.family: Theme.displayFamily; font.pixelSize: 22; color: "#9d3942" }
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
                id: menuList; x: 15; y: menuHeading.y + menuHeading.height + 12
                width: parent.width - 30; height: parent.height - y - 15; clip: true; spacing: 8
                model: root.social.menu; currentIndex: root.social.menuIndex
                onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)
                delegate: CapButton {
                    required property string modelData; required property int index
                    width: menuList.width; height: 42; label: modelData
                    selected: index === root.social.menuIndex; claimsFocus: false
                    tint: modelData.startsWith("Delete") || modelData.startsWith("Leave") || modelData.startsWith("Remove") || modelData.startsWith("Sign out") ? Theme.pink : Theme.blue
                    onActivated: root.social.selectMenu(index)
                }
            }
        }
    }
}
