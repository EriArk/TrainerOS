import QtQuick

Item {
    id: root
    required property var shell
    readonly property var device: shell.device
    readonly property bool takesFocus: visible && !shell.menuOpen && shell.notice.length === 0
    // The main chassis owns the recessed surface for pages and services alike.
    Item {
        anchors.fill: parent; anchors.margins: Theme.panelInset
        anchors.topMargin: Theme.contentTopInset
        PageHeader { id: deviceHeader; compact: true; title: "Your handheld"; subtitle: "D-pad adjusts volume and brightness in steps of 5%." }
        MountedPanel {
            x: 0; y: deviceHeader.height; width: parent.width; height: parent.height - y; color: "#d4e2d6"
            Column {
                x: 24; y: 12; spacing: 7
                Repeater {
                    model: root.device.rows
                    delegate: CapButton {
                        required property int index; required property var modelData
                        objectName: "device-" + index
                        width: 532; height: 42; textSize: 15; label: modelData.title; detail: modelData.value
                        tint: index < 2 ? Theme.blue : index >= 3 && index <= 4 ? Theme.pink : Theme.yellow
                        selected: root.takesFocus && root.device.focusIndex === index
                        onActivated: root.shell.activate(index)
                    }
                }
            }
            Column {
                x: 586; y: 15; spacing: 18
                Repeater {
                    model: root.device.status
                    delegate: Item {
                        required property var modelData
                        width: 284; height: 52
                        Text { text: modelData.title; color: Theme.muted; font.pixelSize: 14; font.weight: Font.DemiBold }
                        Text { y: 24; width: parent.width; text: modelData.value; color: Theme.ink; font.pixelSize: 17; wrapMode: Text.WordWrap }
                    }
                }
                Text { width: 284; text: root.device.busy ? "Updating…" : root.device.error; color: "#853b24"; font.pixelSize: 14; wrapMode: Text.WordWrap }
            }
        }
    }
}
