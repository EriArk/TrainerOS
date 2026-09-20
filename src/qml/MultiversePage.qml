import QtQuick

Item {
    id: root
    required property var shell
    readonly property var model: shell.multiverse
    readonly property bool takesFocus: visible && !shell.menuOpen && !shell.keyboard.open && !shell.notice.length
    PageHeader {
        id: header
        title: root.model.route === "systems" ? "Multiverse" : root.model.systemName
        subtitle: root.model.sample ? "Development preview · fictional titles · no launch" : "Library setup is coming · your Pokémon Worlds are unchanged"
    }
    MountedPanel {
        y: header.height; width: parent.width; height: parent.height - y; color: "#d9deed"
        Grid {
            x: 22; y: 18; columns: 3; spacing: 16
            visible: root.model.route === "systems"
            Repeater {
                model: root.model.systems
                CapButton {
                    required property int index
                    required property var modelData
                    objectName: "multiverse-system-" + index
                    width: (root.width - 76) / 3; height: 120
                    label: modelData.name; detail: "Browse titles"; contentInset: 74
                    tint: Theme.tabColors[index % Theme.tabColors.length]
                    selected: root.takesFocus && parent.visible && root.model.focusIndex === index
                    onActivated: root.shell.activate(index)
                    SystemGlyph { x: 12; y: 25; width: 50; height: 62; shape: modelData.shape }
                }
            }
        }
        Item {
            anchors.fill: parent; visible: root.model.route === "games"
            Text { x: 24; y: 15; width: parent.width - 48; text: root.model.filterLabel + (root.model.query ? " · " + root.model.query : ""); color: Theme.ink; font.pixelSize: 17; elide: Text.ElideRight; textFormat: Text.PlainText }
            ListView {
                id: list
                x: 18; y: 46; width: parent.width - 36; height: parent.height - 104
                clip: true; interactive: false; keyNavigationEnabled: false
                model: root.model.games; currentIndex: root.model.focusIndex
                function reveal() {
                    if (!visible || !root.takesFocus || !currentItem) return
                    positionViewAtIndex(currentIndex, ListView.Contain)
                    currentItem.control.forceActiveFocus(Qt.OtherFocusReason)
                }
                onCurrentIndexChanged: Qt.callLater(reveal)
                onCurrentItemChanged: Qt.callLater(reveal)
                onVisibleChanged: Qt.callLater(reveal)
                Connections { target: root; function onTakesFocusChanged() { Qt.callLater(list.reveal) } }
                delegate: Item {
                    required property int index
                    required property var modelData
                    width: list.width; height: 78
                    property alias control: card
                    CapButton {
                        id: card; x: 6; y: 6; width: parent.width - 12; height: 62
                        objectName: "multiverse-game-" + index
                        label: modelData.title; detail: modelData.status
                        tint: modelData.linked ? "#a9c9eb" : "#c2c9d2"
                        selected: root.takesFocus && root.model.route === "games" && root.model.focusIndex === index
                        onActivated: root.shell.activate(index)
                    }
                }
            }
            CapButton {
                objectName: "multiverse-empty"
                x: 24; y: 65; width: parent.width - 48; height: 95
                visible: root.model.games.length === 0
                label: root.model.query || root.model.filterLabel !== "All titles" ? "No matching titles" : "No titles connected yet"
                detail: root.model.query || root.model.filterLabel !== "All titles" ? "A · Reset search and filter" : "A · Back to systems"
                selected: visible && root.takesFocus && root.model.route === "games"
                onActivated: root.shell.activate(0)
            }
            Row { x: 24; anchors.bottom: parent.bottom; anchors.bottomMargin: 15; spacing: 28
                Hint { button: "X"; label: "Search"; labelColor: Theme.ink }
                Hint { button: "Y"; label: "Filter"; labelColor: Theme.ink }
                Hint { button: "B"; label: "Systems"; labelColor: Theme.ink }
            }
        }
        Item {
            anchors.fill: parent; visible: root.model.route === "detail"
            Column { x: 26; y: 24; spacing: 16; width: parent.width - 52
                Text { width: parent.width; text: root.model.detail.title || ""; textFormat: Text.PlainText; wrapMode: Text.WordWrap; maximumLineCount: 2; color: Theme.ink; font.pixelSize: 32; font.bold: true; elide: Text.ElideRight }
                Text { text: root.model.detail.status || ""; color: Theme.muted; font.pixelSize: 19 }
                Text { width: parent.width; text: "This is a layout sample. File linking and play will arrive with the real library."; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: 18 }
            }
            Row { x: 26; anchors.bottom: parent.bottom; anchors.bottomMargin: 26; spacing: 22
                CapButton { objectName: "multiverse-select"; width: 285; height: 62; label: "Choose for Home"; enabled: root.model.detail.linked === true; selected: parent.parent.visible && root.takesFocus && root.model.focusIndex === 0; onActivated: root.shell.activate(0) }
                CapButton { objectName: "multiverse-back"; width: 160; height: 62; label: "Back"; tint: Theme.blue; selected: parent.parent.visible && root.takesFocus && root.model.focusIndex === 1; onActivated: root.shell.activate(1) }
            }
        }
    }
}
