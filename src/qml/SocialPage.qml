import QtQuick

FocusScope {
    id: root
    required property var shell
    objectName: "social-empty"
    readonly property bool takesFocus: visible && !shell.serviceOpen && !shell.menuOpen && !shell.drawerOpen && !shell.keyboard.open && shell.notice.length === 0
    onTakesFocusChanged: if (takesFocus) forceActiveFocus()
    Component.onCompleted: if (takesFocus) forceActiveFocus()

    PageHeader {
        title: root.shell.socialFace === "friends" ? "Friends" : "Chats"
    }
    // Honest unlinked state until #99/#100 establish a supported user provider.
    // No fake friends, disabled setup button, polling or duplicate account store.
    Item {
        anchors.centerIn: parent
        width: 400; height: 190
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 5; width: 116; height: 92; radius: 24
            color: Theme.blue; border.color: Qt.darker(Theme.blue,1.6); border.width: 2
            Rectangle { x: 8; y: 6; width: 100; height: 3; radius: 1; color: "#70ffffff" }
            Row {
                anchors.centerIn: parent; spacing: 12
                Repeater { model: 3
                    Rectangle { width: 10; height: 10; radius: 5; color: Theme.ink }
                }
            }
        }
        Text {
            y: 126; width: parent.width
            text: "No account connected"
            horizontalAlignment: Text.AlignHCenter
            font.family: Theme.displayFamily; font.pixelSize: 26
            color: Theme.ink
        }
    }
}
