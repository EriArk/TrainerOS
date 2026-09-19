import QtQuick

Rectangle {
    id: root
    required property var picker
    required property bool takesFocus
    color: Theme.paper
    ShellBackgroundPattern { anchors.fill: parent }
    PageHeader { id: pickerHeader; compact: true; title: "Choose your favorite Pokémon"; subtitle: root.picker.query ? "Search · " + root.picker.query : "One companion for your Trainer card" }
    MountedPanel {
        x: 0; y: pickerHeader.height; width: parent.width; height: 326 - y; color: "#d8e5d8"
        ListView {
            id: list
            x: 25; y: 8; width: parent.width - 50; height: 216
            clip: true; interactive: false; keyNavigationEnabled: false
            model: root.picker.entries; currentIndex: root.picker.focusIndex
            function reveal() {
                if (count && root.visible) positionViewAtIndex(currentIndex, ListView.Contain)
                if (root.takesFocus && currentItem) currentItem.control.forceActiveFocus(Qt.OtherFocusReason)
            }
            onCurrentIndexChanged: Qt.callLater(reveal)
            onCurrentItemChanged: Qt.callLater(reveal)
            onModelChanged: Qt.callLater(reveal)
            Connections { target: root; function onTakesFocusChanged() { Qt.callLater(list.reveal) } }
            delegate: Item {
                required property int index
                required property var modelData
                width: list.width; height: 54
                property alias control: button
                CapButton {
                    id: button
                    objectName: "species-" + modelData.id
                    x: 5; y: 5; width: parent.width - 10; height: 42
                    label: modelData.number + "  " + modelData.name; tint: Theme.pink
                    contentInset: 65
                    ClassicIllustration { x: 15; y: 3; width: 36; height: 36; art: modelData.art || ({}) }
                    selected: root.takesFocus && root.picker.focusIndex === index
                    onActivated: root.picker.activate(index)
                }
            }
        }
        Text {
            x: 31; y: 54; width: parent.width - 62; visible: list.count === 0
            text: root.picker.error || "No Pokémon match this search. X changes the search; B keeps your current choice."
            color: Theme.muted; font.pixelSize: 20; wrapMode: Text.WordWrap
        }
    }
    MountedPanel {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 86; color: "#c6dcca"
        Text { x: 31; y: 6; text: list.count + " entries · ↑ / ↓ Browse · ← / → Jump 8 · A Choose"; color: Theme.muted; font.pixelSize: 12 }
        CapButton { objectName: "species-search"; x: 30; y: 32; width: 260; height: 40; label: "X Search"; selected: root.takesFocus && list.count === 0; onActivated: root.picker.requestSearch() }
        CapButton { x: 316; y: 32; width: 260; height: 40; label: "Y No favorite"; tint: Theme.blue; onActivated: root.picker.clearChoice() }
        CapButton { x: 601; y: 32; width: 285; height: 40; label: "B Keep current choice"; tint: Theme.green; onActivated: root.picker.cancel() }
    }
}
