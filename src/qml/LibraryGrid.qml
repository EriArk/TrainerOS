import QtQuick

Item {
    id: root
    objectName: "library-grid"
    required property var entries
    required property int selectionIndex
    required property bool takesFocus
    readonly property int columns: Math.max(5,Math.min(6,Math.floor(tiles.width/140)))
    readonly property int visibleRows: Math.max(2,Math.floor(tiles.height/160))
    signal focusRequested(int index)
    signal activated(int index)
    signal favoriteRequested(string gameId)
    signal emptyActivated()
    function reveal() {
        if (!visible || !takesFocus || tiles.dragging || tiles.flicking || !entries.length) return
        tiles.positionViewAtIndex(selectionIndex,GridView.Contain)
        if (tiles.currentItem) tiles.currentItem.forceActiveFocus(Qt.OtherFocusReason)
    }
    function settleSelection() {
        if (!entries.length || !takesFocus) return
        const first = Math.max(0,Math.round((tiles.contentY-tiles.originY)/tiles.cellHeight)*columns)
        const last = Math.min(entries.length-1,first+visibleRows*columns-1)
        if (selectionIndex < first || selectionIndex > last) root.focusRequested(Math.min(last,first+selectionIndex%columns))
        else Qt.callLater(reveal)
    }
    function turnRow(delta) {
        tiles.contentY = Math.max(tiles.originY,Math.min(tiles.originY+Math.max(0,tiles.contentHeight-tiles.height),tiles.contentY+delta*tiles.cellHeight))
        settleSelection()
    }
    onSelectionIndexChanged: { tiles.userScrolling = false; tiles.cancelFlick(); Qt.callLater(reveal) }
    onVisibleChanged: Qt.callLater(reveal)
    onTakesFocusChanged: Qt.callLater(reveal)
    onEntriesChanged: Qt.callLater(reveal)
    GridView {
        id: tiles; objectName: "grid-scroll"
        x: 12; y: 5; width: parent.width-24; height: parent.height-25
        model: root.entries; currentIndex: root.selectionIndex
        cellWidth: width/root.columns; cellHeight: height/root.visibleRows
        clip: true; cacheBuffer: 0; interactive: root.takesFocus; keyNavigationEnabled: false
        boundsBehavior: Flickable.StopAtBounds; snapMode: GridView.SnapToRow
        maximumFlickVelocity: 1500; flickDeceleration: 3500
        property bool userScrolling: false
        onMovementStarted: if (dragging || flicking) userScrolling = true
        onMovementEnded: { if (userScrolling) root.settleSelection(); userScrolling = false }
        onCurrentItemChanged: Qt.callLater(root.reveal)
        onCellWidthChanged: { userScrolling = false; cancelFlick(); Qt.callLater(root.reveal) }
        onCellHeightChanged: { userScrolling = false; cancelFlick(); Qt.callLater(root.reveal) }
        delegate: Item {
            id: tile
            required property int index
            required property var modelData
            objectName: "grid-game-"+index
            width: tiles.cellWidth; height: tiles.cellHeight
            readonly property bool selected: root.takesFocus && index===root.selectionIndex
            readonly property var artwork: modelData.artwork || ({})
            Accessible.role: Accessible.ListItem
            Accessible.name: modelData.title || ""
            Accessible.onPressAction: if (root.takesFocus) root.activated(index)
            Rectangle {
                x: frame.x+1; y: frame.y+4; width: frame.width; height: frame.height; radius: 9
                color: tile.selected ? "#40172c29" : "#20172c29"
            }
            Rectangle {
                anchors.fill: frame; anchors.margins: -3; radius: 11
                visible: tile.selected; color: "transparent"; border.color: "#65f4cc62"; border.width: 3
            }
            Rectangle {
                id: frame; objectName: "grid-frame-"+tile.index
                readonly property real aspect: tile.artwork.screenshot ? Math.max(.1,preview.imageAspect) : 16/9
                x: (parent.width-width)/2; y: 7+(parent.height-38-height)/2
                width: Math.min(parent.width-18,Math.max(1,parent.height-46)*aspect+8)
                height: (width-8)/aspect+8
                radius: 8; color: Theme.chassisDark
                border.width: tile.selected ? 2 : 1
                border.color: tile.selected ? Theme.focusGlow : Theme.rim
                GamePreview {
                    id: preview; objectName: "grid-picture-"+tile.index; anchors.fill: parent; anchors.margins: 4
                    // Grid explicitly presents screenshots; cover art belongs to Shelves.
                    picture: tile.artwork.screenshot || ""
                    themedFallback: true; fallbackTitle: tile.modelData.title || ""
                    fallbackArt: tile.modelData.logo || ""
                    playbackAllowed: false
                }
                Rectangle { x: 7; y: 2; width: parent.width-14; height: 1; color: "#60ffffff" }
                Text { anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 7; visible: tile.modelData.linked === false; text: "!"; color: Theme.yellow; font.bold: true; font.pixelSize: 18 }
            }
            Text {
                x: 11; y: frame.y+frame.height+5; width: parent.width-22; height: 26
                text: tile.modelData.title || ""; textFormat: Text.PlainText
                font.family: Theme.displayFamily; font.pixelSize: 13; font.bold: tile.selected
                color: tile.selected ? Theme.ink : Theme.muted
                wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            }
            MouseArea { anchors.fill: parent; enabled: root.takesFocus; onClicked: root.activated(tile.index) }
            FavoriteStar {
                objectName: "grid-favorite-"+tile.index
                x: frame.x+frame.width-width-2; y: frame.y+2
                marked: tile.modelData.favorite === true; active: root.takesFocus
                onActivated: root.favoriteRequested(tile.modelData.id)
            }
        }
    }
    Text {
        anchors.horizontalCenter: parent.horizontalCenter; y: -3; text: "⌃"; color: Theme.muted; font.pixelSize: 15
        visible: tiles.contentY>tiles.originY+1
        MouseArea { anchors.fill: parent; anchors.margins: -6; enabled: root.takesFocus; onClicked: root.turnRow(-1) }
    }
    Text {
        anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; text: "⌄"; color: Theme.muted; font.pixelSize: 18
        visible: tiles.contentY+tiles.height<tiles.originY+tiles.contentHeight-1
        MouseArea { anchors.fill: parent; anchors.margins: -6; enabled: root.takesFocus; onClicked: root.turnRow(1) }
    }
    Text { anchors.right: parent.right; anchors.rightMargin: 20; anchors.bottom: parent.bottom; text: root.entries.length ? (root.selectionIndex+1)+" / "+root.entries.length : ""; color: Theme.muted; font.pixelSize: 11 }
    CapButton {
        anchors.centerIn: parent; width: 330; height: 84
        visible: root.entries.length===0; selected: visible && root.takesFocus
        label: "No matching games"; detail: "Choose another collection or reset filters"
        onActivated: root.emptyActivated()
    }
}
