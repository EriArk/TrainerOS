import QtQuick
import TrainerOS 1.0

FocusScope {
    id: root
    required property var shell
    readonly property int faceIndex: ["chats","communities","friends","lobby","recent"].indexOf(shell.socialSurface)
    readonly property var social: shell.social
    readonly property var account: social.account
    readonly property bool connected: account.state === "connected" || account.state === "connecting"
    readonly property bool takesFocus: visible && !shell.serviceOpen && !shell.menuOpen && !shell.drawerOpen && !shell.keyboard.open && shell.notice.length === 0
    property point menuAnchor: Qt.point(width - 310, 44)
    function anchorMenu(item) { menuAnchor = item.mapToItem(root, 0, item.height) }
    objectName: "social-empty"
    onVisibleChanged: if (!visible && social.stream.active) social.streamAction("stop-watch")
    Component.onDestruction: if (social && social.stream.active) social.streamAction("stop-watch")
    Rectangle {
        anchors.fill: parent; z: 8; visible: root.social.stream.active
        color: "#182521"
        MouseArea { anchors.fill: parent }
        GameStreamView {
            id: streamVideo; objectName: "game-stream-view"; anchors.fill: parent; anchors.bottomMargin: 48
            Component.onCompleted: root.social.stream.setVideoSink(videoSink)
            Component.onDestruction: root.social.stream.setVideoSink(null)
        }
        Text {
            anchors.centerIn: streamVideo; visible: !root.social.stream.hasFrame
            text: "Connecting to stream…"; color: Theme.paper; font.pixelSize: 20
        }
        Row {
            anchors { left: parent.left; bottom: parent.bottom; margins: 6 }
            spacing: 8
            CapButton { width: 160; height: 36; textSize: 13; claimsFocus: false; label: "B · Stop watching"; tint: Theme.pink; onActivated: root.social.streamAction("stop-watch") }
            CapButton { width: 150; height: 36; textSize: 13; claimsFocus: false; label: "A · Streams"; tint: Theme.blue; onActivated: root.social.streamsMenu() }
        }
    }
    LobbyPane {
        anchors { top: heading.bottom; bottom: parent.bottom; left: parent.left; right: parent.right }
        visible: root.connected && root.faceIndex === 3
        social: root.social; takesFocus: root.takesFocus && visible
        onMenuAnchored: item => root.anchorMenu(item)
    }
    RecentTogetherPane {
        anchors { top: heading.bottom; bottom: parent.bottom; left: parent.left; right: parent.right }
        visible: root.faceIndex === 4
        social: root.social; takesFocus: root.takesFocus && visible
    }
    onTakesFocusChanged: if (takesFocus) forceActiveFocus()
    Component.onCompleted: if (takesFocus) forceActiveFocus()
    PageHeader {
        id: heading
        visible: root.faceIndex !== 3 && (!root.connected || root.faceIndex >= 4)
        height: visible ? implicitHeight : 0
        title: [root.social.contacts ? "Friends" : "Messages", "Communities", "Discover", "Lobby", "Recently together"][root.faceIndex]
        subtitle: root.connected ? (root.account.name || "") + " · " + (root.faceIndex === 1 && root.account.communityStatus ? root.account.communityStatus : root.account.status || "") : "A little closer, wherever you are."
        Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering;
            anchors { right: parent.right; rightMargin: Theme.pageMargin; bottom: parent.bottom }
            text: "Powered by Fluxer"; color: Theme.muted; font.pixelSize: 9
        }
    }
    Item {
        anchors { top: heading.bottom; bottom: parent.bottom; left: parent.left; right: parent.right }
        visible: !root.connected && root.faceIndex !== 4
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
            Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; width: parent.width; text: root.account.state === "authorizing" ? (root.account.status || "Connecting...") : "Sign in to chat, call friends and send multiplayer invitations."; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: 16; textFormat: Text.PlainText }
            Rectangle { visible: (root.account.code || "").length > 0; width: parent.width; height: 60; radius: 10; color: "#fff4c6"; border.color: "#c5af66"; border.width: 2
                Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; anchors.centerIn: parent; text: root.account.code || ""; font.family: Theme.brandFamily; font.pixelSize: 27; color: Theme.ink; textFormat: Text.PlainText }
            }
            CapButton { visible: root.account.state !== "authorizing"; width: parent.width; height: 54; label: root.account.state === "restoring" ? "Signing in..." : "Sign in"; tint: Theme.yellow; selected: root.takesFocus; enabled: !!root.account.available && root.account.state !== "restoring"; claimsFocus: false; onActivated: root.social.login() }
            Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; width: parent.width; visible: root.account.state !== "authorizing" && !!root.account.status && root.account.status !== "Sign in"; text: root.account.status || ""; color: Theme.muted; font.pixelSize: 13; wrapMode: Text.WordWrap; textFormat: Text.PlainText }
        }
    }
    Item {
        id: conversationPage
        visible: root.connected && root.faceIndex < 2
        anchors { top: heading.bottom; bottom: parent.bottom; left: parent.left; right: parent.right }
        Rectangle { anchors.fill: parent; color: Theme.paper }
        Canvas {
            anchors.fill: parent; opacity: 0.055
            property color patternColor: Theme.chassis
            onPatternColorChanged: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const c=getContext("2d"); c.reset();c.strokeStyle=patternColor;c.lineWidth=1.2
                for(let r=60;r<width;r+=38){c.beginPath();c.arc(width*.81,height*.69,r,0,Math.PI*2);c.stroke()}
            }
        }
        Rectangle { x: people.x + people.width + 12; y: 12; width: 1; height: parent.height-28; color: Theme.chassis; opacity: .18 }
        SocialIconButton {
            x: 12; y: 6; visible: root.faceIndex===1 && root.social.communityOpen
            highlighted: root.social.focusIndex<0 && !root.social.reading
            icon: "arrow-left"; label: "Back to communities"; onClicked: root.social.communityBack()
        }
        Text { x: root.faceIndex===1 && root.social.communityOpen ? 49 : 20; y: 12; width: people.width-x-16
            text: root.faceIndex===1 ? root.social.communityTitle : "Messages"
            font.family: Theme.displayFamily; font.pixelSize: 20; color: Theme.ink; elide: Text.ElideRight
            MouseArea { anchors.fill: parent; enabled: root.social.communityOpen; onClicked: {root.anchorMenu(parent);root.social.communityInfo()} }
        }
        SocialIconButton {
            x: people.x+people.width-30; y: 6; tint: Theme.blue
            highlighted: root.social.focusIndex<0 && !root.social.reading && !root.social.communityOpen
            icon: root.faceIndex===0 ? "magnifying-glass" : root.social.communityOpen ? "info" : "plus"
            label: root.faceIndex===0 ? "Find a person" : root.social.communityOpen ? "Community info" : "Create"
            onClicked: {root.anchorMenu(this);if(root.faceIndex===0)root.social.filterPeople();else if(root.social.communityOpen)root.social.communityInfo();else root.social.collectionOptions()}
        }
        Item { id: filterChip; x: 20; y: 42; width: people.width-16; height: visible?29:0
            visible: root.faceIndex===0 && !!root.social.peopleFilter
            Text { width: parent.width-30; anchors.verticalCenter: parent.verticalCenter; text: "Filter: "+root.social.peopleFilter; color: Theme.muted; font.pixelSize: 12; elide: Text.ElideRight; textFormat: Text.PlainText }
            SocialIconButton { anchors.right: parent.right; width: 28; height: 28; icon: "x"; label: "Show all people"; onClicked: root.social.clearPeopleFilter() }
        }
        ListView {
            id: people; objectName: "social-people"
            x: 12; y: filterChip.visible ? 77 : 51; width: parent.width*.29; height: parent.height-y-29
            clip: true; spacing: 2; model: root.social.rows; currentIndex: root.social.focusIndex
            onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
            section.property: "section"
            section.delegate: Item {
                required property string section; width: people.width; height: section ? 25 : 0
                Text { x: 9; anchors.verticalCenter: parent.verticalCenter; text: section; font.pixelSize: 11; font.bold: true; color: Theme.muted; font.letterSpacing: .7 }
            }
            delegate: Item {
                required property var modelData; required property int index
                width: people.width; height: 59
                Rectangle { anchors.fill: parent; anchors.margins: 2; radius: 10
                    color: index===people.currentIndex ? Qt.rgba(Theme.rim.r,Theme.rim.g,Theme.rim.b,.16) : mouse.containsMouse ? "#0c193f3b" : "transparent"
                    border.width: index===people.currentIndex && !root.social.reading && !root.social.menu.length ? 1 : 0
                    border.color: Qt.rgba(Theme.chassis.r,Theme.chassis.g,Theme.chassis.b,.45)
                }
                Rectangle { x: 57; y: parent.height-1; width: parent.width-x-12; height: 1; color: Theme.chassis; opacity: .1 }
                MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; onClicked: root.social.activate(index) }
                AccountAvatar { x: 8; y: 9; width: 37; name: modelData.name; source: root.visible ? modelData.avatar || "" : ""; presence: modelData.presence || ""
                    MouseArea { anchors.fill: parent; enabled: modelData.kind==="person"; onClicked: root.social.profile(modelData.id) }
                }
                Column { x: 57; y: 11; width: parent.width-x-29; spacing: 4
                    Text { width: parent.width; text: (modelData.pinned ? "· " : "")+modelData.name; color: Theme.ink; font.pixelSize: 15; font.bold: !!modelData.unread; elide: Text.ElideRight; textFormat: Text.PlainText }
                    Text { width: parent.width; text: modelData.marker || modelData.detail || (modelData.kind==="person" ? "Say hello" : ""); color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight; textFormat: Text.PlainText }
                }
                SocialIconButton { anchors { right: parent.right; verticalCenter: parent.verticalCenter } width: 28; icon: "dots-three"; label: "Options for "+modelData.name
                    onClicked: {root.anchorMenu(this);root.social.contextRow(index)} }
            }
        }
        Text { anchors.right: parent.right; anchors.rightMargin: 20; y: parent.height-12; text: "Powered by Fluxer"; font.pixelSize: 9; color: Theme.muted; opacity: .65 }
        Text { x: people.x+8; y: people.y+48; width: people.width-16; visible: !people.count
            text: root.faceIndex===1 ? root.social.communityOpen ? "No text channels available." : "Your groups and communities will appear here." : root.social.peopleFilter ? "No matching people" : "Friends and people you talk to will appear here."
            wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; color: Theme.muted; font.pixelSize: 14 }
        ConversationPane {
            anchors { left: people.right; leftMargin: 29; right: parent.right; rightMargin: 20; top: parent.top; topMargin: 6; bottom: parent.bottom; bottomMargin: 4 }
            visible: root.social.conversation
            social: root.social; active: root.connected && root.faceIndex < 2
            onMenuAnchored: item => root.anchorMenu(item)
        }
        Column { visible: !root.social.conversation; x: people.width + 50; width: parent.width - x - 30; anchors.verticalCenter: parent.verticalCenter; spacing: 12
            Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; width: parent.width; text: root.faceIndex === 1 ? "A place for your people" : "A little closer"; font.family: Theme.displayFamily; font.pixelSize: 25; color: Theme.ink; horizontalAlignment: Text.AlignHCenter }
            Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; width: parent.width; text: root.account.status && root.account.status!=="Connected" ? root.account.status : root.faceIndex===1 ? "Choose a group or community to join the conversation." : "Choose someone on the left, or find new friends in Discover."; font.pixelSize: 15; color: Theme.muted; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap }
        }
    }
    DiscoverPane {
        anchors { top: heading.bottom; bottom: parent.bottom; left: parent.left; right: parent.right }
        visible: root.connected && root.faceIndex === 2
        social: root.social; takesFocus: root.takesFocus && visible
    }
    Rectangle {
        visible: root.social.menu.length > 0; anchors.fill: parent; color: "#6630433b"; z: 9
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
                Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; visible: text.length > 0; width: parent.width; text: root.social.menuDetail; color: Theme.muted; font.pixelSize: 13; wrapMode: Text.Wrap; maximumLineCount: root.social.fullMenuDetail ? 12 : 2; elide: Text.ElideRight; textFormat: Text.PlainText }
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
                        }
                    }
                    Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; visible: !!text; width: parent.width; text: root.social.person.bio || ""; color: Theme.ink; font.pixelSize: 13; wrapMode: Text.Wrap; maximumLineCount: 5; elide: Text.ElideRight; textFormat: Text.PlainText }
                    Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; visible: !!text; width: parent.width; text: root.social.person.status || ""; color: Theme.muted; font.pixelSize: 12; wrapMode: Text.Wrap; textFormat: Text.PlainText }
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
                    objectName: "social-menu-action-" + index
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
