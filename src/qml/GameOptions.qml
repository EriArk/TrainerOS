import QtQuick

Panel {
    id: root
    required property var presentation
    patterned: false
    MountedPanel {
        id: artwork
        x: 18; y: 18; width: parent.width * (Theme.television ? .38 : .48); height: parent.height - 36
        color: Theme.chassisDark
        Image {
            anchors.fill: parent; anchors.margins: 9
            source: root.presentation.hasFrame ? "image://exit-frame/" + root.presentation.frameKey : ""
            fillMode: Image.PreserveAspectFit; cache: false
        }
        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: title.height + 40; color: "#df203f3e"; radius: 13 }
        Text { id: title; x: 20; anchors.bottom: parent.bottom; anchors.bottomMargin: 20; width: parent.width - 40
            text: root.presentation.gameTitle; color: "#fff2d9"; font.family: Theme.displayFamily; font.pixelSize: 29; wrapMode: Text.WordWrap }
    }
    Text { id: heading; x: artwork.x + artwork.width + 24; y: 23; width: parent.width - x - 24
        text: root.presentation.panel.length ? root.presentation.menuCaption : "Game Options"
        color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 25; wrapMode: Text.WordWrap }
    Text { id: status; x: heading.x; y: heading.y + heading.height + 9; width: heading.width
        text: root.presentation.ready ? "The game is still running" : "Release the controls"
        color: Theme.muted; font.pixelSize: 15 }
    ListView {
        id: actions
        x: heading.x; y: status.y + status.height + 16; width: heading.width; height: parent.height - y - 18
        clip: true; spacing: 8; model: root.presentation.menuActions; currentIndex: root.presentation.menuFocus
        onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
        delegate: Item {
            required property int index
            required property var modelData
            width: actions.width; height: button.height + (modelData.id === "exit" ? 14 : 0)
            Rectangle { visible: modelData.id === "exit"; width: parent.width; height: 1; color: "#8b9b8d" }
            CapButton {
                id: button; y: modelData.id === "exit" ? 14 : 0; width: parent.width; height: Theme.television ? 66 : modelData.detail ? 57 : 43
                label: modelData.label; detail: modelData.detail || ""; textSize: Theme.television ? 22 : 18
                tint: modelData.id === "exit" ? Theme.pink : modelData.id === "continue" ? Theme.green : Theme.blue
                selected: actions.currentIndex === index
                enabled: root.presentation.ready && modelData.readOnly !== true
                onActivated: root.presentation.activateAction(modelData.id)
            }
        }
    }
}
