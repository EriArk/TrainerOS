import QtQuick

Item {
    id: root
    required property var shell
    readonly property var account: shell.hall.account
    readonly property bool takesFocus: visible && !shell.menuOpen && !shell.keyboard.open && shell.notice.length === 0
    Panel { anchors.fill: parent }
    Item {
        anchors.fill: parent; anchors.margins: 10
        Text { x: 28; y: 14; text: "RetroAchievements"; color: Theme.ink; font.pixelSize: 30; font.weight: Font.DemiBold }
        Text {
            x: 29; y: 59; width: parent.width - 58; height: 65
            text: root.account.status; textFormat: Text.PlainText; wrapMode: Text.WordWrap
            color: Theme.muted; font.pixelSize: 16; maximumLineCount: 3; elide: Text.ElideRight
        }
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 266; color: "#d4e2d6"
            Column {
                x: 28; y: 14; spacing: 10
                Repeater {
                    model: root.account.rows
                    delegate: CapButton {
                        required property int index; required property var modelData
                        objectName: "achievement-account-" + index
                        width: 770; height: 50; label: modelData.label; detail: modelData.detail
                        tint: index === 0 ? Theme.blue : index === 1 ? Theme.pink : Theme.yellow
                        enabled: modelData.enabled
                        selected: root.takesFocus && root.account.focusIndex === index && enabled
                        onActivated: root.shell.activate(index)
                    }
                }
            }
        }
    }
}
