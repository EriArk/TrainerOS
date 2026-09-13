import QtQuick

Item {
    id: root
    required property var shell
    readonly property var dex: shell.pokedex
    readonly property bool takesFocus: visible && !dex.journal.open && !shell.menuOpen && !shell.keyboard.open && shell.notice.length === 0
    readonly property bool detailOpen: dex.zone === "detail"
    readonly property bool pickerOpen: dex.zone === "picker"

    Item {
        anchors.fill: parent; visible: !root.detailOpen
        PageHeader { id: dexHeader; title: "Pokédex"; trailing: "A field guide for your discoveries" }
        MountedPanel {
            x: 0; y: dexHeader.height; width: parent.width; height: 72; color: "#c4dcd5"
            Row {
                x: 20; y: 10; spacing: 12
                Repeater {
                    model: root.dex.rail
                    delegate: CapButton {
                        required property int index
                        required property var modelData
                        objectName: "dex-rail-" + index
                        width: 136; height: 48; label: modelData.label; detail: modelData.value
                        tint: index === 0 ? Theme.yellow : index === 5 ? Theme.pink : Theme.blue
                        selected: root.takesFocus && root.dex.zone === "rail" && root.dex.focusIndex === index
                        onActivated: root.shell.activate(index, "rail")
                    }
                }
            }
            // Rail is physically joined to the top frame by the side uprights.
            Rectangle { x: 0; y: -parent.y; width: 6; height: parent.y; color: "#c4dcd5" }
            Rectangle { anchors.right: parent.right; y: -parent.y; width: 6; height: parent.y; color: "#c4dcd5" }
        }
        Rectangle {
            x: 0; y: dexHeader.height + 72; width: 530; height: 365 - y; color: "#dbe8dd"
            ListView {
                id: entries
                objectName: "dex-list"
                x: 19; y: 10; width: 476; height: 216
                model: root.dex.entries; currentIndex: root.dex.entryIndex
                interactive: false; keyNavigationEnabled: false; clip: true
                boundsBehavior: Flickable.StopAtBounds
                highlightMoveDuration: 0; cacheBuffer: 216
                function revealCurrent() {
                    if (count > 0 && !root.detailOpen) positionViewAtIndex(currentIndex, ListView.Contain);
                    // ListView may restore its focus scope after delegates are rebuilt.
                    // Reassert the actual control only once the current delegate exists.
                    if (root.takesFocus && root.dex.zone === "list" && currentItem)
                        currentItem.focusControl.forceActiveFocus(Qt.OtherFocusReason);
                }
                onCurrentIndexChanged: Qt.callLater(revealCurrent)
                onCurrentItemChanged: Qt.callLater(revealCurrent)
                onModelChanged: Qt.callLater(revealCurrent)
                Connections { target: root; function onDetailOpenChanged() { Qt.callLater(entries.revealCurrent); } }
                delegate: Item {
                    required property int index
                    required property var modelData
                    property alias focusControl: entryButton
                    width: entries.width; height: 72
                    CapButton {
                        id: entryButton
                        objectName: "dex-entry-" + modelData.id
                        x: 5; y: 5; width: parent.width - 10; height: 58
                        label: modelData.number + "   " + modelData.name + (modelData.favorite ? "  ★" : "")
                        detail: modelData.types + "  ·  " + modelData.status
                        tint: modelData.favorite ? Theme.yellow : Theme.green
                        selected: root.takesFocus && root.dex.zone === "list" && root.dex.entryIndex === index
                        onActivated: root.shell.activate(index, "list")
                    }
                }
            }
            Text {
                x: 28; y: 31; width: 452; wrapMode: Text.WordWrap
                visible: root.dex.entries.length === 0
                text: root.dex.emptyMessage; color: Theme.ink; font.pixelSize: 20; textFormat: Text.PlainText
            }
            CapButton {
                objectName: "dex-recovery"
                x: 28; y: 163; width: 330; height: 48; tint: Theme.yellow
                visible: root.dex.entries.length === 0; label: root.dex.recoveryLabel
                selected: root.takesFocus && root.dex.zone === "recovery"
                onActivated: root.shell.activate(0, "recovery")
            }
        }
        Rectangle {
            x: 530; y: dexHeader.height + 72; width: parent.width - x; height: 365 - y; color: "#e6edde"
            Rectangle {
                x: 24; y: 20; width: 117; height: 117; radius: 59; color: "#f6f5e5"; border.color: "#abc9b0"; border.width: 2
                TrainerEmblem { x: 12; y: 12; width: 93; height: 93; emblem: "spark" }
            }
            Text { x: 160; y: 31; text: root.dex.detail.number; color: Theme.muted; font.pixelSize: 24 }
            Text { x: 160; y: 77; text: "FIELD NOTES"; color: Theme.muted; font.pixelSize: 12; font.letterSpacing: 1.4 }
            Text { x: 24; y: 145; width: parent.width - 48; text: root.dex.detail.name; color: Theme.ink; font.pixelSize: 26; font.weight: Font.DemiBold; elide: Text.ElideRight }
            Text { x: 24; y: 186; width: parent.width - 48; text: root.dex.entries.length ? root.dex.detail.types + " · " + root.dex.detail.status : "Try another trail through the guide."; color: Theme.muted; font.pixelSize: 15; wrapMode: Text.WordWrap }
        }
        MountedPanel {
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
            height: 45; color: "#c4dcd5"
            Text { x: 27; y: 13; text: root.dex.entries.length + " entries · " + root.dex.source; color: Theme.muted; font.pixelSize: 14 }
            Text { x: 390; y: 13; text: "X Search   ·   ← / → Jump 8   ·   ↑ Filters"; color: Theme.muted; font.pixelSize: 14 }
        }
    }

    Item {
        anchors.fill: parent; visible: root.detailOpen
        PageHeader {
            eyebrow: "POKÉDEX / " + root.dex.detail.number
            title: root.dex.detail.name; trailing: root.dex.detail.form
        }
        Text { x: 29; y: 95; text: root.dex.detail.types; color: Theme.muted; font.pixelSize: 19 }
        Text { x: 420; y: 98; width: parent.width - 448; text: "Height " + root.dex.detail.height + "   ·   Weight " + root.dex.detail.weight; color: Theme.muted; font.pixelSize: 16; horizontalAlignment: Text.AlignRight }
        Rectangle {
            x: 0; y: 135; width: 291; height: 197; color: "#d6e4d8"
            Text { x: 25; y: 8; text: "BASE STATS · REFERENCE"; color: Theme.muted; font.pixelSize: 11; font.letterSpacing: 1 }
            Repeater {
                model: root.dex.detail.stats
                delegate: Item {
                    required property int index
                    required property var modelData
                    x: 25; y: 31 + index * 25; width: 242; height: 19
                    Text { x: 0; y: 1; width: 64; text: modelData.label; color: Theme.ink; font.pixelSize: 13 }
                    Rectangle {
                        x: 67; y: 4; width: 134; height: 12; radius: 5; color: "#b8cabc"
                        Rectangle { width: parent.width * modelData.fraction; height: parent.height; radius: 5; color: Theme.tabColors[index % Theme.tabColors.length]; border.color: "#669982" }
                    }
                    Text { x: 208; y: 1; width: 33; text: modelData.value; color: Theme.ink; font.pixelSize: 13; horizontalAlignment: Text.AlignRight }
                }
            }
        }
        Column {
            x: 316; y: 146; width: parent.width - 344; spacing: 9
            Text { text: sessionState.persistent ? "YOUR JOURNAL · MANUALLY RECORDED" : "YOUR JOURNAL · SAMPLE DATA"; color: Theme.muted; font.pixelSize: 11; font.letterSpacing: 1 }
            Text { width: parent.width; text: "Seen: " + root.dex.detail.seen + "   ·   Caught: " + root.dex.detail.caught; color: Theme.ink; font.pixelSize: 19; elide: Text.ElideRight }
            Text { width: parent.width; text: root.dex.detail.notes.length ? root.dex.detail.notes : "Y opens your field journal. Unknown records stay unknown."; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 15; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
            Text { width: parent.width; text: "Regional lists: " + (root.dex.detail.worlds.length ? root.dex.detail.worlds : "National guide only"); color: Theme.muted; font.pixelSize: 13; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
            Text { width: parent.width; text: "Family: " + (root.dex.detail.family.length ? root.dex.detail.family : "Not available in this reference"); color: Theme.muted; font.pixelSize: 13; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
        }
        MountedPanel {
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
            height: 78; color: "#c4dcd5"
            CapButton {
                objectName: "dex-favorite"
                x: 28; y: 17; width: 231; height: 46; tint: Theme.yellow
                label: root.dex.saving ? "Saving…" : root.dex.detail.favorite ? "A   Remove favorite" : "A   Add favorite"
                selected: root.takesFocus && root.detailOpen && root.dex.focusIndex === 0
                onActivated: root.shell.activate(0)
            }
            CapButton { x: 277; y: 17; width: 215; height: 46; tint: Theme.green; label: "Y   Field journal"; enabled: root.dex.detail.editable; onActivated: root.dex.editJournal() }
            CapButton { x: 510; y: 17; width: 176; height: 46; tint: Theme.pink; label: "X   Form"; enabled: root.dex.detail.formCount > 1; opacity: enabled ? 1 : 0.5; onActivated: root.dex.cycleForm() }
            CapButton {
                objectName: "dex-back"
                x: 704; y: 17; width: 184; height: 46; tint: Theme.blue; label: "B   Back to entries"
                selected: root.takesFocus && root.detailOpen && root.dex.focusIndex === 1
                onActivated: root.shell.activate(1)
            }
        }
    }
    Item {
        anchors.fill: parent; visible: root.pickerOpen
        Rectangle { anchors.fill: parent; color: "#80103935" }
        MouseArea { anchors.fill: parent }
        Rectangle {
            id: pickerTray
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 325; color: Theme.chassis; border.color: Theme.rim; border.width: 2
            Text { x: 22; y: 15; text: root.dex.pickerTitle; color: "#f3f4dd"; font.pixelSize: 24; font.weight: Font.DemiBold }
            Text { x: 23; y: 52; text: "A Apply choice   ·   B Cancel   ·   Current choice is marked"; color: "#d1e2d2"; font.pixelSize: 14 }
            Grid {
                x: 23; y: 86; columns: 5; spacing: 12
                Repeater {
                    model: root.dex.choices
                    delegate: CapButton {
                        required property int index
                        required property var modelData
                        objectName: "dex-choice-" + index
                        width: 164; height: 46; textSize: 16
                        label: (modelData.current ? "• " : "") + modelData.label
                        tint: modelData.id === "cancel" ? Theme.pink : modelData.current ? Theme.yellow : Theme.blue
                        focusColor: "#fff3b4"
                        selected: root.takesFocus && root.pickerOpen && root.dex.focusIndex === index
                        onActivated: root.shell.activate(index)
                    }
                }
            }
        }
    }
    PokedexJournalPanel { anchors.fill: parent; shell: root.shell; visible: root.dex.journal.open }
}
