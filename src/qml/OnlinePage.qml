import QtQuick

Item {
    id: root
    required property var shell
    objectName: "online-page"
    readonly property bool chats: shell.onlineFace === "chats"
    Row {
        id: sections
        visible: root.chats
        height: 32; spacing: 24; x: 14
        Repeater {
            model: [{id:"chats",name:"Messages"},{id:"communities",name:"Communities"},{id:"friends",name:"Discover"}]
            delegate: Item {
                required property var modelData
                objectName: "online-chat-" + modelData.id
                width: label.implicitWidth + 12; height: 29
                Text {
                    id: label; anchors.centerIn: parent; text: modelData.name
                    font.pixelSize: 13; font.bold: root.shell.socialFace === modelData.id
                    color: root.shell.socialFace === modelData.id ? Theme.ink : Theme.muted
                }
                Rectangle {
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                    height: root.shell.chatNavigationFocused ? 3 : 1
                    color: Theme.focusGlow; visible: root.shell.socialFace === modelData.id
                }
                MouseArea { anchors.fill: parent; onClicked: root.shell.selectChatFace(modelData.id) }
            }
        }
    }
    SocialPage {
        anchors { fill: parent; topMargin: root.chats ? 34 : 0 }
        shell: root.shell
        visible: root.shell.onlineFace === "lobby" || root.chats
    }
    Item {
        anchors.fill: parent; visible: root.shell.onlineFace === "bazaar"
        Column {
            anchors.centerIn: parent; width: Math.min(parent.width - 64, 530); spacing: 18
            Text { width: parent.width; text: "Bazaar"; font.family: Theme.displayFamily; font.pixelSize: 32; color: Theme.ink }
            Rectangle { width: 52; height: 3; radius: 1; color: Theme.yellow }
            Text { width: parent.width; text: "A place to meet, explore and trade."; font.pixelSize: 18; color: Theme.ink; wrapMode: Text.WordWrap }
            Text { width: parent.width; text: "Worlds and player rooms are not available in this build yet."; font.pixelSize: 14; color: Theme.muted; wrapMode: Text.WordWrap }
        }
    }
}
