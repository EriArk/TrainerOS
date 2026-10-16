import QtQuick

Item {
    id: root
    objectName: "library-shelves"
    required property var entries
    required property int selectionIndex
    required property bool takesFocus
    required property int columns
    signal focusRequested(int index)
    signal activated(int index)
    signal favoriteRequested(string gameId)
    signal emptyActivated()
    // Small collections get one generous shelf; larger libraries keep two rows.
    readonly property int visibleRows: entries.length <= columns ? 1 : 2
    readonly property int displayColumns: visibleRows === 1 ? Math.max(1,entries.length) : columns
    readonly property int rowCount: Math.max(visibleRows, Math.ceil(entries.length / columns))
    readonly property real rowHeight: shelves.height / visibleRows
    readonly property int firstRow: Math.max(0,shelves.indexAt(1,shelves.contentY+1))
    function reveal() {
        if (!visible || !takesFocus || shelves.dragging || shelves.flicking) return
        const row = Math.floor(selectionIndex/columns)
        shelves.positionViewAtIndex(row,ListView.Contain)
        const item = shelves.itemAtIndex(row)
        if (item) item.focusCover(selectionIndex%columns)
    }
    function scrollRows(direction) {
        if (!takesFocus) return
        const next = Math.max(0,Math.min(Math.max(0,rowCount-visibleRows),firstRow+direction))
        shelves.positionViewAtIndex(next,ListView.Beginning)
        settleSelection()
    }
    function settleSelection() {
        if (!takesFocus || entries.length===0) return
        const row = Math.floor(selectionIndex/columns)
        const first = firstRow
        if (row < first || row >= first+visibleRows) {
            focusRequested(Math.min(entries.length-1,Math.max(first,Math.min(first+visibleRows-1,row))*columns + selectionIndex%columns))
            return
        }
        Qt.callLater(reveal)
    }
    onSelectionIndexChanged: { shelves.cancelFlick(); Qt.callLater(reveal) }
    onTakesFocusChanged: Qt.callLater(reveal)
    onVisibleChanged: if (visible) Qt.callLater(reveal)
    onEntriesChanged: Qt.callLater(reveal)
    onRowHeightChanged: { shelves.cancelFlick(); Qt.callLater(reveal) }
    ListView {
        id: shelves; objectName: "shelves-scroll"
        x: 14; y: 5; width: parent.width-28; height: parent.height-25
        clip: true; interactive: root.takesFocus; keyNavigationEnabled: false
        model: root.rowCount; spacing: 0; cacheBuffer: 0
        boundsBehavior: Flickable.StopAtBounds; snapMode: ListView.SnapToItem
        maximumFlickVelocity: 1500; flickDeceleration: 3500
        onMovementEnded: root.settleSelection()
        onCountChanged: Qt.callLater(root.reveal)
        delegate: Item {
            id: shelfRow
            required property int index
            width: shelves.width; height: root.rowHeight
            function focusCover(column) { const item=covers.itemAt(column);if(item)item.forceActiveFocus(Qt.OtherFocusReason) }
            readonly property real coverHeight: height-48
            Rectangle {
                x: 6; y: shelfRow.coverHeight+17; width: parent.width-12; height: 11
                gradient: Gradient { GradientStop { position: 0; color: "#22172c29" } GradientStop { position: 1; color: "transparent" } }
            }
            Rectangle {
                x: 4; y: shelfRow.coverHeight+12; width: parent.width-8; height: 7; radius: 2
                gradient: Gradient { GradientStop { position: 0; color: Theme.chassisTop } GradientStop { position: 1; color: Theme.chassis } }
                Rectangle { y: 0; width: parent.width; height: 1; color: Theme.rim }
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.chassisDark }
            }
            Rectangle {
                x: 7; y: shelfRow.coverHeight+8; width: parent.width-14; height: 5; radius: 2
                gradient: Gradient { GradientStop { position: 0; color: Theme.rim } GradientStop { position: 1; color: Theme.screenSlope } }
            }
            Repeater {
                id: covers
                model: Math.max(0,Math.min(root.columns,root.entries.length-shelfRow.index*root.columns))
                ShelfCover {
                    required property int index
                    readonly property int gameIndex: shelfRow.index*root.columns+index
                    objectName: "shelf-game-"+gameIndex
                    x: index*width; y: 8; width: shelfRow.width/root.displayColumns; height: shelfRow.height-8
                    coverHeight: shelfRow.coverHeight
                    entry: root.entries[gameIndex] || ({})
                    selected: root.takesFocus && gameIndex===root.selectionIndex
                    active: root.takesFocus && shelfRow.y+shelfRow.height>shelves.contentY && shelfRow.y<shelves.contentY+shelves.height
                    onActivated: root.activated(gameIndex)
                    onFavoriteRequested: gameId => root.favoriteRequested(gameId)
                }
            }
        }
    }
    Text {
        anchors.horizontalCenter: parent.horizontalCenter; y: 0
        text: "⌃"; color: Theme.muted; font.pixelSize: 15
        visible: root.firstRow>0
        MouseArea { anchors.fill: parent; anchors.margins: -7; enabled: root.takesFocus; onClicked: root.scrollRows(-1) }
    }
    Text {
        anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom
        text: "⌄"; color: Theme.muted; font.pixelSize: 18
        visible: root.firstRow+root.visibleRows<root.rowCount
        MouseArea { anchors.fill: parent; anchors.margins: -7; enabled: root.takesFocus; onClicked: root.scrollRows(1) }
    }
    Text { anchors.right: parent.right; anchors.rightMargin: 20; anchors.bottom: parent.bottom; text: root.entries.length ? (root.selectionIndex+1)+" / "+root.entries.length : ""; color: Theme.muted; font.pixelSize: 11 }
    CapButton {
        anchors.horizontalCenter: parent.horizontalCenter
        y: shelves.y + (root.rowHeight-height)/2 - 8; width: 330; height: 84
        visible: root.entries.length===0; selected: visible && root.takesFocus
        label: "No games on these shelves"; detail: "Choose another collection or reset filters"
        onActivated: root.emptyActivated()
    }
}
