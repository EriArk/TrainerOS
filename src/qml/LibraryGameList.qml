import QtQuick

Item {
    id: root
    objectName: "library-paged-list"
    required property var entries
    required property int selectionIndex
    required property bool takesFocus
    signal activated(int index)
    signal focusRequested(int index)
    required property int pageSize
    readonly property real rowHeight: Math.min(49, Math.max(0, (height - 34) / pageSize))
    readonly property int page: Math.floor(Math.max(0, selectionIndex) / pageSize)
    readonly property int pageCount: Math.ceil(entries.length / pageSize)
    readonly property int first: page * pageSize
    property int previousPage: 0
    function turnPage(direction) {
        if (!takesFocus || page + direction < 0 || page + direction >= pageCount) return
        focusRequested(Math.min(entries.length - 1, (page + direction) * pageSize + selectionIndex % pageSize))
    }
    function reveal() {
        if (takesFocus && visible) {
            const row = rows.itemAt(selectionIndex - first)
            if (row) row.forceActiveFocus(Qt.OtherFocusReason)
        }
    }
    onSelectionIndexChanged: Qt.callLater(reveal)
    onTakesFocusChanged: Qt.callLater(reveal)
    onPageChanged: {
        slide.stop()
        sheet.y = page > previousPage ? 12 : -12
        previousPage = page
        slide.start()
        Qt.callLater(reveal)
    }
    Item {
        id: viewport
        width: parent.width; height: root.rowHeight * root.pageSize; clip: true
        Column {
            id: sheet; width: parent.width; spacing: 0
            NumberAnimation { id: slide; target: sheet; property: "y"; to: 0; duration: Theme.motion(130); easing.type: Easing.OutCubic }
            Repeater {
                id: rows
                model: Math.max(0, Math.min(root.pageSize, root.entries.length - root.first))
                delegate: Item {
                    id: row
                    required property int index
                    readonly property int gameIndex: root.first + index
                    readonly property var entry: root.entries[gameIndex] || ({})
                    readonly property bool chosen: gameIndex === root.selectionIndex
                    objectName: "library-list-row-" + index
                    width: sheet.width; height: root.rowHeight
                    Accessible.role: Accessible.ListItem
                    Accessible.name: entry.title || ""
                    Accessible.selected: chosen
                    Component.onCompleted: Qt.callLater(root.reveal)
                    Repeater {
                        model: [ { line: 6, alpha: 0.07 }, { line: 3, alpha: 0.18 }, { line: 1.2, alpha: 0.9 } ]
                        Rectangle {
                            required property var modelData
                            x: 3; y: 3; width: row.width - 6; height: row.height - 7
                            radius: 8; color: "transparent"; visible: row.chosen
                            border.color: Theme.focusGlow; border.width: modelData.line; opacity: modelData.alpha
                        }
                    }
                    Text {
                        x: 12; y: 0; width: parent.width - 76; height: parent.height - 3
                        verticalAlignment: Text.AlignVCenter
                        text: row.entry.title || ""; textFormat: Text.PlainText
                        font.family: Theme.displayFamily; font.pixelSize: 16
                        font.bold: row.chosen; color: Theme.ink; elide: Text.ElideRight
                    }
                    Text {
                        anchors.right: parent.right; anchors.rightMargin: 12
                        y: 0; width: 46; height: parent.height - 3
                        verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignRight
                        text: (row.entry.linked === false ? "! " : "") + (row.entry.platformShort || "")
                        textFormat: Text.PlainText; font.pixelSize: 10; color: Theme.muted; elide: Text.ElideRight
                    }
                    Rectangle {
                        x: 12; width: parent.width - 24; height: 1; anchors.bottom: parent.bottom
                        color: Theme.ink; opacity: 0.12
                        visible: !row.chosen && row.gameIndex + 1 !== root.selectionIndex && index + 1 < rows.count
                    }
                }
            }
        }
        // One gesture surface: a swipe changes a whole page, never launches a row.
        MouseArea {
            anchors.fill: parent; enabled: root.takesFocus
            property real downY: 0
            property real downX: 0
            property real wheelTravel: 0
            onPressed: mouse => { downY = mouse.y; downX = mouse.x }
            onReleased: mouse => {
                const dy = mouse.y - downY
                if (Math.abs(dy) > 26) root.turnPage(dy < 0 ? 1 : -1)
                else if (Math.abs(mouse.x - downX) < 15 && Math.abs(dy) < 15) {
                    const i = root.first + Math.floor(mouse.y / root.rowHeight)
                    if (i >= root.first && i < Math.min(root.entries.length, root.first + root.pageSize)) root.activated(i)
                }
            }
            onWheel: wheel => {
                wheelTravel += wheel.angleDelta.y
                if (Math.abs(wheelTravel) >= 120) { root.turnPage(wheelTravel < 0 ? 1 : -1); wheelTravel = 0 }
                wheel.accepted = true
            }
        }
    }
    Row {
        anchors.horizontalCenter: parent.horizontalCenter; y: viewport.height + 5
        height: 29; spacing: 17; visible: root.pageCount > 0
        Text {
            objectName: "library-list-previous"
            width: 30; height: parent.height; text: "‹"; font.pixelSize: 25
            horizontalAlignment: Text.AlignHCenter; color: Theme.ink; opacity: root.page > 0 ? 1 : 0.25
            Accessible.role: Accessible.Button; Accessible.name: "Previous page"
            MouseArea { anchors.fill: parent; enabled: root.takesFocus && root.page > 0; onClicked: root.turnPage(-1) }
        }
        Text {
            height: parent.height; text: (root.page + 1) + " / " + root.pageCount
            verticalAlignment: Text.AlignVCenter; color: Theme.muted; font.pixelSize: 13
        }
        Text {
            objectName: "library-list-next"
            width: 30; height: parent.height; text: "›"; font.pixelSize: 25
            horizontalAlignment: Text.AlignHCenter; color: Theme.ink; opacity: root.page + 1 < root.pageCount ? 1 : 0.25
            Accessible.role: Accessible.Button; Accessible.name: "Next page"
            MouseArea { anchors.fill: parent; enabled: root.takesFocus && root.page + 1 < root.pageCount; onClicked: root.turnPage(1) }
        }
    }
}
