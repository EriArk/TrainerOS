import QtQuick

Item {
    id: root
    required property var shell
    readonly property var model: shell.multiverse
    readonly property bool takesFocus: visible && !shell.menuOpen && !shell.keyboard.open && !shell.notice.length
    PageHeader {
        id: header
        title: root.model.route === "systems" ? "Multiverse" : root.model.systemName
        subtitle: root.model.sample ? "Development preview · fictional titles · no launch" : "Your worlds beyond Pokémon"
    }
    MountedPanel {
        y: header.height; width: parent.width; height: parent.height - y; color: "#d9deed"
        Text {
            anchors.centerIn: parent
            visible: root.model.route === "systems" && root.model.systems.length === 0
            text: "No Adventures yet"
            color: Theme.muted; font.pixelSize: 26
        }
        GridView {
            id: systemsGrid
            x: 16; y: 12; width: parent.width - 32; height: parent.height - 24
            cellWidth: width / 3; cellHeight: 110; clip: true
            interactive: false; keyNavigationEnabled: false
            visible: root.model.route === "systems"
            model: root.model.systems; currentIndex: root.model.focusIndex
            function reveal() {
                if (!visible || !root.takesFocus || !currentItem) return
                positionViewAtIndex(currentIndex, GridView.Contain)
                currentItem.control.forceActiveFocus(Qt.OtherFocusReason)
            }
            onCurrentIndexChanged: Qt.callLater(reveal)
            onCurrentItemChanged: Qt.callLater(reveal)
            onVisibleChanged: Qt.callLater(reveal)
            Connections { target: root; function onTakesFocusChanged() { Qt.callLater(systemsGrid.reveal) } }
            delegate: Item {
                required property int index
                required property var modelData
                width: systemsGrid.cellWidth; height: systemsGrid.cellHeight
                property alias control: systemCard
                CapButton {
                    id: systemCard
                    objectName: "multiverse-system-" + index
                    x: 8; y: 8; width: parent.width - 16; height: 94
                    label: modelData.name; detail: modelData.count ? modelData.count + (modelData.count === 1 ? " Adventure" : " Adventures") : "Browse titles"; contentInset: 64
                    tint: Theme.tabColors[index % Theme.tabColors.length]
                    selected: root.takesFocus && parent.visible && root.model.focusIndex === index
                    onActivated: root.shell.activate(index)
                    SystemGlyph { x: 10; y: 19; width: 42; height: 54; shape: modelData.shape }
                }
            }
        }
        LibraryWheel {
            anchors.fill: parent; visible: root.model.route === "games"
            entries: root.model.games; entry: root.model.detail; selectionIndex: root.model.focusIndex
            takesFocus: root.takesFocus
            filterText: root.model.filterLabel + (root.model.query ? " · " + root.model.query : "")
            actionVisible: entry.linked === true
            emptyTitle: root.model.query || root.model.filterLabel !== "All titles" ? "No matching titles" : "No titles connected yet"
            emptyDetail: root.model.query || root.model.filterLabel !== "All titles" ? "Reset search and filter" : "Back to systems"
            onActivated: index => root.shell.activate(index)
            onEmptyActivated: root.shell.activate(0)
        }
    }
}
