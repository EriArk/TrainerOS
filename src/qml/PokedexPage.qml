import QtQuick

Item {
    id: root
    required property var shell
    readonly property var dex: shell.pokedex
    enabled: !shell.drawerOpen
    readonly property bool takesFocus: visible && !shell.drawerOpen && !dex.journal.open && !shell.menuOpen && !shell.keyboard.open && shell.notice.length === 0
    readonly property bool detailOpen: dex.zone === "detail" || dex.zone === "art"
    readonly property bool artOpen: dex.zone === "art"
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
            x: 0; y: dexHeader.height + 72; width: 470; height: root.height - 45 - y; color: "#dbe8dd"
            ListView {
                id: entries
                objectName: "dex-list"
                x: 19; y: 10; width: parent.width - 54; height: parent.height - 20
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
                onVisibleChanged: Qt.callLater(revealCurrent)
                onHeightChanged: Qt.callLater(revealCurrent)
                Connections { target: root; function onTakesFocusChanged() { Qt.callLater(entries.revealCurrent); } }
                Connections { target: root; function onDetailOpenChanged() { Qt.callLater(entries.revealCurrent); } }
                delegate: Item {
                    required property int index
                    required property var modelData
                    property alias focusControl: entryButton
                    width: entries.width; height: 65
                    CapButton {
                        id: entryButton
                        objectName: "dex-entry-" + modelData.id
                        x: 5; y: 5; width: parent.width - 10; height: 52
                        label: modelData.number + "   " + modelData.name + (modelData.favorite ? "  ★" : "")
                        detail: modelData.types + "  ·  " + modelData.status
                        contentInset: 73
                        tint: modelData.favorite ? Theme.yellow : Theme.green
                        selected: root.takesFocus && root.dex.zone === "list" && root.dex.entryIndex === index
                        onActivated: root.shell.activate(index, "list")
                        ClassicIllustration { x: 13; y: 2; width: 48; height: 48; art: modelData.art || ({}) }
                    }
                }
            }
            Text {
                x: 28; y: 31; width: parent.width - 56; wrapMode: Text.WordWrap
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
            id: preview
            objectName: "dex-list-preview"
            x: 470; y: dexHeader.height + 72; width: parent.width - x; height: root.height - 45 - y; color: "#e6edde"
            readonly property bool hasEntry: root.dex.entries.length > 0
            readonly property var types: root.dex.detail.types ? root.dex.detail.types.split(" / ") : []
            function typeTint(type) {
                const colors = {Bug:"#cddd9c", Dark:"#c6b9b0", Dragon:"#bbbbe8", Electric:"#f5dc83",
                    Fairy:"#efc3df", Fighting:"#e5b399", Fire:"#f1b18a", Flying:"#cad7ef", Ghost:"#c8bde0",
                    Grass:"#b8d999", Ground:"#dec59e", Ice:"#b5e0e0", Normal:"#d7d4bc", Poison:"#dbb5dc",
                    Psychic:"#edb2c6", Rock:"#d1c492", Steel:"#c3d2d8", Water:"#accfea"}
                return colors[type] || "#d7dfd0"
            }
            Text {
                x: 18; y: 8; width: parent.width - 108; height: 31
                text: root.dex.detail.name; textFormat: Text.PlainText
                color: Theme.ink; font.pixelSize: 25; font.weight: Font.DemiBold
                fontSizeMode: Text.Fit; minimumPixelSize: 18; elide: Text.ElideRight
            }
            Text { x: parent.width - 82; y: 15; width: 64; text: root.dex.detail.number; color: Theme.muted; font.pixelSize: 18; horizontalAlignment: Text.AlignRight }
            Text { x: 19; y: 38; width: parent.width - 156; text: root.dex.detail.form; color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight }
            ClassicIllustration {
                objectName: "dex-preview-art"
                x: 8; y: 52; width: parent.width - 143; height: parent.height - 56
                art: root.dex.detail.art || ({}); showLabel: true; visible: preview.hasEntry
            }
            Column {
                x: parent.width - 124; y: 47; width: 106; spacing: 3
                visible: preview.hasEntry
                Column {
                    width: parent.width; spacing: 4
                    Repeater {
                        model: preview.types
                        delegate: Rectangle {
                            required property string modelData
                            width: 106; height: 20; radius: 5; color: preview.typeTint(modelData)
                            border.color: Qt.darker(color, 1.18)
                            Text { anchors.centerIn: parent; text: parent.modelData; color: Theme.ink; font.pixelSize: 12; font.weight: Font.DemiBold }
                        }
                    }
                }
                Column {
                    spacing: 1
                    Text { text: "HEIGHT"; color: Theme.muted; font.pixelSize: 10; font.letterSpacing: 1 }
                    Text { text: root.dex.detail.height; color: Theme.ink; font.pixelSize: 17; font.weight: Font.DemiBold }
                }
                Column {
                    spacing: 1
                    Text { text: "WEIGHT"; color: Theme.muted; font.pixelSize: 10; font.letterSpacing: 1 }
                    Text { text: root.dex.detail.weight; color: Theme.ink; font.pixelSize: 17; font.weight: Font.DemiBold }
                }
                Column {
                    spacing: 1
                    Text { text: "YOUR JOURNAL"; color: Theme.muted; font.pixelSize: 9; font.letterSpacing: 0.6 }
                    Text { text: root.dex.detail.status; color: Theme.muted; font.pixelSize: 11 }
                }
            }
            Text {
                x: 20; y: 80; width: parent.width - 40; visible: !preview.hasEntry
                text: "Try another trail through the guide."; wrapMode: Text.WordWrap
                color: Theme.muted; font.pixelSize: 16
            }
        }
        MountedPanel {
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
            height: 45; color: "#c4dcd5"
            Text { x: 27; y: 12; text: root.dex.entries.length + " entries · " + root.dex.source; color: Theme.muted; font.pixelSize: 12 }
            Text { x: 295; y: 12; text: root.dex.artCoverage; color: Theme.muted; font.pixelSize: 12 }
            Text { x: 569; y: 12; text: "X Search  ·  ← / → Jump 8  ·  ↑ Filters"; color: Theme.muted; font.pixelSize: 12 }
        }
    }

    Item {
        anchors.fill: parent; visible: root.detailOpen
        PageHeader {
            width: parent.width - (root.dex.spritePreview.url ? 105 : 0)
            eyebrow: "POKÉDEX / " + root.dex.detail.number
            title: root.dex.detail.name; trailing: root.dex.detail.form
        }
        SpriteActor {
            objectName: "dex-idle-sprite"
            x: parent.width - 112; y: 0; width: 112; height: 86
            visible: root.dex.zone === "detail" && !!root.dex.spritePreview.url
            clips: root.dex.spriteClips; playing: root.takesFocus
        }
        Text { x: 29; y: 95; text: root.dex.detail.types; color: Theme.muted; font.pixelSize: 19 }
        Text { x: 420; y: 98; width: parent.width - 448; text: "Height " + root.dex.detail.height + "   ·   Weight " + root.dex.detail.weight; color: Theme.muted; font.pixelSize: 16; horizontalAlignment: Text.AlignRight }
        ClassicIllustration { objectName: "dex-detail-art"; x: 20; y: 119; width: 240; height: 164; art: root.dex.detail.art || ({}); showLabel: true }
        Text { x: 37; y: 286; text: "↑ Illustrations"; color: Theme.muted; font.pixelSize: 12 }
        Rectangle {
            x: 274; y: 126; width: 280; height: 166; color: "#d6e4d8"
            Text { x: 25; y: 8; text: "BASE STATS · REFERENCE"; color: Theme.muted; font.pixelSize: 11; font.letterSpacing: 1 }
            Repeater {
                model: root.dex.detail.stats
                delegate: Item {
                    required property int index
                    required property var modelData
                    x: 25; y: 27 + index * 22; width: 242; height: 19
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
            x: 577; y: 120; width: parent.width - 602; spacing: 6
            Text { text: sessionState.persistent ? "YOUR JOURNAL · MANUALLY RECORDED" : "YOUR JOURNAL · SAMPLE DATA"; color: Theme.muted; font.pixelSize: 11; font.letterSpacing: 1 }
            Text { width: parent.width; text: "Seen: " + root.dex.detail.seen + "\nCaught: " + root.dex.detail.caught; color: Theme.ink; font.pixelSize: 16 }
            Text { width: parent.width; text: root.dex.detail.notes.length ? root.dex.detail.notes : "Select opens your field journal. Unknown records stay unknown."; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 15; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
            Text { width: parent.width; text: "Regional lists: " + (root.dex.detail.worlds.length ? root.dex.detail.worlds : "National guide only"); color: Theme.muted; font.pixelSize: 13; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
            Text { width: parent.width; text: "Family: " + (root.dex.detail.family.length ? root.dex.detail.family : "Not available in this reference"); color: Theme.muted; font.pixelSize: 13; elide: Text.ElideRight }
        }
        MountedPanel {
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
            height: 78; color: "#c4dcd5"
            CapButton {
                objectName: "dex-favorite"
                x: 28; y: 17; width: 231; height: 46; tint: Theme.yellow
                label: root.dex.saving ? "Saving…" : root.dex.detail.favorite ? "A   Remove favorite" : "A   Add favorite"
                selected: root.takesFocus && !root.artOpen && root.detailOpen && root.dex.focusIndex === 0
                onActivated: root.shell.activate(0)
            }
            CapButton { x: 277; y: 17; width: 215; height: 46; tint: Theme.green; label: "Select  Journal"; enabled: root.dex.detail.editable; onActivated: root.dex.editJournal() }
            CapButton { x: 510; y: 17; width: 176; height: 46; tint: Theme.pink; label: "X   Form"; enabled: root.dex.detail.formCount > 1; opacity: enabled ? 1 : 0.5; onActivated: root.dex.cycleForm() }
            CapButton {
                objectName: "dex-back"
                x: 704; y: 17; width: 184; height: 46; tint: Theme.blue; label: "B   Back to entries"
                selected: root.takesFocus && !root.artOpen && root.detailOpen && root.dex.focusIndex === 1
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
    ArtworkPanel { anchors.fill: parent; dex: root.dex; visible: root.artOpen; takesFocus: root.takesFocus && root.artOpen }
}
