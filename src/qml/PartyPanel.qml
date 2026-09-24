import QtQuick

Item {
    id: root
    required property var shell
    readonly property var party: shell.party
    readonly property var selected: party.detail
    readonly property bool storage: party.section === "storage"
    readonly property bool takesFocus: visible && !shell.serviceOpen && !shell.drawerOpen && !shell.menuOpen && !shell.keyboard.open && !shell.notice.length
    enabled: !shell.drawerOpen
    Item {
        anchors.fill: parent; anchors.margins: Theme.panelInset
        anchors.topMargin: Theme.contentTopInset; anchors.bottomMargin: Theme.panelInset + 30
        PageHeader {
            id: heading; compact: true
            title: root.storage ? "Storage" : "Party"
            trailing: root.party.sample ? "Development preview" : "Pokémon Center"
            subtitle: root.party.title || "Choose an Adventure"
        }
        Item {
            id: body; y: heading.height + 4; width: parent.width; height: parent.height - y
            Rectangle {
                id: tray; width: parent.width * 0.46; height: parent.height
                color: root.storage ? "#ccdedf" : "#d3e2cc"; radius: 9
                border.color: "#839d93"
                CapButton {
                    objectName: "party-box"; x: 12; y: 7; width: parent.width - 24; height: 33
                    visible: root.storage && root.party.available
                    label: "‹     " + root.party.boxName + "  " + (root.party.box + 1) + " / " + root.party.boxCount + "     ›"; centered: true; textSize: 15; tint: Theme.blue
                    selected: root.takesFocus && root.party.boxFocused && !root.party.detailOpen
                    onActivated: root.party.changeBox(1)
                }
                Grid {
                    id: slots; x: 12; y: root.storage ? 51 : 12; width: parent.width - 24
                    columns: root.storage ? 6 : 2; spacing: root.storage ? 7 : 10
                    Repeater {
                        model: root.party.entries
                        CapButton {
                            id: slot
                            required property int index
                            required property var modelData
                            objectName: "party-slot-" + index
                            width: (slots.width - (slots.columns - 1) * slots.spacing) / slots.columns
                            height: root.storage ? (tray.height - 108) / 5 - 7 : (tray.height - 78) / 3
                            label: ""; tint: modelData.kind === "empty" ? "#dce6dc" : modelData.kind === "unreadable" ? "#e8bdb1" : index % 2 ? "#b6d9ed" : "#c6df9c"
                            selected: root.takesFocus && !root.party.detailOpen && !root.party.boxFocused && !root.party.activitiesFocused && root.party.focusIndex === index
                            onActivated: root.shell.activate(index)
                            Item {
                                x: root.storage ? 3 : 7; y: 4; width: root.storage ? parent.width - 6 : 53; height: parent.height - 8
                                SpritePreview {
                                    id: icon; anchors.fill: parent; asset: slot.modelData.sprite || ({}); pixelScale: 4; trimTransparentMargins: true
                                    playing: slot.selected; visible: slot.modelData.kind === "known" && !!asset.url
                                }
                                ClassicIllustration { anchors.fill: parent; art: slot.modelData.art || ({}); visible: slot.modelData.kind === "known" && !icon.ready }
                                Rectangle {
                                    anchors.centerIn: parent; width: Math.min(parent.width * .65, parent.height * .7); height: width * 1.18
                                    radius: width / 2; rotation: -12; color: "#fff2cf"; border.color: "#a39c78"; border.width: 2
                                    visible: slot.modelData.kind === "egg"
                                    Rectangle { x: parent.width * .16; y: parent.height * .2; width: parent.width * .27; height: width; radius: width / 2; color: "#92b798" }
                                    Rectangle { x: parent.width * .55; y: parent.height * .55; width: parent.width * .27; height: width; radius: width / 2; color: "#92b798" }
                                }
                                Text { textFormat: Text.PlainText; anchors.centerIn: parent; text: slot.modelData.kind === "unreadable" ? "?" : "·"; visible: slot.modelData.kind === "unreadable" || slot.modelData.kind === "empty"; font.pixelSize: 22; color: Theme.muted }
                            }
                            Column {
                                visible: !root.storage; x: 63; y: 8; width: parent.width - x - 7; spacing: 4
                                Text { textFormat: Text.PlainText; width: parent.width; text: slot.modelData.name; font.family: Theme.displayFamily; font.bold: true; font.pixelSize: 15; color: Theme.ink; elide: Text.ElideRight }
                                Text { textFormat: Text.PlainText; width: parent.width; text: slot.modelData.kind === "known" ? "Lv. " + slot.modelData.level + " · " + slot.modelData.hp : slot.modelData.condition; font.pixelSize: 11; color: Theme.ink; elide: Text.ElideRight }
                                Rectangle {
                                    width: parent.width; height: 5; radius: 2; color: "#819789"; visible: slot.modelData.kind === "known"
                                    Rectangle { width: parent.width * (slot.modelData.hpRatio || 0); height: 5; radius: 2; color: "#4b9670" }
                                }
                            }
                        }
                    }
                }
                Column {
                    x: 22; y: 44; width: parent.width - 44; spacing: 16; visible: !root.party.available
                    Text { textFormat: Text.PlainText; width: parent.width; text: root.storage ? "Your Pokémon boxes" : "Your team"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 25 }
                    Text { textFormat: Text.PlainText; width: parent.width; text: root.party.status; color: Theme.muted; font.pixelSize: 16; wrapMode: Text.WordWrap }
                    CapButton { objectName: "party-unavailable"; width: parent.width; height: 43; label: "Save backups"; tint: Theme.blue; selected: root.takesFocus && !root.party.activitiesFocused; onActivated: root.shell.activate(0) }
                }
                CapButton {
                    objectName: "party-activities"; x: 12; anchors.bottom: parent.bottom; anchors.bottomMargin: 10; width: parent.width - 24; height: 33
                    label: "Activities"; tint: Theme.blue; textSize: 14; centered: true
                    selected: root.takesFocus && root.party.activitiesFocused && !root.party.detailOpen
                    onActivated: root.shell.activate(0, "party-activities")
                }
            }
            Item {
                id: summary; objectName: "party-summary"; x: tray.width + 18; width: parent.width - x; height: parent.height
                readonly property bool known: root.selected.kind === "known"
                Text { textFormat: Text.PlainText; id: name; width: parent.width - 82; text: root.selected.name || "Pokémon Center"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 25; font.bold: true; elide: Text.ElideRight }
                Text { textFormat: Text.PlainText; anchors.right: parent.right; y: 6; text: summary.known ? "Lv. " + root.selected.level : ""; color: Theme.ink; font.pixelSize: 19; font.bold: true }
                Text { textFormat: Text.PlainText; y: 33; width: parent.width; text: (root.selected.types || "") + (summary.known && root.selected.condition && root.selected.condition !== "Healthy" ? " · " + root.selected.condition : ""); color: Theme.muted; font.pixelSize: 14; elide: Text.ElideRight }
                ClassicIllustration { x: 0; y: 58; width: 131; height: 123; visible: summary.known; art: root.selected.art || ({}) }
                Column {
                    x: 147; y: 56; width: parent.width - x; spacing: 6; visible: summary.known
                    Repeater {
                        model: (root.storage ? [] : [{label:"HP",value:root.selected.hp,tint:"#c3dfa7"}]).concat([
                            {label:"Ability",value:root.selected.ability,tint:"#bfdbeb"},
                            {label:"Nature",value:root.selected.nature,tint:"#dbcae8"},
                            {label:"Item",value:root.selected.item,tint:"#f0d397"}
                        ])
                        Rectangle {
                            required property var modelData
                            width: parent.width; height: root.storage ? 35 : 25; radius: 5; color: modelData.tint
                            Text { textFormat: Text.PlainText; x: 8; anchors.verticalCenter: parent.verticalCenter; text: modelData.label; color: Theme.muted; font.pixelSize: 11 }
                            Text { textFormat: Text.PlainText; x: 60; width: parent.width - 68; anchors.verticalCenter: parent.verticalCenter; text: modelData.value || "—"; color: Theme.ink; font.pixelSize: 13; font.bold: true; elide: Text.ElideRight }
                        }
                    }
                }
                Row {
                    y: 190; width: parent.width; spacing: 5; visible: summary.known
                    Repeater {
                        model: ["HP", "Attack", "Defense", "Sp. Atk", "Sp. Def", "Speed"]
                        Rectangle {
                            required property int index; required property string modelData
                            width: (summary.width - 25) / 6; height: 39; radius: 5
                            color: ["#f1d487","#d4e4ab","#b4d9e9","#ecbcc3","#d6bfe9","#f0c295"][index]
                            Text { textFormat: Text.PlainText; y: 3; width: parent.width; horizontalAlignment: Text.AlignHCenter; text: modelData; color: Theme.muted; font.pixelSize: 10 }
                            Text { textFormat: Text.PlainText; y: 16; width: parent.width; horizontalAlignment: Text.AlignHCenter; text: root.selected.stats ? root.selected.stats[index] : "—"; color: Theme.ink; font.pixelSize: 17; font.bold: true }
                        }
                    }
                }
                Grid {
                    y: 240; width: parent.width; columns: 2; spacing: 6; visible: summary.known
                    Repeater {
                        model: 4
                        Rectangle {
                            required property int index
                            width: (summary.width - 6) / 2; height: 30; radius: 5; color: index % 2 ? "#d3e5d1" : "#d1e0ed"
                            Text { textFormat: Text.PlainText; x: 8; width: parent.width - 16; anchors.verticalCenter: parent.verticalCenter; text: root.selected.moves ? root.selected.moves.split("\n")[index] || "—" : "—"; color: Theme.ink; font.pixelSize: 11; elide: Text.ElideRight }
                        }
                    }
                }
                Text { textFormat: Text.PlainText;
                    x: 12; y: 112; width: parent.width - 24; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter
                    visible: !summary.known; font.pixelSize: 20; color: Theme.muted
                    text: root.selected.kind === "egg" ? "An Egg in your care" : root.selected.kind === "empty" ? "An empty slot" : root.selected.kind === "unreadable" ? "This Pokémon could not be read" : "No Pokémon to display yet."
                }
            }
        }
    }
    Rectangle {
        anchors.fill: parent; anchors.topMargin: Theme.contentTopInset; color: "#660e2524"; visible: root.party.detailOpen
        MouseArea { anchors.fill: parent }
        MountedPanel {
            anchors.centerIn: parent; width: 330; height: 232; color: "#e3e9dc"
            Text { textFormat: Text.PlainText; x: 22; y: 17; width: parent.width - 44; text: root.selected.name || "Pokémon"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 24; elide: Text.ElideRight }
            Row {
                x: 22; y: 56; spacing: 10
                CapButton { width: 138; height: 38; label: "Move"; enabled: false }
                CapButton { width: 138; height: 38; label: "Heal"; enabled: false }
            }
            CapButton { objectName: "party-menu-backups"; x: 22; y: 108; width: 286; height: 42; label: "Adventure backups"; tint: Theme.blue; selected: root.takesFocus && root.party.menuIndex === 1; onActivated: root.party.activate(1) }
            CapButton { objectName: "party-detail-back"; x: 22; y: 166; width: 286; height: 42; label: "Close"; selected: root.takesFocus && root.party.menuIndex === 0; onActivated: root.party.activate(0) }
        }
    }
}
