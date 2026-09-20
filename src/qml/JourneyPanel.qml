import QtQuick

Item {
    id: root
    required property var shell
    property bool takesFocus: false
    readonly property var current: shell.home
    readonly property bool champions: shell.hall.route !== "archive-journey"
    readonly property bool championDetail: shell.hall.route === "archive-champion-detail"
    readonly property var snapshot: shell.hall.championPreview
    readonly property bool sampleChampion: !!snapshot.title
    PageHeader {
        id: heading; compact: true
        title: root.champions ? "Champion records" : "Journey Record"
        trailing: "Hall of Fame"
        subtitle: root.shell.sampleLibrary ? "Development sample · not a personal playthrough"
            : root.current.progressNote || "Only verified save observations appear here"
    }
    MountedPanel {
        y: heading.height; width: parent.width; height: parent.height - y; color: "#e1e8dc"
        Text { x: 24; y: 12; width: parent.width - 48; text: root.current.adventure; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 24; font.bold: true; elide: Text.ElideRight }
        Item {
            anchors.fill: parent; visible: !root.champions
            BadgeTray { x: 24; y: 54; width: parent.width - 48; height: 145; slots: root.current.badgeSlots; count: root.current.badges; badgePrefix: "journey-badge-" }
            Text { x: 28; y: 205; width: parent.width - 56; text: "Caught  " + root.current.caught + "     ·     Save playtime  —     ·     Milestones  —"; color: Theme.ink; font.pixelSize: 17; elide: Text.ElideRight }
            Text { x: 28; y: 232; width: parent.width - 56; text: "Unknown fields stay unknown. Badges do not establish a Champion victory."; color: Theme.muted; font.pixelSize: 12; wrapMode: Text.WordWrap }
        }
        Column {
            x: 28; y: 65; width: parent.width - 56; spacing: 18; visible: root.champions && !root.sampleChampion
            Text { width: parent.width; text: "No verified Champion snapshots yet"; color: Theme.ink; font.pixelSize: 26; font.bold: true; wrapMode: Text.WordWrap }
            Text { width: parent.width; text: "Historical teams and victory dates need a supported save reader.\nYour manual Adventure memories remain in their own archive."; color: Theme.muted; font.pixelSize: 18; wrapMode: Text.WordWrap }
        }
        Column {
            x: 28; y: 52; width: parent.width - 56; spacing: 8; visible: root.champions && root.sampleChampion
            Text { width: parent.width; text: root.snapshot.title || ""; color: Theme.ink; font.pixelSize: 25; font.bold: true }
            Text { width: parent.width; text: root.championDetail ? "Historical sample team · never substituted with today's Party" : "Development sample · never saved to your archive"; color: Theme.muted; font.pixelSize: 15 }
            Text { width: parent.width; text: (root.snapshot.victory || "") + "\n" + (root.snapshot.observed || ""); color: Theme.ink; font.pixelSize: 16; lineHeight: 1.2 }
            Row {
                spacing: 12; visible: root.championDetail
                Repeater {
                    model: root.snapshot.team || []
                    Rectangle {
                        required property var modelData
                        width: 255; height: 58; radius: 12; color: "#cbdad1"; border.color: "#9ab5a5"
                        Text { x: 12; y: 8; text: modelData.name; color: Theme.ink; font.pixelSize: 17 }
                        Text { x: 12; y: 34; text: modelData.level; color: Theme.muted; font.pixelSize: 14 }
                    }
                }
            }
            Text { visible: !root.championDetail; width: parent.width; text: "A opens the historical team. B returns to Journey."; color: Theme.muted; font.pixelSize: 14 }
        }
        Row {
            x: 24; anchors.bottom: parent.bottom; anchors.bottomMargin: 13; spacing: 16
            CapButton {
                objectName: "journey-primary"; width: 306; height: 45
                label: root.championDetail ? "A / B · Champion records" : root.champions ? root.sampleChampion ? "A · View sample record" : "A / B · Journey Record" : "A · Adventure memories"
                tint: Theme.blue; selected: root.takesFocus
                onActivated: root.shell.activate(0)
            }
            CapButton {
                width: 258; height: 45; visible: !root.champions; label: "X · Champion records"; tint: Theme.yellow
                onActivated: root.shell.activate(0, "journey-champions")
            }
            Text { y: 13; text: !root.champions && root.shell.hall.editable ? "Select · New memory" : ""; color: Theme.muted; font.pixelSize: 14 }
        }
    }
}
