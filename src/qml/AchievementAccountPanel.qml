import QtQuick

Item {
    id: root
    required property var shell
    readonly property var account: shell.hall.account
    readonly property bool takesFocus: visible && !shell.menuOpen && !shell.keyboard.open && shell.notice.length === 0
    Panel { anchors.fill: parent }
    Item {
        anchors.fill: parent; anchors.margins: Theme.panelInset
        PageHeader { y: 6; compact: true; title: "RetroAchievements"; subtitle: root.account.status; multilineStatus: true }
        MountedPanel {
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
