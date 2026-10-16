import QtQuick

Item {
    id: root
    required property var shell
    readonly property var game: shell.homeGame
    readonly property var sessions: shell.gameHistory
    readonly property var latest: sessions.length ? sessions[0] : ({})
    readonly property bool summary: shell.overviewFace === "summary"
    readonly property bool screens: shell.overviewFace === "screens"
    readonly property bool active: visible && shell.unobstructed
    readonly property bool hasCapture: !!shell.home.exitPreviewLabel
    readonly property real bottomInset: Theme.adventureCutoutHeight
    Item {
        id: heading; width: parent.width; height: 44
        Text {
            width: parent.width-platform.width-14; height: parent.height
            text: root.game.title || "Your next adventure"; textFormat: Text.PlainText
            font.family: Theme.displayFamily; font.pixelSize: 26; font.bold: true
            color: Theme.ink; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
        }
        PlatformBadge {
            id: platform; anchors.right: parent.right; frameless: true
            label: root.game.platformShort || ""; shape: root.game.platformShape || "console"
            visible: !!root.game.id
        }
    }
    Item {
        id: dashboard; objectName: "overview-dashboard"
        y: heading.height+8; width: parent.width
        height: parent.height-y-root.bottomInset; visible: root.summary
        readonly property real heroHeight: Math.max(150,height-112)
        Item {
            id: picture; objectName: "overview-screens"
            width: parent.width*0.51; height: dashboard.heroHeight
            HomeScreensCollage {
                anchors.fill: parent; anchors.bottomMargin: 18
                balanced: true; frameCount: 3
                source: root.hasCapture ? root.shell.home.exitPreview : ""
                selected: root.active && root.shell.focusIndex === 1
                enabled: root.active
                onActivated: root.shell.showOverview("screens")
            }
            Text {
                y: parent.height-13; width: parent.width; horizontalAlignment: Text.AlignHCenter
                text: root.hasCapture ? root.shell.home.exitPreviewLabel : "Your album starts with your first session"
                color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight
            }
        }
        Column {
            id: facts; x: picture.width+22; width: parent.width-x-18; spacing: 12
            Repeater {
                model: [
                    {label: "Time played", value: root.shell.home.recordedTime || "—", accent: Theme.blue},
                    {label: "Play sessions", value: String(root.shell.home.sessionCount || 0), accent: Theme.green},
                    {label: "Last played", value: root.latest.time || "Your story starts here", accent: Theme.pink}
                ]
                Item {
                    required property var modelData
                    width: facts.width; height: Math.max(46,(dashboard.heroHeight-24)/3)
                    Rectangle { x: 0; y: 3; width: 4; height: 29; radius: 2; color: modelData.accent }
                    Text { x: 15; width: parent.width-15; text: modelData.label; color: Theme.muted; font.pixelSize: 12 }
                    Text {
                        x: 15; y: 17; width: parent.width-15; height: 29
                        text: modelData.value; textFormat: Text.PlainText; color: Theme.ink
                        font.family: Theme.displayFamily; font.pixelSize: modelData.label === "Last played" ? 18 : 25
                        minimumPixelSize: 14; fontSizeMode: Text.HorizontalFit; elide: Text.ElideRight
                    }
                    Rectangle { y: parent.height; width: parent.width; height: 1; color: "#24526d63" }
                }
            }
        }
        Item {
            id: recent; objectName: "overview-timeline"
            y: dashboard.heroHeight+18; width: parent.width; height: 88
            Text { text: "Your timeline"; font.family: Theme.displayFamily; font.pixelSize: 20; color: Theme.ink }
            Text { anchors.right: parent.right; y: 3; text: "View history  ›"; color: Theme.muted; font.pixelSize: 13 }
            Rectangle { x: 10; y: 39; width: parent.width-20; height: 2; radius: 1; color: Qt.alpha(Theme.rim,0.6) }
            Row {
                y: 32; width: parent.width; spacing: 18
                Repeater {
                    model: root.sessions.slice(0,3).reverse()
                    Item {
                        required property var modelData
                        width: (recent.width-36)/3; height: 55
                        Rectangle { x: 7; width: 15; height: 15; radius: 7.5; color: modelData.outcome === "returned" ? Theme.green : modelData.outcome === "running" ? Theme.blue : Theme.pink; border.color: Theme.paper; border.width: 3 }
                        Text { x: 7; y: 18; text: modelData.date; color: Theme.ink; font.pixelSize: 12 }
                        Text { x: 7; y: 35; width: parent.width-14; text: modelData.duration; color: Theme.muted; font.pixelSize: 12; elide: Text.ElideRight }
                    }
                }
            }
            Text {
                visible: !root.sessions.length; x: 24; y: 48; width: parent.width-48
                text: "The first chapter is yours. Your sessions will leave their mark here."
                color: Theme.muted; font.pixelSize: 14; elide: Text.ElideRight
            }
            Rectangle {
                anchors.right: parent.right; y: 25; width: 105; height: 2; radius: 1
                color: Theme.focusGlow
                visible: root.active && root.shell.focusIndex === 0
            }
            MouseArea { anchors.fill: parent; enabled: root.active; onClicked: root.shell.showOverview("timeline") }
        }
    }
    GameTimeline {
        y: heading.height+7; width: parent.width; height: parent.height-y-root.bottomInset
        shell: root.shell; active: root.active && visible; visible: root.shell.overviewFace === "timeline"
    }
    Item {
        y: heading.height+8; width: parent.width; height: parent.height-y-root.bottomInset; visible: root.screens
        Text {
            text: "‹  Overview"; color: Theme.muted; font.pixelSize: 14
            MouseArea { anchors.fill: parent; anchors.margins: -8; enabled: root.active; onClicked: root.shell.showOverview("summary") }
        }
        Rectangle {
            x: 0; y: 30; width: parent.width; height: parent.height-y; radius: 12
            color: Theme.chassisDark; border.color: Theme.rim; border.width: 2
            Image {
                anchors.fill: parent; anchors.margins: 10; anchors.bottomMargin: 34
                visible: root.hasCapture; source: root.screens && root.hasCapture ? root.shell.home.exitPreview : ""
                asynchronous: true; fillMode: Image.PreserveAspectFit
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                y: root.hasCapture ? parent.height-27 : parent.height/2-height/2
                text: root.shell.home.exitPreviewLabel || "No saved game moments yet"; color: Theme.paper; font.pixelSize: 16
            }
        }
    }
}
