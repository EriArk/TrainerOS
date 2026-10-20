import QtQuick
import QtQuick.Effects

Item {
    id: root
    required property var shell
    readonly property bool takesFocus: visible && !shell.collectionManager.open && !shell.serviceOpen && !shell.menuOpen && !shell.keyboard.open && !shell.notice.length && !shell.drawerOpen && !shell.quickAppsOpen && !shell.homeMenuOpen
    readonly property string focusKey: shell.gamesRootZone + ":" + shell.gamesCollectionIndex + ":" + shell.gamesPlatformIndex
    readonly property var collectionRows: shell.gamesCollections.concat([{id: "_create", name: "New collection", count: 0, colour: "#aac7b1", art: ""}])
    function restoreScroll() {
        collections.contentX = Math.max(0, Math.min(shell.collectionScroll * collections.cell, collections.contentWidth - collections.width))
        platforms.contentX = Math.max(0, Math.min(shell.platformScroll * platforms.cellWidth, platforms.contentWidth - platforms.width))
        collections.reveal(); platforms.reveal()
    }
    function finishScroll(view, zone, rows) {
        const cell = zone === "collections" ? view.cell : view.cellWidth
        if (zone === "collections") shell.collectionScroll = view.contentX / cell
        else shell.platformScroll = view.contentX / cell
        if (!rows.length) return
        const first = Math.max(0, Math.ceil(view.contentX / cell))
        const last = Math.min(rows.length-1, Math.floor((view.contentX+view.width)/cell)-1)
        const index = Math.max(first, Math.min(last, view.currentIndex))
        shell.focusGamesRootItem(zone, rows[Math.min(index,rows.length-1)].id)
    }
    onVisibleChanged: if (visible) Qt.callLater(restoreScroll)
    Component.onCompleted: Qt.callLater(restoreScroll)
    Text {
        x: 30; y: 1; text: "Collections"; color: Theme.ink
        font.family: Theme.displayFamily; font.pixelSize: 17
    }
    ListView {
        id: collections
        objectName: "series-grid"
        x: 30; y: 24; width: parent.width - 60; height: Math.min(width / 6 - 22, root.height * 0.24) + 44
        readonly property real cell: width / 6
        orientation: ListView.Horizontal; clip: true; boundsBehavior: Flickable.StopAtBounds
        model: root.collectionRows; currentIndex: shell.gamesCollectionIndex
        keyNavigationEnabled: false
        onMovementEnded: root.finishScroll(collections, "collections", root.collectionRows)
        function reveal() {
            if (!root.takesFocus || shell.gamesRootZone !== "collections" || !currentItem) return
            positionViewAtIndex(currentIndex, ListView.Contain)
            shell.collectionScroll = contentX / cell
            currentItem.control.forceActiveFocus(Qt.OtherFocusReason)
        }
        onCurrentIndexChanged: Qt.callLater(reveal)
        onCurrentItemChanged: Qt.callLater(reveal)
        Connections { target: root; function onTakesFocusChanged() { Qt.callLater(collections.reveal) } }
        Connections { target: root; function onFocusKeyChanged() { Qt.callLater(collections.reveal) } }
        delegate: Item {
            required property int index
            required property var modelData
            width: collections.cell; height: collections.height
            property alias control: cap
            Item {
                id: cap
                objectName: "series-card-" + modelData.id
                readonly property real diameter: collections.height - 44
                readonly property bool selected: root.takesFocus && shell.gamesRootZone === "collections" && collections.currentIndex === index
                x: 4; y: 4; width: parent.width - 8; height: parent.height - 8
                Accessible.role: Accessible.Button
                Accessible.name: modelData.name
                Accessible.onPressAction: mouse.clicked(null)
                onSelectedChanged: if(selected)forceActiveFocus(Qt.OtherFocusReason)
                Rectangle {
                    x: (parent.width-width)/2; y: 4; width: cap.diameter; height: width; radius: width/2
                    color: "#4014262b"
                }
                Rectangle {
                    id: rim
                    x: (parent.width-width)/2; y: 0; width: cap.diameter; height: width; radius: width/2
                    border.width: cap.selected ? 2 : 1; border.color: cap.selected ? Theme.focus : Qt.lighter(modelData.colour, 1.6)
                    gradient: Gradient {
                        GradientStop { position: 0; color: Qt.lighter(modelData.colour,1.65) }
                        GradientStop { position: 0.5; color: modelData.colour }
                        GradientStop { position: 1; color: Qt.darker(modelData.colour,1.5) }
                    }
                    Rectangle { anchors.fill: parent; anchors.margins: -3; radius: width/2; color: "transparent"; border.width: 2; border.color: Theme.focus; opacity: 0.4; visible: cap.selected }
                    Image {
                        id: art
                        x: 5; y: 5; width: rim.width-10; height: width
                        source: modelData.art || ""
                        sourceSize.width: 512; sourceSize.height: 512
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true; visible: false
                    }
                    Rectangle {
                        id: artMask
                        width: art.width; height: art.height; radius: width/2
                        color: "white"; visible: false; layer.enabled: true
                    }
                    MultiEffect {
                        x: art.x; y: art.y; width: art.width; height: art.height
                        source: art; maskEnabled: true; maskSource: artMask
                        visible: art.status === Image.Ready
                    }
                    Text {
                        anchors.centerIn: parent; visible: art.status !== Image.Ready
                        text: modelData.id === "_create" ? "+" : modelData.id.startsWith("auto:") || modelData.dynamic ? "\u21bb" : "\u25a4"
                        font.pixelSize: rim.width*0.5; color: Theme.paper
                    }
                    Rectangle {
                        x: parent.width-width+3; y: parent.height-height-2
                        width: Math.max(23,countText.implicitWidth+10); height: 20; radius: 10
                        color: Theme.paper; border.color: Qt.darker(modelData.colour,1.2); border.width: 1
                        visible: modelData.id !== "_create"
                        Text { id: countText; anchors.centerIn: parent; text: modelData.count; color: Theme.ink; font.pixelSize: 12; font.bold: true }
                    }
                }
                Text {
                    x: 2; y: rim.height + 7; width: parent.width - 4; height: 30
                    text: modelData.name; color: Theme.ink; font.family: Theme.displayFamily
                    font.pixelSize: 15; font.bold: cap.selected; horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
                    minimumPixelSize: 12; fontSizeMode: Text.Fit
                }
                MouseArea {
                    id: mouse; anchors.fill: parent
                    onClicked: { shell.collectionScroll = collections.contentX / collections.cell; shell.openGamesRootItem("collections", modelData.id) }
                    onPressAndHold: if(modelData.id.startsWith("user:"))shell.openContext("collection", modelData.id)
                }
            }
            SocialIconButton {
                x: (parent.width + cap.diameter)/2 - 24; y: 2; width: 24; height: 24
                visible: modelData.id.startsWith("user:")
                icon: "dots-three"; label: "Edit " + modelData.name
                onClicked: shell.openContext("collection", modelData.id)
            }
        }
    }
    Rectangle {
        id: divider
        x: 30; y: collections.y + collections.height + 3; width: parent.width - 60; height: 1
        color: Theme.ink; opacity: 0.12
    }
    Text {
        x: 30; y: divider.y + 6; text: "Platforms"; color: Theme.ink
        font.family: Theme.displayFamily; font.pixelSize: 17
    }
    GridView {
        id: platforms
        objectName: "games-platforms"
        x: 30; y: divider.y + 29; width: parent.width - 60; height: parent.height - y - 18
        readonly property real coverAspect: 3 / 4
        readonly property real coverGap: 6
        readonly property int columns: Math.max(1, Math.round(width / ((height - 12) * coverAspect + coverGap)))
        cellWidth: width / columns; cellHeight: height; flow: GridView.FlowTopToBottom
        clip: true; boundsBehavior: Flickable.StopAtBounds; keyNavigationEnabled: false
        model: shell.gamesPlatforms; currentIndex: shell.gamesPlatformIndex
        onMovementEnded: root.finishScroll(platforms, "platforms", shell.gamesPlatforms)
        function reveal() {
            if (!root.takesFocus || shell.gamesRootZone !== "platforms" || !currentItem) return
            positionViewAtIndex(currentIndex, GridView.Contain)
            shell.platformScroll = contentX / cellWidth
            currentItem.control.forceActiveFocus(Qt.OtherFocusReason)
        }
        onCurrentIndexChanged: Qt.callLater(reveal)
        onCurrentItemChanged: Qt.callLater(reveal)
        Connections { target: root; function onTakesFocusChanged() { Qt.callLater(platforms.reveal) } }
        Connections { target: root; function onFocusKeyChanged() { Qt.callLater(platforms.reveal) } }
        delegate: Item {
            required property int index
            required property var modelData
            width: platforms.cellWidth; height: platforms.cellHeight
            property alias control: card
            PlatformCard {
                id: card; objectName: "games-platform-" + modelData.id
                width: Math.min(parent.width - platforms.coverGap, (parent.height - 12) * platforms.coverAspect)
                height: width / platforms.coverAspect
                x: (parent.width-width)/2; y: parent.height-height-6
                compact: true; portrait: true; entry: modelData
                selected: root.takesFocus && shell.gamesRootZone === "platforms" && platforms.currentIndex === index
                onActivated: { shell.platformScroll = platforms.contentX / platforms.cellWidth; shell.openGamesRootItem("platforms", modelData.id) }
            }
        }
    }
    component ScrollArrow: Item {
        objectName: "games-scroll-" + zone + (back ? "-left" : "-right")
        Accessible.role: Accessible.Button
        Accessible.name: (back ? "Previous " : "More ") + zone
        required property var view
        required property string zone
        property bool back: false
        readonly property real cell: zone === "collections" ? view.cell : view.cellWidth
        width: 28; height: 44
        x: back ? 0 : root.width-width
        y: view.y + (view.height-height)/2
        visible: view.count > Math.round(view.width/cell) && (back ? view.contentX > 1 : view.contentX + view.width < view.contentWidth - 1)
        Text { anchors.centerIn: parent; text: parent.back ? "\u2039" : "\u203a"; font.pixelSize: 32; color: Theme.ink; opacity: 0.65 }
        MouseArea {
            anchors.fill: parent
            onClicked: {
                const rows=parent.zone === "collections" ? root.collectionRows : shell.gamesPlatforms
                const index=Math.max(0,Math.min(rows.length-1, parent.back ? Math.ceil(parent.view.contentX/parent.cell)-1 : Math.floor((parent.view.contentX+parent.view.width)/parent.cell)))
                shell.focusGamesRootItem(parent.zone,rows[index].id)
            }
        }
    }
    ScrollArrow { view: collections; zone: "collections"; back: true }
    ScrollArrow { view: collections; zone: "collections" }
    ScrollArrow { view: platforms; zone: "platforms"; back: true }
    ScrollArrow { view: platforms; zone: "platforms" }
    Text {
        anchors.centerIn: platforms; visible: platforms.count === 0
        text: "Your consoles will appear here\nAdd games in Start \u2192 Library"
        color: Theme.muted; font.pixelSize: 16; horizontalAlignment: Text.AlignHCenter
    }
}
