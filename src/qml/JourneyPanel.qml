import QtQuick

Item {
    id: root
    required property var shell
    property bool takesFocus: false
    readonly property var current: shell.home
    readonly property var journey: shell.hall.journey
    readonly property bool champions: shell.hall.route !== "archive-journey"
    readonly property var snapshot: shell.hall.championPreview
    readonly property bool hasRecord: !!snapshot.title
    PageHeader {
        id: heading; compact: true
        title: root.champions ? "Champion records" : "Journey Record"
        subtitle: root.current.adventure
        trailing: root.shell.sampleLibrary ? "Development sample" : "Hall of Fame"
    }
    Item {
        id: body; y: heading.height; width: parent.width; height: parent.height - y
        Column {
            x: 24; y: 8; width: parent.width - 48; spacing: 10; visible: !root.champions
            BadgeTray {
                width: parent.width; height: Math.min(165, body.height * .30)
                slots: root.current.badgeSlots; count: root.current.badges; badgePrefix: "journey-badge-"
                color: "#e6dfbd"; border.color: "#aa9562"
            }
            Row {
                width: parent.width; spacing: 12
                Repeater {
                    model: [{label: "PLAY TIME", value: root.journey.time, tint: "#badceb"},
                            {label: "SEEN", value: root.journey.seen, tint: "#d1e6ad"},
                            {label: "CAUGHT", value: root.current.caught, tint: "#eed4b0"}]
                    Rectangle {
                        required property var modelData
                        width: (parent.width - 24) / 3; height: 70; radius: 10
                        color: modelData.tint; border.color: "#a2b3a1"
                        Text { x: 16; y: 10; text: modelData.label; color: Theme.muted; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.5 }
                        Text { x: 16; y: 29; text: modelData.value || "—"; color: Theme.ink; font.pixelSize: 27; font.bold: true }
                    }
                }
            }
            Row {
                width: parent.width; spacing: 12
                Repeater {
                    model: root.journey.milestones || []
                    Item {
                        required property int index
                        required property var modelData
                        width: (parent.width - 24) / Math.max(1, root.journey.milestones.length); height: 64
                        Rectangle { x: 22; y: 17; width: parent.width - 10; height: 3; visible: index + 1 < root.journey.milestones.length; color: "#b2bbaa" }
                        Rectangle {
                            x: 7; y: 8; width: 22; height: 22; rotation: 45; radius: 3
                            color: modelData.earned ? "#efba47" : "#d6dccc"; border.width: 2
                            border.color: modelData.earned ? "#a27229" : "#a6b39d"
                            Rectangle { x: 4; y: 4; width: 9; height: 9; radius: 1; color: modelData.earned ? "#ffe794" : "#edf0e4" }
                        }
                        Text { x: 4; y: 39; width: parent.width - 8; text: modelData.title; font.pixelSize: 16; font.bold: modelData.earned; color: modelData.earned ? Theme.ink : Theme.muted; elide: Text.ElideRight }
                    }
                }
            }
        }
        Column {
            x: 32; y: 52; width: parent.width - 64; spacing: 14; visible: root.champions && !root.hasRecord
            Text { text: "Your Champion teams belong here"; color: Theme.ink; font.pixelSize: 27; font.bold: true }
            Text { width: parent.width; text: root.journey.error || "No Champion team recorded for this Adventure yet."; color: Theme.muted; font.pixelSize: 17; wrapMode: Text.WordWrap }
        }
        Item {
            anchors.fill: parent; visible: root.champions && root.hasRecord
            Rectangle {
                x: 24; y: 12; width: parent.width - 48; height: 60; radius: 10
                color: "#f1d28a"; border.color: "#a98540"
                Text { x: 18; anchors.verticalCenter: parent.verticalCenter; text: root.snapshot.title || ""; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 27; font.bold: true }
                Text { anchors.right: parent.right; anchors.rightMargin: 20; anchors.verticalCenter: parent.verticalCenter; text: root.snapshot.position || "Sample"; color: "#6d5429"; font.pixelSize: 20; font.bold: true }
            }
            Row {
                x: 24; y: 87; width: parent.width - 48; spacing: 9
                Repeater {
                    model: root.snapshot.team || []
                    Rectangle {
                        required property int index
                        required property var modelData
                        width: (parent.width - 45) / 6; height: Math.min(238, body.height - 169); radius: 12
                        color: ["#cee6bb","#bcdeeb","#ebc9bc","#d9c9e9","#eedb9e","#b7ddd4"][index % 6]; border.color: "#9fac99"
                        Rectangle { anchors.fill: parent; anchors.margins: 2; radius: 10; color: "transparent"; border.color: "#f5f4df" }
                        ClassicIllustration { x: 7; y: 8; width: parent.width - 14; height: parent.height - 70; art: modelData.art || ({}) }
                        Text { x: 5; y: parent.height - 58; width: parent.width - 10; text: modelData.name; textFormat: Text.PlainText; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight; color: Theme.ink; font.pixelSize: 17; font.bold: true }
                        Text { x: 5; y: parent.height - 31; width: parent.width - 10; text: modelData.level + (modelData.shiny ? "  ✦" : ""); horizontalAlignment: Text.AlignHCenter; color: Theme.ink; font.pixelSize: 15 }
                    }
                }
            }
            Text { x: 28; y: Math.min(238, body.height - 169) + 95; text: root.shell.sampleLibrary ? "Development sample" : root.snapshot.observed || ""; color: Theme.muted; font.pixelSize: 13 }
        }
        CapButton {
            objectName: "journey-primary"
            x: root.shell.chooseAdventureAvailable ? Theme.adventureCutoutWidth : 24
            anchors.bottom: parent.bottom; anchors.bottomMargin: 13; width: 300; height: 43
            label: root.champions ? "Journey Record" : "Champion records" + (root.journey.championCount ? " · " + root.journey.championCount : "")
            tint: root.champions ? Theme.blue : Theme.yellow; selected: root.takesFocus
            onActivated: root.shell.activate(0)
        }
    }
}
