import QtQuick

Panel {
    id: card
    property string heading: "Quick access"
    property string caption: ""
    property var actions: []
    property int currentIndex: 0
    property bool ready: true
    signal chosen(int index)
    function restoreFocus() {
        const button = buttons.itemAt(currentIndex)
        if (visible && ready && button) button.claimFocus()
    }
    onReadyChanged: Qt.callLater(restoreFocus)
    onVisibleChanged: Qt.callLater(restoreFocus)
    patterned: false
    width: 390; height: 92 + actions.length * 58 + 15
    Rectangle { x: 3; y: parent.height - 3; width: parent.width - 6; height: 6; radius: 3; color: "#50121e20"; z: -1 }
    Text { x: 24; y: 17; width: parent.width - 48; height: 32; text: card.heading; textFormat: Text.PlainText; font.family: Theme.displayFamily; font.pixelSize: 25; color: Theme.ink; elide: Text.ElideRight }
    Text { x: 24; y: 54; width: parent.width - 48; text: card.ready ? card.caption : "Release the controls"; textFormat: Text.PlainText; font.pixelSize: 15; color: Theme.muted; elide: Text.ElideRight }
    MountedPanel {
        x: 10; y: 85; width: parent.width - 20; height: parent.height - 95; color: Theme.chassis
        Column {
            x: 10; y: 7; width: parent.width - 20; spacing: 10
            Repeater {
                id: buttons
                model: card.actions
                CapButton {
                    required property int index
                    required property var modelData
                    objectName: "home-action-" + modelData.id
                    width: parent.width; height: 48
                    label: modelData.label; textSize: 21
                    tint: modelData.id === "exit" ? Theme.pink : index === 0 ? Theme.yellow : Theme.blue
                    selected: card.currentIndex === index; deferredFocus: true
                    enabled: card.ready
                    onActivated: card.chosen(index)
                }
            }
        }
    }
}
