import QtQuick

Panel {
    id: root
    required property var shell
    readonly property bool inbox: shell.notificationsOpen
    readonly property var live: shell.liveGame
    radius: 30
    surface: Qt.tint(Theme.paper,"#09c29948")
    patterned: true
    Text { x: 27; y: 20; text: "Now"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 29 }
    Text { x: 103; y: 32; text: root.shell.homeMenuCaption; color: Theme.muted; font.pixelSize: 14 }
    SocialIconButton { anchors.right: parent.right; anchors.rightMargin: 20; y: 19; icon: "x"; label: "Close"; onClicked: root.shell.closeHomeMenu() }
    Item {
        id: activity; x: 25; y: 77; width: (parent.width-68)*.49; height: parent.height-y-25
        Rectangle { anchors.fill: parent; radius: 16; color: "#b5fffef9"; border.color: "#6694a899" }
        Text { id: title; x: 17; y: 15; width: parent.width-34; text: root.live.title || (root.shell.social.homeLobby.title ? "Together" : "Your activity"); color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 23; elide: Text.ElideRight; textFormat: Text.PlainText }
        ListView { id: actions; x: 12; y: 61; width: parent.width-24; height: parent.height-y-12; clip: true; spacing: 7
            model: root.shell.homeMenuActions; currentIndex: root.inbox ? -1 : root.shell.homeMenuFocus
            onCurrentIndexChanged: if(currentIndex>=0)positionViewAtIndex(currentIndex,ListView.Contain)
            delegate: ActivityRow { required property int index; required property var modelData
                width: actions.width; label: modelData.label; detail: modelData.detail || ""; selected: !root.inbox && root.shell.homeMenuFocus===index
                glyph: modelData.id==="call" || modelData.id.indexOf("voice-")===0 ? "speaker" : modelData.id==="live-options" ? "settings" : modelData.id==="live-return" ? "play" : "party"
                accent: modelData.id==="live-return"?Theme.green:Theme.blue; enabled: modelData.enabled!==false
                onActivated: root.shell.activateHomeAction(modelData.id)
            }
        }
        Column { anchors.centerIn: parent; width: parent.width-56; spacing: 16; visible: actions.count===0
            MenuGlyph { anchors.horizontalCenter: parent.horizontalCenter; width: 46; height: 46; glyph: "play"; ink: Theme.rim }
            Text { width: parent.width; text: "A little breathing room"; font.family: Theme.displayFamily; font.pixelSize: 24; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; color: Theme.ink }
            Text { width: parent.width; text: "Your running game, lobby and call will be here."; font.pixelSize: 15; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; color: Theme.muted }
        }
    }
    Item { x: activity.x+activity.width+18; y: activity.y; width: parent.width-x-25; height: activity.height
        Text { x: 10; y: 15; text: "Notifications"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 23 }
        CapButton { anchors.right: parent.right; y: 12; width: 104; height: 29; label: "Clear updates"; textSize: 12; centered: true; claimsFocus: false; tint: Theme.blue; visible: root.shell.notifications.length>0; onActivated: root.shell.dismissNotifications() }
        ListView { id: notices; x: 2; y: 61; width: parent.width-4; height: parent.height-y; clip: true; spacing: 9
            model: root.shell.notifications; currentIndex: root.inbox?root.shell.notificationFocus:-1
            onCurrentIndexChanged: if(currentIndex>=0)positionViewAtIndex(currentIndex,ListView.Contain)
            delegate: Item { required property int index; required property var modelData
                width: notices.width; height: 59
                ActivityRow { width: parent.width-36; height: parent.height; label: modelData.name; detail: modelData.detail || ""; glyph: modelData.request?"party":"chat"; accent: modelData.request?Theme.pink:Theme.blue
                    selected: root.inbox && root.shell.notificationFocus===index; onActivated: root.shell.openNotificationById(modelData.id)
                }
                SocialIconButton { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; icon: "x"; label: "Remove notification"; visible: !modelData.ringing && !modelData.pending; onClicked: root.shell.dismissNotificationById(modelData.id) }
            }
        }
        Text { anchors.centerIn: parent; visible: notices.count===0; text: "All caught up"; color: Theme.muted; font.family: Theme.displayFamily; font.pixelSize: 22 }
    }
}
