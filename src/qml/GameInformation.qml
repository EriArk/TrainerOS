import QtQuick

Item {
    id: root
    required property var entry
    property bool active: false
    property bool previewsEnabled: false
    readonly property var facts: [
        { label: "YEAR", value: entry.year || "", tint: "#f2df9d" },
        { label: "PLAYERS", value: entry.players || "", tint: "#c9dfb0" },
        { label: "GENRE", value: entry.genre || "", tint: "#bedce4" },
        { label: "DEVELOPER", value: entry.developer || "", tint: "#ddd0e9" },
        { label: "PUBLISHER", value: entry.publisher || "", tint: "#edcdbb" }
    ].filter(function(f) { return f.value.length > 0 })
    Text {
        id: title
        width: parent.width - 84; height: 49
        text: root.entry.title || ""; textFormat: Text.PlainText
        font.family: Theme.displayFamily; font.pixelSize: 23; font.bold: true
        color: Theme.ink; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
    }
    PlatformBadge {
        objectName: "library-detail-platform"
        anchors.right: parent.right
        label: root.entry.platformShort || ""; shape: root.entry.platformShape || "console"
    }
    Rectangle {
        id: picture
        y: 57; width: root.width * 0.61; height: Math.min(174, root.height * 0.51)
        radius: 5; color: "#142e32"; border.color: "#526a66"; border.width: 2
        GamePreview {
            anchors.fill: parent; anchors.margins: 4
            picture: root.entry.screenshot || ""
            video: root.entry.artwork ? (root.entry.artwork.video || "") : ""
            playbackAllowed: root.previewsEnabled && root.active
        }
    }
    Column {
        id: factsColumn
        x: picture.width + 13; y: picture.y; width: parent.width - x; spacing: 5
        Repeater {
            model: root.facts
            Rectangle {
                required property var modelData
                width: parent.width; height: Math.max(27, value.implicitHeight + 15)
                radius: 4; color: modelData.tint
                Text {
                    x: 7; y: 3; width: parent.width - 14
                    text: modelData.label; font.pixelSize: 8; font.bold: true
                    font.letterSpacing: 0.5; color: "#4b615e"
                }
                Text {
                    id: value
                    x: 7; y: 13; width: parent.width - 14
                    text: modelData.value; textFormat: Text.PlainText
                    font.pixelSize: 12; font.bold: true; color: Theme.ink
                    wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
                }
            }
        }
    }
    Text {
        id: status
        y: Math.max(picture.y + picture.height, factsColumn.y + factsColumn.height) + 10; width: parent.width
        text: root.entry.playable ? "" : root.entry.status || ""
        visible: text.length > 0; color: Theme.muted; font.pixelSize: 12
        textFormat: Text.PlainText
    }
    Item {
        id: descriptionViewport
        y: status.y + (status.visible ? status.height + 8 : 0)
        width: parent.width; height: Math.max(0, parent.height - y); clip: true
        readonly property real travel: Math.max(0, description.implicitHeight - height)
        readonly property bool mayScroll: root.active && root.visible && Qt.application.state === Qt.ApplicationActive && !Theme.reducedMotion && travel > 1
        function restart() { reading.stop(); description.y = 0; if (mayScroll) reading.start() }
        onMayScrollChanged: restart()
        onTravelChanged: restart()
        Connections { target: root; function onEntryChanged() { descriptionViewport.restart() } }
        Text {
            id: description
            objectName: "multiverse-description"
            width: parent.width; text: root.entry.synopsis || ""; textFormat: Text.PlainText
            color: Theme.ink; font.pixelSize: 14; lineHeight: 1.16; wrapMode: Text.WordWrap
        }
        SequentialAnimation {
            id: reading
            loops: Animation.Infinite
            PauseAnimation { duration: 6500 }
            NumberAnimation { target: description; property: "y"; to: -descriptionViewport.travel; duration: descriptionViewport.travel / 12 * 1000 }
            PauseAnimation { duration: 4500 }
            PropertyAction { target: description; property: "y"; value: 0 }
        }
    }
}
