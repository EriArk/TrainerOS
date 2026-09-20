import QtQuick

Item {
    id: root
    required property var shell
    readonly property var party: shell.party
    readonly property bool takesFocus: visible && !shell.drawerOpen && !shell.menuOpen && !shell.keyboard.open && !shell.notice.length
    enabled: !shell.drawerOpen
    Item {
        anchors.fill: parent; anchors.margins: Theme.panelInset
        anchors.topMargin: Theme.contentTopInset; anchors.bottomMargin: Theme.panelInset + 30; clip: true
        PageHeader {
            id: heading; compact: true
            title: root.party.detailOpen ? (root.party.detail.name || "Pokémon") : root.party.section === "party" ? "Party" : "Storage"
            trailing: "Pokémon Center"; subtitle: root.party.status
        }
        MountedPanel {
            y: heading.height; width: parent.width; height: parent.height - y; color: "#d8e5dc"
            Text { x: 24; y: 12; width: parent.width - 48; text: root.party.title || "Choose an Adventure with Y"; color: Theme.muted; font.pixelSize: 15; elide: Text.ElideRight; textFormat: Text.PlainText }
            Item {
                anchors.fill: parent; visible: !root.party.detailOpen
                Text { x: 24; y: 37; text: root.party.section === "storage" && root.party.sample ? "Sample box " + (root.party.box + 1) + " / 2 · ← / → at an edge changes box" : ""; color: Theme.ink; font.pixelSize: 14 }
                Grid {
                    x: 24; y: root.party.section === "party" ? 43 : 65
                    columns: root.party.section === "party" ? 2 : 4; spacing: 12
                    Repeater {
                        model: root.party.entries
                        CapButton {
                            required property int index
                            required property var modelData
                            objectName: "party-slot-" + index
                            width: (root.width - 2 * Theme.panelInset - 48 - (parent.columns - 1) * 12) / parent.columns
                            height: root.party.section === "party" ? 61 : 53
                            label: (index + 1) + " · " + modelData.name
                            detail: root.party.section === "party" ? modelData.summary : modelData.kind === "known" ? "Lv. " + modelData.level : modelData.condition
                            textSize: root.party.section === "party" ? 18 : 15
                            tint: modelData.kind === "empty" ? "#c9d3ca" : modelData.kind === "unreadable" ? "#e5c3bd" : index % 2 ? Theme.blue : Theme.green
                            selected: root.takesFocus && !root.party.detailOpen && root.party.focusIndex === index
                            onActivated: root.shell.activate(index)
                        }
                    }
                }
                Column {
                    x: 28; y: 61; width: parent.width - 56; spacing: 18; visible: !root.party.sample
                    Text { width: parent.width; text: "No verified Pokémon records yet"; color: Theme.ink; font.pixelSize: 25; font.bold: true }
                    Text { width: parent.width; text: "Your saved game stays untouched.\nOrdinary save backups remain available where supported."; color: Theme.muted; font.pixelSize: 18; wrapMode: Text.WordWrap }
                    CapButton { objectName: "party-unavailable"; width: 360; height: 54; label: "A · Open save backups"; selected: root.takesFocus && !root.party.detailOpen && !root.party.sample && !root.party.activitiesFocused; onActivated: root.shell.activate(0) }
                }
            }
            Item {
                anchors.fill: parent; visible: root.party.detailOpen
                Rectangle {
                    x: 24; y: 48; width: 158; height: 158; radius: 79; color: "#b9cfc1"; border.color: "#78988b"; border.width: 3
                    Text { anchors.centerIn: parent; text: root.party.detail.species || "?"; color: Theme.ink; font.pixelSize: 29; font.bold: true }
                }
                Text { x: 204; y: 50; width: 300; text: "Level  " + (root.party.detail.level || "—") + "\nHP  " + (root.party.detail.hp || "—") + "\nStatus  " + (root.party.detail.condition || "—") + "\nHeld item  " + (root.party.detail.item || "—"); color: Theme.ink; font.pixelSize: 19; lineHeight: 1.5; textFormat: Text.PlainText }
                Text { x: 530; y: 50; width: parent.width - 556; text: "Moves\n" + (root.party.detail.moves || "Unknown"); color: Theme.ink; font.pixelSize: 17; lineHeight: 1.5; wrapMode: Text.WordWrap; textFormat: Text.PlainText }
                CapButton { objectName: "party-detail-back"; x: 24; y: 226; width: 260; height: 44; label: "A / B · Back to slots"; selected: root.takesFocus && root.party.detailOpen; onActivated: root.shell.activate(0) }
                Text { x: 312; y: 229; width: parent.width - 340; text: "Read-only preview · healing, moving and release unavailable"; color: Theme.muted; font.pixelSize: 14; wrapMode: Text.WordWrap }
            }
            CapButton {
                objectName: "party-activities"; anchors.right: parent.right; anchors.rightMargin: 24
                anchors.bottom: parent.bottom; anchors.bottomMargin: 8; width: 248; height: 38
                label: "Activities"; tint: Theme.blue; visible: !root.party.detailOpen
                selected: root.takesFocus && root.party.activitiesFocused
                onActivated: root.shell.activate(0, "party-activities")
            }
            Row {
                x: 24; anchors.bottom: parent.bottom; anchors.bottomMargin: 12; spacing: 24
                visible: !root.party.detailOpen
                Hint { button: "X"; label: root.party.section === "party" ? "Storage" : "Party"; labelColor: Theme.ink }
                Hint { button: "Select"; label: "Save backups"; labelColor: Theme.ink }
            }
        }
    }
}
