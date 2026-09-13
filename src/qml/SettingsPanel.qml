import QtQuick

Item {
    id: root
    required property var shell
    readonly property var settings: shell.settings
    readonly property bool takesFocus: visible && !shell.menuOpen && shell.notice.length === 0
    Panel { anchors.fill: parent }
    Item {
        anchors.fill: parent; anchors.margins: Theme.panelInset
        anchors.topMargin: Theme.contentTopInset
        Text { x: 30; y: 24; text: "Make it yours"; color: Theme.ink; font.pixelSize: 34; font.weight: Font.DemiBold }
        Text { x: 31; y: 74; text: "Same familiar controls, your favorite color."; color: Theme.muted; font.pixelSize: 17 }
        Rectangle {
            x: 0; y: 118; width: parent.width; height: parent.height - y; color: "#d4e2d6"
            Column {
                x: 30; y: 18; spacing: 8
                Repeater {
                    model: root.settings.rows
                    delegate: CapButton {
                        required property int index; required property var modelData
                        objectName: "settings-" + index
                        width: 610; height: 50; label: modelData.title; detail: modelData.value
                        tint: index === 0 ? Theme.yellow : index === 1 ? Theme.blue : Theme.pink
                        selected: root.takesFocus && root.settings.focusIndex === index
                        onActivated: root.shell.activate(index)
                    }
                }
            }
            Text { x: 32; y: parent.height - 33; text: root.settings.saving ? "Saving…" : root.settings.error; color: "#853b24"; font.pixelSize: 14 }
        }
    }
}
