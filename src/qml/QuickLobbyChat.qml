import QtQuick

Rectangle {
    id: root
    objectName: "player-mini-chat"
    required property var controller
    required property var social
    required property var presentation
    enabled: presentation.ready
    radius: 16
    color: "#ed182326"
    border.width: 1
    border.color: "#586e70"

    component ChatButton: Item {
        id: button
        property string glyph
        property string label
        signal clicked()
        width: 36; height: 36
        opacity: enabled ? 1 : .32
        Accessible.role: Accessible.Button
        Accessible.name: label
        Accessible.onPressAction: if (enabled) clicked()
        Rectangle { anchors.fill: parent; radius: 9; color: pointer.pressed ? "#34474b" : pointer.containsMouse ? "#283a3e" : "transparent" }
        MenuGlyph { anchors.centerIn: parent; width: 18; height: 18; glyph: button.glyph; ink: "#cbdad8" }
        MouseArea { id: pointer; anchors.fill: parent; hoverEnabled: true; onClicked: button.clicked() }
    }

    Text {
        x: 19; y: 15; width: parent.width-68
        text: root.controller.name || "Lobby chat"
        elide: Text.ElideRight; textFormat: Text.PlainText
        color: "#e4ece6"; font.family: Theme.displayFamily; font.pixelSize: 20
    }
    ChatButton { anchors.right: parent.right; anchors.rightMargin: 8; y: 8; glyph: "close"; label: "Return to game"; onClicked: root.controller.close() }
    Rectangle { x: 18; y: 48; width: parent.width-36; height: 1; color: "#284e6769" }

    ListView {
        id: messages
        x: 16; y: 61; width: parent.width-32; height: Math.max(0,composer.y-y-12)
        clip: true; spacing: 12
        visible: root.controller.conversationReady; model: visible ? root.social.messages : []
        onCountChanged: Qt.callLater(positionViewAtEnd)
        delegate: Item {
            required property var modelData
            width: messages.width; height: bubble.height
            Rectangle {
                id: bubble
                x: modelData.mine ? parent.width-width : 0
                width: Math.min(parent.width-26,Math.max(author.implicitWidth,body.implicitWidth)+22)
                height: body.height+author.height+18; radius: 10
                color: modelData.mine ? "#643a5256" : "#283f4b4e"
                Text {
                    id: author; x: 11; y: 7; width: parent.width-22
                    text: modelData.name; font.pixelSize: 10
                    color: modelData.mine ? "#a7c6b1" : "#a6c4d4"
                    elide: Text.ElideRight; textFormat: Text.PlainText
                }
                Text {
                    id: body; x: 11; y: author.y+author.height+3; width: parent.width-22
                    text: modelData.text || (modelData.attachments && modelData.attachments.length ? "File · available in Lobby" : "")
                    font.pixelSize: 14; wrapMode: Text.Wrap; color: "#e1e8e4"; textFormat: Text.PlainText
                }
            }
        }
        Timer {
            interval: 1200; repeat: true
            running: root.visible && root.controller.conversationReady && messages.atYEnd
            onTriggered: if(root.social.account.readTail)root.controller.presented(root.social.account.readTail)
        }
    }
    Text { anchors.centerIn: parent; visible: !root.controller.conversationReady; text: "Opening room chat…"; color: "#a5b7b5"; font.pixelSize: 14 }
    Rectangle {
        id: composer
        x: 14; anchors.bottom: parent.bottom; anchors.bottomMargin: 14
        width: parent.width-28; height: 41; radius: 10; color: "#ac101b1e"; border.color: "#4b6264"
        Text {
            x: 12; anchors.verticalCenter: parent.verticalCenter; width: parent.width-57
            text: root.social.draft || "Write a message…"; color: root.social.draft ? "#e1e8e4" : "#97aaa9"
            font.pixelSize: 14; elide: Text.ElideRight; textFormat: Text.PlainText
        }
        MouseArea { x: 0; y: 0; width: parent.width-42; height: parent.height; onClicked: root.controller.compose() }
        ChatButton {
            anchors.right: parent.right; anchors.rightMargin: 3; anchors.verticalCenter: parent.verticalCenter
            glyph: "send"; label: "Send"; enabled: root.controller.conversationReady && !!root.social.draft.trim()
            onClicked: root.controller.send()
        }
    }
}
