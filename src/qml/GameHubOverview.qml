import QtQuick

Item {
    id: root
    required property var shell
    readonly property var game: shell.homeGame
    readonly property bool summary: shell.overviewFace === "summary"
    readonly property bool screens: shell.overviewFace === "screens"
    GenericExperiencePage {
        anchors.fill: parent; shell: root.shell; history: true
        visible: root.shell.overviewFace === "timeline"
    }
    Item {
        anchors.fill: parent; visible: root.summary || root.screens
        PageHeader {
            id: heading; width: parent.width; compact: true
            title: root.screens ? "Screens" : (root.game.title || "GameHub")
            subtitle: root.game.system || "Choose a game in Games"
        }
        Row {
            id: tabs; y: heading.height + 8; spacing: 10
            CapButton {
                objectName: "overview-timeline"
                width: 164; height: 35; textSize: 14; label: "Timeline"; tint: Theme.blue
                selected: root.visible && root.summary && root.shell.focusIndex === 0 && root.shell.unobstructed
                onActivated: root.shell.showOverview("timeline")
            }
            CapButton {
                objectName: "overview-screens"
                width: 164; height: 35; textSize: 14; label: "Screens"; tint: Theme.pink
                selected: root.visible && root.summary && root.shell.focusIndex === 1 && root.shell.unobstructed
                onActivated: root.shell.showOverview("screens")
            }
            CapButton {
                visible: root.screens; width: 120; height: 35; textSize: 14; label: "‹  Overview"
                onActivated: root.shell.showOverview("summary")
            }
        }
        MountedPanel {
            x: 0; y: tabs.y + tabs.height + 16
            width: parent.width; height: Math.max(0, parent.height-y); color: "#e0e8dd"
            Image {
                id: cover
                x: 14; y: 14; width: parent.width * .32; height: parent.height - 28
                visible: root.summary
                source: root.game.preview || ""; asynchronous: true; fillMode: Image.PreserveAspectFit
            }
            Column {
                visible: root.summary; x: cover.x + cover.width + 24; y: 18
                width: parent.width-x-20; spacing: 12
                Text { width: parent.width; text: root.game.title || "Your next adventure"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 25; wrapMode: Text.WordWrap }
                Text { width: parent.width; text: root.game.description || "Your recorded sessions and game moments stay together here."; color: Theme.muted; font.pixelSize: 16; wrapMode: Text.WordWrap; maximumLineCount: 5; elide: Text.ElideRight }
                Text { text: root.shell.home.recordedTime || "—"; color: Theme.ink; font.pixelSize: 20 }
            }
            Image {
                anchors.fill: parent; anchors.margins: 14; anchors.bottomMargin: 38
                visible: root.screens && !!root.shell.home.exitPreviewLabel
                source: visible ? root.shell.home.exitPreview : ""
                asynchronous: true; fillMode: Image.PreserveAspectFit
            }
            Text {
                visible: root.screens; anchors.horizontalCenter: parent.horizontalCenter
                y: root.shell.home.exitPreviewLabel ? parent.height-29 : parent.height/2-height/2
                text: root.shell.home.exitPreviewLabel || "No saved game moments yet"
                color: Theme.muted; font.pixelSize: 16
            }
        }
    }
}
