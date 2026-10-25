import QtQuick

Item {
    id: root
    required property var shell
    readonly property var flow: shell.settings.communication
    readonly property var account: shell.social.account
    readonly property var profile: account.profile || ({})
    readonly property bool signedIn: !!account.userId
    visible: shell.service === "account-profile"
    objectName: "account-profile-overlay"
    Rectangle { anchors.fill: parent; color: Theme.surfaceColor("#80132224") }
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onWheel: wheel => wheel.accepted=true }
    Rectangle { x: card.x-3; y: card.y+5; width: card.width+6; height: card.height+4; radius: 22; color: Theme.surfaceColor("#35142120") }
    Rectangle {
        id: card; objectName: "account-profile-card"
        anchors.centerIn: parent; width: Math.min(430,parent.width-44); height: Math.min(440,parent.height-28)
        radius: 18; color: Theme.paper; border.color: Theme.rim; border.width: 2
        Rectangle { anchors.fill: parent; anchors.margins: 4; radius: 14; color: "transparent"; border.color: Theme.surfaceColor("#80ffffff") }
        AccountAvatar { x: 22; y: 22; width: 62; name: root.account.name || "Player"; source: root.profile.avatar || ""; presence: root.account.state === "connected" ? root.account.presence || "" : "" }
        Column {
            x: 98; y: 24; width: parent.width-x-85; spacing: 4
            Text { width: parent.width; text: root.signedIn ? root.profile.name || root.account.name || "Your profile" : "Welcome"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 23; elide: Text.ElideRight; textFormat: Text.PlainText }
            Text { width: parent.width; text: root.profile.tag || "Sign in to Fluxer"; color: Theme.muted; font.pixelSize: 12; elide: Text.ElideRight; textFormat: Text.PlainText }
        }
        SocialIconButton { objectName: "account-profile-edit"; x: parent.width-80; y: 20; icon: "wrench"; label: root.flow.profileEditing ? "Back to profile" : "Edit profile"; visible: root.signedIn && !root.flow.choosingAvatar; highlighted: root.flow.profileEditing; onClicked: root.flow.editProfile() }
        SocialIconButton { objectName: "account-profile-close"; x: parent.width-43; y: 20; icon: "x"; label: "Back"; onClicked: root.shell.pressButton("B") }
        Text {
            x: 22; y: 98; width: parent.width-44; height: 38
            text: root.flow.choosingAvatar ? "Choose a picture" : root.flow.profileEditing ? "Make yourself at home" : root.profile.bio || (root.signedIn ? "How would you like to appear?" : "Your friends and profile, wherever you play.")
            color: Theme.muted; font.pixelSize: 13; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight; textFormat: Text.PlainText
        }
        ListView {
            id: actions; objectName: "account-profile-actions"
            x: 18; y: 141; width: parent.width-36; height: parent.height-y-40
            model: root.visible ? root.flow.rows : []; currentIndex: root.flow.focusIndex; spacing: 3; clip: true
            onCurrentIndexChanged: if(currentIndex>=0)positionViewAtIndex(currentIndex,ListView.Contain)
            delegate: Rectangle {
                required property int index; required property var modelData
                objectName: "account-profile-action-"+index
                width: actions.width; height: modelData.kind === "presence" ? 32 : 44; radius: 9
                color: root.flow.focusIndex===index && !root.shell.keyboard.open ? Theme.surfaceColor("#f8e7b2") : pointer.containsMouse ? Theme.surfaceColor("#e9efe0") : "transparent"
                border.color: Theme.surfaceColor("#d8c48d"); border.width: root.flow.focusIndex===index && !root.shell.keyboard.open ? 1 : 0
                Rectangle { visible: modelData.kind === "presence"; x: 12; anchors.verticalCenter: parent.verticalCenter; width: 10; height: 10; radius: 5; color: modelData.presence === "online" ? Theme.surfaceColor("#70af72") : modelData.presence === "idle" ? Theme.yellow : modelData.presence === "dnd" ? Theme.pink : Theme.surfaceColor("#a0aaa4") }
                Image { visible: modelData.kind === "picture"; x: 7; y: 3; width: 38; height: 38; source: modelData.image || ""; fillMode: Image.PreserveAspectFit; asynchronous: true }
                Text { x: modelData.kind === "picture" ? 54 : modelData.kind === "presence" ? 32 : 12; y: modelData.kind === "presence" ? 5 : 3; width: parent.width-x-26; text: modelData.title; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 16; elide: Text.ElideRight; textFormat: Text.PlainText }
                Text { visible: modelData.kind !== "presence"; x: 12; y: 24; width: parent.width-30; text: modelData.detail || ""; color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight; textFormat: Text.PlainText }
                Text { anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: modelData.selected ? "✓" : "›"; color: Theme.muted; font.pixelSize: 17 }
                MouseArea { id: pointer; anchors.fill: parent; hoverEnabled: true; onClicked: root.flow.activate(index) }
                Accessible.role: Accessible.Button; Accessible.name: modelData.title; Accessible.onPressAction: root.flow.activate(index)
            }
        }
        Text { x: 22; y: parent.height-34; width: parent.width-44; text: root.flow.status; color: Theme.muted; font.pixelSize: 12; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight; textFormat: Text.PlainText }
    }
}
