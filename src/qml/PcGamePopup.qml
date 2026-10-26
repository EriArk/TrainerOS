import QtQuick

Item {
    id: root
    required property var flow
    readonly property bool choosingGame: flow.stage === "candidates"
    readonly property bool confirming: flow.stage === "confirm"
    readonly property bool preview: confirming || flow.stage === "appearance" || flow.stage === "settings"
    readonly property int primaryAction: confirming ? 2 : flow.stage === "settings" ? 4 : -1
    readonly property var visibleRows: {
        let result = []
        for(let i = 0; i < flow.rows.length; ++i) {
            if(i === primaryAction) continue
            result.push(Object.assign({}, flow.rows[i], { actionIndex: i }))
        }
        return result
    }
    Rectangle { anchors.fill: parent; color: "#99102022" }
    MouseArea { anchors.fill: parent }
    Rectangle {
        id: panel
        anchors.centerIn: parent
        width: Math.min(740, parent.width - 40)
        height: Math.min(root.choosingGame ? 486 : Math.max(288, 141 + message.height + Math.min(root.flow.rows.length, 4) * 64), parent.height - 36)
        radius: 22; color: Theme.surface
        border.width: 1; border.color: Theme.divider
        Text {
            x: 26; y: 23; width: parent.width - 88; elide: Text.ElideRight
            text: root.flow.editing ? root.flow.title : root.confirming ? "Make it yours" : "Add a PC game"
            font.family: Theme.displayFamily; font.pixelSize: 27; color: Theme.ink
        }
        SocialIconButton { anchors.right: parent.right; anchors.rightMargin: 19; y: 18; icon: "x"; label: "Close"; onClicked: root.flow.close() }
        Text {
            id: message
            x: 27; y: 69; width: parent.width - 54; height: Math.min(84, implicitHeight)
            text: root.flow.message || (root.flow.editing ? "Launch, controls and presentation for this game." : "Install something new, or bring a game you already have.")
            textFormat: Text.PlainText; wrapMode: Text.Wrap; elide: Text.ElideRight
            font.pixelSize: 15; color: Theme.muted; maximumLineCount: 4
        }
        Rectangle {
            x: 27; y: message.y + message.height + 17; width: parent.width - 54; height: 2
            color: Theme.divider
            Rectangle { width: parent.width * Math.max(0, Math.min(1, root.flow.progress)); height: 2; color: Theme.accent; visible: root.flow.busy }
        }
        ListView {
            id: choices
            x: root.preview ? 190 : 18; y: message.y + message.height + 29; width: parent.width - x - 18
            height: parent.height - y - 43
            clip: true; spacing: 2; boundsBehavior: Flickable.StopAtBounds
            model: root.visibleRows
            currentIndex: root.visibleRows.findIndex(function(row) { return row.actionIndex === root.flow.focusIndex })
            function reveal() { if(count && currentIndex >= 0)positionViewAtIndex(currentIndex,ListView.Contain) }
            onCurrentIndexChanged: Qt.callLater(reveal)
            onModelChanged: Qt.callLater(reveal)
            delegate: Rectangle {
                required property int index
                required property var modelData
                width: choices.width; height: root.choosingGame ? 58 : 62; radius: 10
                opacity: modelData.enabled === false ? 0.45 : 1
                color: index === choices.currentIndex ? Theme.selection : "transparent"
                Rectangle { x: 0; y: 14; height: parent.height - 28; width: 3; radius: 1.5; color: Theme.accent; visible: index === choices.currentIndex }
                Text { x: 15; y: 9; width: parent.width - 52; text: modelData.title; textFormat: Text.PlainText; elide: Text.ElideRight; color: Theme.ink; font.pixelSize: 17 }
                Text { x: 15; y: 33; width: parent.width - 52; text: modelData.subtitle || ""; textFormat: Text.PlainText; elide: Text.ElideMiddle; color: Theme.muted; font.pixelSize: 12 }
                Text { anchors.right: parent.right; anchors.rightMargin: 15; anchors.verticalCenter: parent.verticalCenter; text: "›"; color: Theme.accent; font.pixelSize: 24 }
                MouseArea { anchors.fill: parent; onClicked: root.flow.activate(modelData.actionIndex) }
            }
        }
        Item {
            x: 27; y: choices.y + 8; width: 144; height: Math.min(208, choices.height - 8); visible: root.preview
            Rectangle { anchors.fill: parent; radius: 8; color: Theme.selection; border.color: Theme.divider }
            Text { anchors.centerIn: parent; text: "PC"; font.family: Theme.displayFamily; font.pixelSize: 38; color: Theme.muted; visible: !root.flow.artwork }
            Image { anchors.fill: parent; source: root.flow.artwork; fillMode: Image.PreserveAspectFit; asynchronous: true; sourceSize.width: 300; sourceSize.height: 420 }
        }
        Text {
            x: 27; anchors.bottom: parent.bottom; anchors.bottomMargin: 15
            text: Theme.buttonLabel("A") + "  Select     " + Theme.buttonLabel("B") + (root.flow.editing && root.flow.stage !== "settings" ? "  Back" : "  Close")
            color: Theme.muted; font.pixelSize: 12
        }
        Rectangle {
            anchors.right: parent.right; anchors.rightMargin: 22; anchors.bottom: parent.bottom; anchors.bottomMargin: 9
            width: 132; height: 32; radius: 8; visible: root.primaryAction >= 0
            color: root.flow.focusIndex === root.primaryAction ? Theme.selection : "transparent"
            Text { anchors.centerIn: parent; text: root.confirming ? "Add to Games" : "Save changes"; font.pixelSize: 15; color: Theme.accent }
            MouseArea { anchors.fill: parent; onClicked: root.flow.activate(root.primaryAction) }
        }
    }
    ChatFilePicker {
        anchors.fill: parent; picker: root.flow.picker
        title: root.flow.stage === "storage" ? "Choose storage" : root.flow.picker.choosingFolder ? "Choose folder" : "Choose file"
    }
}
