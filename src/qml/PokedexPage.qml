import QtQuick

Item {
    id: root
    required property var shell
    readonly property var dex: shell.pokedex
    enabled: !shell.drawerOpen
    readonly property bool takesFocus: visible && !shell.drawerOpen && !shell.menuOpen && !shell.keyboard.open && shell.notice.length === 0
    readonly property bool artOpen: dex.zone === "art"
    readonly property bool pickerOpen: dex.zone === "picker"

    Item {
        anchors.fill: parent
        PageHeader { id: dexHeader; compact: true; title: "Pokédex"; trailing: root.dex.saveTotals; subtitle: root.dex.saveCaption }
        MountedPanel {
            x: 0; y: dexHeader.height; width: parent.width; height: 62; color: "#c4dcd5"
            Row {
                x: 14; y: 7; spacing: 8
                Repeater {
                    model: root.dex.rail
                    delegate: CapButton {
                        required property int index
                        required property var modelData
                        objectName: "dex-rail-" + index
                        width: (root.width - 84) / 8; height: 46; textSize: 14; label: modelData.label; detail: modelData.value
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
            x: 0; y: dexHeader.height + 62; width: 330; height: root.height - y; color: "#dbe8dd"
            ListView {
                id: entries
                objectName: "dex-list"
                x: 19; y: 10; width: parent.width - 38; height: parent.height - 20
                model: root.dex.entries; currentIndex: root.dex.entryIndex
                interactive: false; keyNavigationEnabled: false; clip: true
                boundsBehavior: Flickable.StopAtBounds
                highlightMoveDuration: 0; cacheBuffer: 216
                function revealCurrent() {
                    if (count > 0) positionViewAtIndex(currentIndex, ListView.Contain);
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
                        contentInset: 60; textSize: 14
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
                x: 24; y: 163; width: parent.width - 48; height: 48; tint: Theme.yellow
                visible: root.dex.entries.length === 0; label: root.dex.recoveryLabel
                selected: root.takesFocus && root.dex.zone === "recovery"
                onActivated: root.shell.activate(0, "recovery")
            }
        }
        Rectangle {
            id: preview
            objectName: "dex-list-preview"
            x: 330; y: dexHeader.height + 62; width: parent.width - x; height: root.height - y
            color: "#e6edde"
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
                id: speciesName
                x: 18; y: 8; width: parent.width - 185; height: 32
                text: root.dex.detail.name; textFormat: Text.PlainText
                color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 27; font.bold: true
                fontSizeMode: Text.Fit; minimumPixelSize: 18; elide: Text.ElideRight
            }
            Text { x: 19; y: 42; width: 237; text: root.dex.detail.number + "  ·  " + root.dex.detail.form; color: Theme.muted; font.pixelSize: 12; elide: Text.ElideRight }
            SpriteActor {
                objectName: "dex-idle-sprite"
                x: speciesName.x + Math.min(speciesName.width, speciesName.contentWidth) + 12
                y: 0; width: Math.max(86, parent.width - x - 9); height: 50
                visible: preview.hasEntry && !!root.dex.spritePreview.url
                clips: root.dex.spriteClips; playing: root.takesFocus && !root.pickerOpen && !root.artOpen
            }
            Row {
                x: 19; y: 67; spacing: 6; visible: preview.hasEntry
                Repeater {
                    model: preview.types
                    delegate: Rectangle {
                        required property string modelData
                        width: 106; height: 22; radius: 6; color: preview.typeTint(modelData)
                        border.color: Qt.darker(color, 1.2)
                        Text { anchors.centerIn: parent; text: parent.modelData; color: Theme.ink; font.pixelSize: 12; font.bold: true }
                    }
                }
            }
            ClassicIllustration {
                objectName: "dex-detail-art"
                x: 10; y: 94; width: 246; height: parent.height - y - 35
                art: root.dex.detail.art || ({}); showLabel: true; visible: preview.hasEntry
            }
            Text { x: 19; anchors.bottom: parent.bottom; anchors.bottomMargin: 12
                text: root.dex.detail.height + "  ·  " + root.dex.detail.weight; color: Theme.muted; font.pixelSize: 14; visible: preview.hasEntry }
            Item {
                id: stats; objectName: "dex-stat-board"
                x: 273; y: 52; width: parent.width - x - 18; height: 126; visible: preview.hasEntry
                Text { text: "BASE STATS"; color: Theme.muted; font.pixelSize: 10; font.letterSpacing: 1.5 }
                Grid {
                    y: 22; width: parent.width; columns: 3; spacing: 8
                    Repeater {
                        model: root.dex.detail.stats
                        delegate: Rectangle {
                            required property int index
                            required property var modelData
                            readonly property var tints: ["#efacb1", "#f1bf88", "#e9d185", "#acc8ee", "#b5d5a1", "#cdb5e3"]
                            width: (stats.width - 16) / 3; height: 47; radius: 9
                            color: tints[index]; border.color: Qt.darker(color, 1.2)
                            Rectangle { anchors.fill: parent; anchors.margins: 2; radius: 7; color: "transparent"; border.color: "#60ffffff" }
                            Text { x: 10; y: 5; text: modelData.label; color: Qt.darker(parent.color, 2.6); font.pixelSize: 11; font.bold: true }
                            Text { anchors.right: parent.right; anchors.rightMargin: 10; y: 16; text: modelData.value; color: Qt.darker(parent.color, 3.2); font.family: Theme.displayFamily; font.pixelSize: 25; font.bold: true }
                        }
                    }
                }
            }
            Column {
                x: stats.x; y: stats.y + stats.height + 5; width: stats.width; spacing: 3; visible: preview.hasEntry
                Rectangle {
                    width: parent.width; height: 25; radius: 6
                    color: root.dex.detail.status === "Caught" ? "#badc9c" : root.dex.detail.status === "Seen" ? "#f0d08c" : "#c7d9e4"
                    Text { x: 8; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 16
                        text: root.dex.detail.recordSource + ": " + root.dex.detail.status + (root.dex.detail.favorite ? "  ★" : "")
                        textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 12; font.bold: true; elide: Text.ElideRight }
                }
                Text { width: parent.width; text: root.dex.detail.family; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight }
            }
            Text { x: 20; y: 95; width: parent.width - 40; visible: !preview.hasEntry; text: "Try another trail through the guide."; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: 16 }
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
    ArtworkPanel { anchors.fill: parent; dex: root.dex; visible: root.artOpen; takesFocus: root.takesFocus && root.artOpen }
}
