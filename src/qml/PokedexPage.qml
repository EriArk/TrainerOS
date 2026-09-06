import QtQuick

Item {
    id: root
    required property var shell
    readonly property var dex: shell.pokedex
    readonly property bool takesFocus: visible && !shell.menuOpen && !shell.keyboard.open && shell.notice.length === 0
    readonly property bool detailOpen: dex.zone === "detail"
    readonly property bool pickerOpen: dex.zone === "picker"

    Item {
        anchors.fill: parent; visible: !root.detailOpen
        Text { x: 28; y: 9; text: "Pokédex"; color: Theme.ink; font.pixelSize: 32; font.weight: Font.DemiBold }
        Text { x: 230; y: 24; text: "A field guide for your discoveries"; color: Theme.muted; font.pixelSize: 15 }
        Rectangle {
            x: 0; y: 55; width: parent.width; height: 72; color: "#c4dcd5"
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
            Rectangle { x: 0; y: -55; width: 6; height: 55; color: "#c4dcd5" }
            Rectangle { anchors.right: parent.right; y: -55; width: 6; height: 55; color: "#c4dcd5" }
        }
        Rectangle {
            x: 0; y: 127; width: 512; height: 238; color: "#dbe8dd"
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
            x: 530; y: 127; width: parent.width - x; height: 238; color: "#e6edde"
            Rectangle {
                x: 24; y: 20; width: 117; height: 117; radius: 59; color: "#f6f5e5"; border.color: "#abc9b0"; border.width: 2
                TrainerEmblem { x: 12; y: 12; width: 93; height: 93; emblem: "spark" }
            }
            Text { x: 160; y: 31; text: root.dex.detail.number; color: Theme.muted; font.pixelSize: 24 }
            Text { x: 160; y: 77; text: "FIELD NOTES"; color: Theme.muted; font.pixelSize: 12; font.letterSpacing: 1.4 }
            Text { x: 24; y: 145; width: parent.width - 48; text: root.dex.detail.name; color: Theme.ink; font.pixelSize: 26; font.weight: Font.DemiBold; elide: Text.ElideRight }
            Text { x: 24; y: 186; width: parent.width - 48; text: root.dex.entries.length ? root.dex.detail.types + " · " + root.dex.detail.status : "Try another trail through the guide."; color: Theme.muted; font.pixelSize: 15; wrapMode: Text.WordWrap }
        }
        Rectangle {
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
            height: 45; color: "#c4dcd5"
            Text { x: 27; y: 13; text: root.dex.entries.length + " entries · sample guide"; color: Theme.muted; font.pixelSize: 14 }
            Text { x: 320; y: 13; text: "↑ Filters above first entry   ·   A Open   ·   ↓ Browse"; color: Theme.muted; font.pixelSize: 14 }
        }
    }

    Item {
        anchors.fill: parent; visible: root.detailOpen
        Text { x: 28; y: 16; text: "POKÉDEX / " + root.dex.detail.number; color: Theme.muted; font.pixelSize: 13; font.letterSpacing: 1.2 }
        Text { x: 28; y: 42; text: root.dex.detail.name; color: Theme.ink; font.pixelSize: 36; font.weight: Font.DemiBold }
        Text { x: 29; y: 95; text: root.dex.detail.types; color: Theme.muted; font.pixelSize: 19 }
        Rectangle {
            x: 0; y: 135; width: 276; height: 197; color: "#e6edde"
            TrainerEmblem { x: 57; y: 17; width: 162; height: 162; emblem: "spark" }
        }
        Column {
            x: 302; y: 153; spacing: 12
            Text { text: sessionState.persistent ? "YOUR RECORD · LOCAL FAVORITES" : "YOUR RECORD · SAMPLE DATA"; color: Theme.muted; font.pixelSize: 12; font.letterSpacing: 1.2 }
            Text { text: "Seen: " + root.dex.detail.seen + "    ·    Caught: " + root.dex.detail.caught; color: Theme.ink; font.pixelSize: 21 }
            Text { text: root.dex.detail.favorite ? "★ In your favorites" : "No favorite mark"; color: Theme.ink; font.pixelSize: 20 }
            Text { text: "Sample collections: " + root.dex.detail.worlds; color: Theme.muted; font.pixelSize: 16 }
            Text { text: sessionState.persistent ? "Favorites are saved here. Game progress isn't connected yet." : "Favorites last until this preview closes."; color: Theme.muted; font.pixelSize: 14 }
        }
        Rectangle {
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
            height: 78; color: "#c4dcd5"
            CapButton {
                objectName: "dex-favorite"
                x: 28; y: 17; width: 290; height: 46; tint: Theme.yellow
                label: root.dex.saving ? "Saving…" : root.dex.detail.favorite ? "Remove favorite" : "Add favorite"
                selected: root.takesFocus && root.detailOpen && root.dex.focusIndex === 0
                onActivated: root.shell.activate(0)
            }
            CapButton {
                objectName: "dex-back"
                x: 336; y: 17; width: 290; height: 46; tint: Theme.blue; label: "Back to entries"
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
}
