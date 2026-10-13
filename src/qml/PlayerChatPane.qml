import QtQuick

Panel {
    id: root
    objectName: "player-mini-chat"
    required property var controller
    required property var social
    required property var presentation
    patterned: false
    enabled: presentation.ready
    Text { x: 16; y: 12; width: parent.width - 70; text: "Playing together"; elide: Text.ElideRight; textFormat: Text.PlainText; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 20 }
    SocialIconButton { anchors.right: parent.right; anchors.rightMargin: 10; y: 7; icon: "x"; label: "Return to game"; tint: Theme.pink; onClicked: root.controller.close() }
    ConversationPane {
        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: footer.top; margins: 14; topMargin: 47 }
        social: root.social; compact: true; active: root.visible && root.controller.conversationReady
        visible: root.controller.conversationReady
    }
    Text { anchors.centerIn: parent; width: parent.width - 40; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; text: root.social.account.status && root.social.account.status !== "Connected" ? root.social.account.status : "Opening conversation…"; visible: !root.controller.conversationReady; color: Theme.muted; font.pixelSize: 15; textFormat: Text.PlainText }
    Row { id: footer; anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 12 } spacing: 18
        Hint { button: "A"; label: "Write"; labelColor: Theme.ink }
        Hint { button: "X"; label: "Send"; labelColor: Theme.ink }
        Hint { button: "B"; label: "Play"; labelColor: Theme.ink }
    }
}
