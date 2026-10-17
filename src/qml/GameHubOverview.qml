import QtQuick

Item {
    id: root
    required property var shell
    readonly property var media: visible ? shell.storyMedia : []
    readonly property var sessions: visible ? shell.storyHistory : []
    readonly property var summaryData: visible ? shell.story : ({})
    readonly property var latest: sessions.length ? sessions[0] : ({})
    readonly property bool summary: shell.overviewFace === "summary"
    readonly property bool screens: shell.overviewFace === "screens"
    readonly property bool active: visible && shell.unobstructed
    readonly property real bottomInset: Theme.adventureCutoutHeight
    Item {
        id: heading; width: parent.width; height: root.screens ? 0 : 44; visible: !root.screens
        Text {
            width: parent.width; height: parent.height
            text: root.shell.overviewFace === "screens" ? "Screens" : "My Story"; textFormat: Text.PlainText
            font.family: Theme.displayFamily; font.pixelSize: 26; font.bold: true
            color: Theme.ink; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
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
                source: root.media.length ? root.media[0].source : ""
                rearSource: root.media.length > 1 ? root.media[1].source : ""
                thirdSource: root.media.length > 2 ? root.media[2].source : ""
                selected: root.active && root.shell.focusIndex === 1
                enabled: root.active
                onActivated: root.shell.showOverview("screens")
            }
            Text {
                y: parent.height-13; width: parent.width; horizontalAlignment: Text.AlignHCenter
                text: root.media.length ? "Moments from your adventures" : "Your album starts with your first session"
                color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight
            }
        }
        Column {
            id: facts; x: picture.width+22; width: parent.width-x-18; spacing: 12
            Repeater {
                model: [
                    {label: "Recorded playtime", value: root.summaryData.recordedTime || "—", accent: Theme.blue},
                    {label: "Games played", value: String(root.summaryData.gamesPlayed || 0), accent: Theme.green},
                    {label: "Achievements", value: root.summaryData.achievementCount || "—", note: root.summaryData.achievementNote || "", accent: Theme.pink}
                ]
                Item {
                    required property var modelData
                    required property int index
                    width: facts.width; height: Math.max(46,(dashboard.heroHeight-24)/3)
                    Rectangle { x: 0; y: 3; width: 4; height: 29; radius: 2; color: modelData.accent }
                    Text { x: 15; width: parent.width-15; text: modelData.label; color: Theme.muted; font.pixelSize: 12 }
                    Text {
                        x: 15; y: 17; width: parent.width-15; height: 29
                        text: modelData.value; textFormat: Text.PlainText; color: Theme.ink
                        font.family: Theme.displayFamily; font.pixelSize: 25
                        minimumPixelSize: 14; fontSizeMode: Text.HorizontalFit; elide: Text.ElideRight
                    }
                    Text { x: 15; y: 47; width: parent.width-15; text: modelData.note || ""; color: Theme.muted; font.pixelSize: 10; elide: Text.ElideRight }
                    Rectangle { y: parent.height; width: parent.width; height: 1; color: "#24526d63"; visible: index < 2 }
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
                        Text { x: 7; y: 18; width: parent.width-14; text: modelData.game; textFormat: Text.PlainText; elide: Text.ElideRight; color: Theme.ink; font.pixelSize: 12 }
                        Text { x: 7; y: 35; width: parent.width-14; text: modelData.date + "  \u00b7  " + modelData.duration; color: Theme.muted; font.pixelSize: 12; elide: Text.ElideRight }
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
    ScreensAlbum {
        y: 0; width: parent.width; height: parent.height-root.bottomInset
        shell: root.shell; visible: root.screens
        active: root.active && visible
    }
}
