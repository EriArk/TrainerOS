import QtQuick

Item {
    id: root
    required property var shell
    readonly property var model: shell.multiverse
    readonly property bool takesFocus: visible && !shell.collectionManager.open && !shell.serviceOpen && !shell.libraryTools.open && !shell.scraper.open && !shell.menuOpen && !shell.keyboard.open && !shell.notice.length
    PageHeader {
        id: header
        title: root.model.collection !== "multiverse" ? root.model.collectionName : root.model.route === "systems" ? "All games" : root.model.systemName
        compact: root.model.route === "systems"
        subtitle: root.model.route === "systems" ? "" : root.model.sample ? "Development preview · fictional titles · no launch" : root.model.collection !== "multiverse" ? "Across your library" : "Your library across systems"
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
            cellWidth: width / 3; cellHeight: height/2; clip: true
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
                PlatformCard {
                    id: systemCard
                    objectName: "multiverse-system-" + index
                    x: 8; y: 8; width: parent.width - 16; height: parent.height-16
                    entry: modelData
                    selected: root.takesFocus && parent.visible && root.model.focusIndex === index
                    onActivated: root.shell.activate(index)
                }
            }
        }
        LibraryWheel {
            anchors.fill: parent; visible: root.model.route === "games"
            entries: root.model.games; entry: root.model.detail; selectionIndex: root.model.focusIndex
            takesFocus: root.takesFocus
            previewsEnabled: root.shell.settings.videoPreviews && !root.shell.serviceOpen && !adventureLaunch.active && !sessionState.blocked
            filterText: root.model.filterLabel + (root.model.query ? " · " + root.model.query : "")
            actionVisible: entry.linked === true
            emptyTitle: root.model.query || root.model.filterLabel !== "All titles" ? "No matching titles" : "This collection is empty"
            emptyDetail: root.model.query || root.model.filterLabel !== "All titles" ? "Reset search and filter" : "Add games or edit collection rules"
            onActivated: index => root.shell.activate(index)
            onEmptyActivated: { if(root.model.collection.startsWith("user:") && !root.model.query && root.model.filterLabel === "All titles") root.shell.manageCollection(false); else root.shell.activate(0) }
        }
    }
}
