import QtQuick

Item {
    id: root
    required property var shell
    readonly property var model: shell.multiverse
    readonly property bool takesFocus: visible && !(root.pc && shell.pcAddFocused) && shell.pairedNavigationAvailable && !shell.collectionManager.open && !shell.serviceOpen && !shell.libraryTools.open && !shell.scraper.open && !shell.menuOpen && !shell.keyboard.open && !shell.notice.length
    readonly property bool pc: model.route === "games" && model.systemId === "pc"
    readonly property bool emptyPc: pc && model.games.length === 0
    CapButton { anchors.right: parent.right; anchors.rightMargin: 12; y: 0; z: 2; visible: root.pc && !root.emptyPc
        width: 134; height: 34; label: "+ Add game"; tint: Theme.green; selected: shell.pcAddFocused
        onActivated: shell.pcGames.begin() }
    PageHeader {
        id: header; visible: root.model.route !== "games"; height: visible ? implicitHeight : 0; width: parent.width - (root.model.route === "games" ? 240 : 0)
        title: root.model.collection !== "multiverse" ? root.model.collectionName : root.model.route === "systems" ? "All games" : root.model.systemName
        compact: root.model.route === "systems"
        subtitle: root.model.route === "systems" ? "" : root.model.sample ? "Development preview · fictional titles · no launch" : root.model.collection !== "multiverse" ? "Across your library" : "Your library across systems"
    }
    Item {
        y: root.pc ? 42 : header.height; width: parent.width; height: parent.height - y
        Column {
            anchors.centerIn: parent; spacing: 14; visible: root.emptyPc
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Your PC games"; font.family: Theme.displayFamily; font.pixelSize: 29; color: Theme.ink }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Bring a game folder into your library."; font.pixelSize: 17; color: Theme.muted }
            CapButton { anchors.horizontalCenter: parent.horizontalCenter; width: 194; height: 44; label: "+ Add game"; tint: Theme.green
                selected: root.takesFocus || root.shell.pcAddFocused; onActivated: root.shell.pcGames.begin() }
        }
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
            model: visible ? root.model.systems : []; currentIndex: root.model.focusIndex
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
        Connections {
            target: root.model
            function onScrollRequested(direction) { if (root.takesFocus && browser.visible) browser.scrollDescription(direction) }
        }
        LibraryGrid {
            anchors.fill: parent
            visible: root.model.route === "games" && !root.emptyPc && root.model.viewStyle === "grid"
            entries: visible ? root.model.games : []; selectionIndex: root.model.focusIndex
            takesFocus: root.takesFocus && visible
            onColumnsChanged: if (visible) root.model.gridColumns = columns
            onVisibleChanged: if (visible) root.model.gridColumns = columns
            onFocusRequested: index => root.shell.focusLibraryGame(index)
            onActivated: index => root.shell.activate(index)
            onFavoriteRequested: gameId => root.shell.toggleLibraryFavorite(gameId)
            onEmptyActivated: browser.emptyActivated()
        }
        LibraryShelves {
            anchors.fill: parent
            visible: root.model.route === "games" && !root.emptyPc && root.model.viewStyle === "shelves"
            entries: visible ? root.model.games : []; selectionIndex: root.model.focusIndex
            columns: root.model.shelfColumns; takesFocus: root.takesFocus && visible
            onFocusRequested: index => root.shell.focusLibraryGame(index)
            onActivated: index => root.shell.activate(index)
            onFavoriteRequested: gameId => root.shell.toggleLibraryFavorite(gameId)
            onEmptyActivated: browser.emptyActivated()
        }
        LibraryWheel {
            id: browser
            favoritesAvailable: true
            viewStyle: root.model.viewStyle; listPageSize: root.model.listPageSize
            onFocusRequested: index => root.shell.focusLibraryGame(index)
            anchors.fill: parent; visible: root.model.route === "games" && !root.emptyPc && (root.model.viewStyle === "wheel" || root.model.viewStyle === "list")
            entries: visible ? root.model.games : []; entry: root.model.detail; selectionIndex: root.model.focusIndex
            takesFocus: root.takesFocus && visible
            previewsEnabled: root.shell.settings.videoPreviews && !root.shell.serviceOpen && !adventureLaunch.active && !sessionState.blocked
            filterText: root.model.filterLabel + (root.model.query ? " · " + root.model.query : "")
            actionVisible: entry.linked === true
            emptyTitle: root.pc ? "Your PC games, at home here" : root.model.query || root.model.filterLabel !== "All titles" ? "No matching titles" : "This collection is empty"
            emptyDetail: root.pc ? "Add a game folder to get started" : root.model.query || root.model.filterLabel !== "All titles" ? "Reset search and filter" : "Add games or edit collection rules"
            onActivated: index => root.shell.activate(index)
            onFavoriteRequested: gameId => root.shell.toggleLibraryFavorite(gameId)
            onEmptyActivated: { if(root.pc)root.shell.pcGames.begin(); else if(root.model.collection.startsWith("user:") && !root.model.query && root.model.filterLabel === "All titles") root.shell.manageCollection(false); else root.shell.activate(0) }
        }
    }
}
