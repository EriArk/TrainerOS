import QtQuick

Panel {
    id: root
    required property var shell
    patterned: false
    readonly property bool inbox: shell.notificationsOpen
    Text { x: 25; y: 19; text: "Options"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 29 }
    SocialIconButton { anchors.right: parent.right; anchors.rightMargin: 20; y: 16; width: 38; height: 38; icon: "x"; label: "Close Options"; onClicked: root.shell.closeHomeMenu() }
    Text { x: 26; y: 59; text: root.shell.homeMenuCaption; color: Theme.muted; font.pixelSize: 15 }
    MountedPanel {
        id: shortcuts
        x: 18; y: 91; width: Math.max(240,root.width * (Theme.television ? 0.44 : 0.34)); height: root.height - y - 20
        color: "#d5e5de"
        ListView {
            id: actions
            anchors.fill: parent; anchors.margins: 12; spacing: 8; clip: true
            model: root.shell.homeMenuActions; currentIndex: root.shell.homeMenuFocus
            onCurrentIndexChanged: if(currentIndex >= 0)positionViewAtIndex(currentIndex,ListView.Contain)
            delegate: CapButton {
                required property int index
                required property var modelData
                width: actions.width; height: Theme.television ? 66 : modelData.detail ? 55 : 44
                label: modelData.label; detail: modelData.detail || ""; textSize: Theme.television ? 22 : 18
                tint: modelData.id === "profile" ? Theme.green : Theme.blue
                selected: !root.inbox && actions.currentIndex === index
                enabled: modelData.enabled !== false
                onActivated: root.shell.activateHomeAction(modelData.id)
            }
        }
    }
    MountedPanel {
        x: shortcuts.x + shortcuts.width + 16; y: shortcuts.y
        width: parent.width - x - 18; height: shortcuts.height
        color: "#eee5d1"
        Text { x: 20; y: 16; text: "Notifications"; font.family: Theme.displayFamily; font.pixelSize: 22; color: Theme.ink }
        ListView {
            id: notices
            x: 12; y: 56; width: parent.width - 24; height: parent.height - y - 12
            clip: true; spacing: 8; model: root.shell.notifications
            currentIndex: root.inbox ? root.shell.notificationFocus : -1
            onCurrentIndexChanged: if(currentIndex>=0)positionViewAtIndex(currentIndex,ListView.Contain)
            delegate: CapButton {
                required property var modelData
                required property int index
                width: notices.width; height: Theme.television ? 85 : 64; claimsFocus: false
                label: modelData.name; detail: modelData.detail; textSize: Theme.television ? 22 : 18
                selected: root.inbox && notices.currentIndex === index
                tint: modelData.request ? Theme.pink : Theme.blue
                onActivated: root.shell.openNotificationById(modelData.id)
            }
        }
        Column {
            anchors.centerIn: parent; width: parent.width - 40; spacing: 9
            visible: notices.count === 0
            Text { width: parent.width; text: "All caught up"; horizontalAlignment: Text.AlignHCenter; font.family: Theme.displayFamily; font.pixelSize: 24; color: Theme.ink }
            Text { width: parent.width; text: "Messages, calls and invitations appear here."; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 16; color: Theme.muted }
        }
    }
}
