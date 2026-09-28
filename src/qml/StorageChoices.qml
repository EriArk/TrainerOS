import QtQuick

Item {
    id: root
    required property var flow
    property bool takesFocus: visible
    property bool compact: false
    ListView {
        id: list
        anchors.fill: parent; anchors.margins: 5
        model: root.flow.rows; currentIndex: root.flow.focusIndex
        clip: true; interactive: false; spacing: root.compact ? 7 : 10
        onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
        onCountChanged: positionViewAtIndex(currentIndex,ListView.Contain)
        delegate: CapButton {
            id: card
            required property int index; required property var modelData
            readonly property bool refresh: modelData.kind === "refresh" || modelData.label === "Refresh"
            width: list.width; height: refresh ? 40 : root.compact ? 62 : 72
            label: ""; tint: refresh ? "#d6e0d5" : index % 2 ? "#b8d6e2" : "#cee0b4"
            selected: root.takesFocus && !root.flow.busy && index===root.flow.focusIndex
            enabled: !root.flow.busy
            onActivated: root.flow.activate(index)
            Rectangle {
                x: 13; y: root.compact ? 6 : 11; width: 44; height: 49; radius: 6; visible: !card.refresh
                color: modelData.kind==="internal" ? "#5b8294" : "#587d68"; border.color: "#44625e"
                Rectangle { x: 6; y: 8; width: 32; height: 21; radius: 3; color: "#deeadc"
                    Text { anchors.centerIn: parent; text: modelData.kind==="internal" ? "INT" : "SD"; color: Theme.ink; font.family: Theme.brandFamily; font.pixelSize: 13 }
                }
                Row { x: 8; y: 36; spacing: 3; Repeater { model: 4; Rectangle { width: 5; height: 8; radius: 1; color: "#f2cf78" } } }
            }
            Text { x: card.refresh ? 15 : 72; y: 9; width: parent.width-x-70; text: modelData.label; textFormat: Text.PlainText; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: card.refresh ? 17 : 20; elide: Text.ElideRight }
            Text { x: 72; y: root.compact ? 31 : 34; width: parent.width-88; visible: !card.refresh; text: modelData.detail || ""; color: Theme.muted; font.pixelSize: 13 }
            Text { anchors.right: parent.right; anchors.rightMargin: 15; y: 13; visible: modelData.current===true; text: "IN USE"; color: "#426047"; font.bold: true; font.pixelSize: 9 }
            Rectangle { x: 72; y: root.compact ? 51 : 56; width: parent.width-90; height: 4; radius: 2; color: "#30557363"; visible: !card.refresh
                Rectangle { width: parent.width*(modelData.total > 0 ? Math.max(0,Math.min(1,1-modelData.available/modelData.total)) : 0); height: 4; radius: 2; color: "#769988" }
            }
        }
    }
}
