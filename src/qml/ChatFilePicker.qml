import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic

Item {
    id: root
    required property var picker
    property string title: "Choose a file"
    property var entries: []
    property string displayedPath: ""
    visible: picker.open
    function syncRows() {
        const y = contents.contentY
        const preserve = displayedPath === picker.path && entries.length > 0
        entries = picker.rows
        displayedPath = picker.path
        Qt.callLater(function() { contents.contentY = preserve ? Math.max(0,Math.min(y,Math.max(0,contents.contentHeight-contents.height))) : 0 })
    }
    onVisibleChanged: if(visible)syncRows()
    Component.onCompleted: syncRows()
    Connections {
        target: root.picker
        function onRowsChanged() { root.syncRows() }
        function onScrollRequested(direction) { contents.contentY=Math.max(0,Math.min(Math.max(0,contents.contentHeight-contents.height),contents.contentY+direction*60)); contents.maybeLoad() }
    }
    Rectangle { anchors.fill: parent; color: "#98102022" }
    MouseArea { anchors.fill: parent }
    component Tool: Item {
        required property string symbol
        required property string label
        property bool selected: false
        signal clicked()
        width: 31; height: 31; opacity: enabled ? 1 : .35
        Accessible.role: Accessible.Button; Accessible.name: label
        Accessible.onPressAction: if(enabled)clicked()
        Rectangle { anchors.fill: parent; radius: 7; color: parent.selected ? Theme.yellow : hover.containsMouse ? Qt.rgba(Theme.rim.r,Theme.rim.g,Theme.rim.b,.12) : "transparent" }
        Text { anchors.centerIn: parent; text: parent.symbol; font.pixelSize: 21; color: Theme.ink }
        MouseArea { id: hover; anchors.fill: parent; hoverEnabled: true; onClicked: parent.clicked() }
        ToolTip.visible: hover.containsMouse; ToolTip.delay: 700; ToolTip.text: label
    }
    Panel {
        id: panel
        anchors.centerIn: parent; width: Math.max(0,Math.min(790,parent.width-48)); height: Math.max(0,Math.min(440,parent.height-34)); radius: 24
        FilesIcon { x: 22; y: 19; width: 30; height: 30; kind: "folder" }
        Text { x: 64; y: 20; text: root.title; font.family: Theme.displayFamily; font.pixelSize: 23; color: Theme.ink }
        SocialIconButton { anchors.right: parent.right; anchors.rightMargin: 20; y: 17; icon: "x"; label: "Cancel"; onClicked: root.picker.cancel() }
        Rectangle { x: 17; y: 59; width: parent.width-34; height: 37; radius: 8; color: Qt.rgba(Theme.rim.r,Theme.rim.g,Theme.rim.b,.08) }
        Row { x: 23; y: 62; spacing: 3
            Tool { symbol: "‹"; label: "Back"; enabled: root.picker.canBack; selected: root.picker.zone==="toolbar"&&root.picker.focusIndex===0; onClicked: root.picker.back() }
            Tool { symbol: "↑"; label: "Parent folder"; selected: root.picker.zone==="toolbar"&&root.picker.focusIndex===1; onClicked: root.picker.parentFolder() }
        }
        Flickable {
            id: breadcrumbScroll; x: 97; y: 62; width: parent.width-x-97; height: 31; clip: true
            contentWidth: breadcrumbRow.width; boundsBehavior: Flickable.StopAtBounds
            onContentWidthChanged: contentX=Math.max(0,contentWidth-width)
            Row { id: breadcrumbRow; height: 31; spacing: 4
                Repeater { model: root.picker.breadcrumbs
                    Row { required property var modelData; required property int index; height: 31; spacing: 4
                        Text { visible: index>0; text: "›"; color: Theme.muted; font.pixelSize: 17; anchors.verticalCenter: parent.verticalCenter }
                        Rectangle { width: crumbLabel.implicitWidth+14; height: 31; radius: 6
                            color: root.picker.zone==="crumbs"&&root.picker.focusIndex===index ? Theme.yellow : "transparent"
                            Text { id: crumbLabel; anchors.centerIn: parent; text: modelData.title; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 12 }
                            TapHandler { onTapped: root.picker.navigate(modelData.path) }
                        }
                    }
                }
            }
        }
        Row { anchors.right: parent.right; anchors.rightMargin: 23; y: 62; spacing: 3
            Tool { symbol: "↻"; label: "Refresh"; selected: root.picker.zone==="toolbar"&&root.picker.focusIndex===2; onClicked: root.picker.refresh() }
            Tool { symbol: root.picker.grid ? "☷" : "▦"; label: root.picker.grid ? "List · R3" : "Icons · R3"; selected: root.picker.zone==="toolbar"&&root.picker.focusIndex===3; onClicked: root.picker.toggleView() }
        }
        ListView {
            id: places; x: 20; y: 109; width: 156; height: panel.height-y-73; clip: true; spacing: 3
            model: root.picker.places; currentIndex: root.picker.placeIndex
            boundsBehavior: Flickable.StopAtBounds
            onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
            ScrollBar.vertical: Basic.ScrollBar { width: 4; visible: places.contentHeight > places.height; background: null; contentItem: Rectangle { implicitWidth: 4; implicitHeight: 24; radius: 2; color: Theme.rim; opacity: .5 } }
            delegate: Item {
                required property var modelData; required property int index
                width: places.width-5; height: 36
                Rectangle { anchors.fill: parent; radius: 7
                    color: root.picker.path===modelData.path ? Qt.rgba(Theme.blue.r,Theme.blue.g,Theme.blue.b,.28) : "transparent"
                    border.width: root.picker.zone==="places"&&root.picker.placeIndex===index ? 1.5 : 0; border.color: Theme.focusGlow }
                FilesIcon { x: 8; y: 6; width: 24; height: 24; kind: modelData.icon }
                Text { x: 41; anchors.verticalCenter: parent.verticalCenter; width: parent.width-x-5; text: modelData.title; textFormat: Text.PlainText; font.pixelSize: 12; color: Theme.ink; elide: Text.ElideRight }
                TapHandler { onTapped: root.picker.activate(index,"places") }
            }
        }
        Rectangle { x: 183; y: 111; width: 1; height: places.height-4; color: Theme.rim; opacity: .22 }
        Item {
            id: body; x: 195; y: 107; width: panel.width-x-23; height: panel.height-y-73
            Row { visible: !root.picker.grid; width: parent.width; height: 25
                Text { width: body.width*.61; text: "Name"; color: Theme.muted; font.pixelSize: 11; leftPadding: 8 }
                Text { width: body.width*.16; text: "Size"; color: Theme.muted; font.pixelSize: 11 }
                Text { text: "Modified"; color: Theme.muted; font.pixelSize: 11 }
            }
            GridView {
                id: contents; y: root.picker.grid ? 0 : 25; width: parent.width; height: parent.height-y; clip: true
                model: root.entries; currentIndex: root.picker.rowIndex
                readonly property int columns: root.picker.grid ? Math.max(3,Math.floor(width/110)) : 1
                cellWidth: width/columns; cellHeight: root.picker.grid ? 86 : 35
                boundsBehavior: Flickable.StopAtBounds; keyNavigationEnabled: false
                onColumnsChanged: root.picker.setColumns(columns)
                Component.onCompleted: root.picker.setColumns(columns)
                onCurrentIndexChanged: if(currentIndex>=0)Qt.callLater(function(){ contents.positionViewAtIndex(contents.currentIndex,GridView.Contain) })
                function maybeLoad() { if(root.visible && root.picker.hasMore && !root.picker.busy && contentY+height>=contentHeight-90)root.picker.loadMore() }
                onContentYChanged: if(moving)maybeLoad()
                onMovementEnded: maybeLoad()
                ScrollBar.vertical: Basic.ScrollBar { width: 5; visible: contents.contentHeight > contents.height; background: null; contentItem: Rectangle { implicitWidth: 5; implicitHeight: 24; radius: 2.5; color: Theme.rim; opacity: .6 } }
                delegate: Item {
                    required property var modelData; required property int index
                    width: contents.cellWidth; height: contents.cellHeight
                    opacity: root.picker.choosingFolder&&!modelData.directory ? .43 : 1
                    Accessible.role: Accessible.Button; Accessible.name: modelData.title
                    Accessible.onPressAction: root.picker.activate(index,"list")
                    Rectangle { anchors.fill: parent; anchors.margins: 2; radius: 6
                        color: root.picker.rowIndex===index&&root.picker.zone==="list" ? Qt.rgba(Theme.blue.r,Theme.blue.g,Theme.blue.b,.22) : "transparent"
                        border.width: root.picker.rowIndex===index&&root.picker.zone==="list" ? 1 : 0; border.color: Theme.focusGlow }
                    FilesIcon { x: root.picker.grid ? (parent.width-width)/2 : 8; y: root.picker.grid ? 5 : 5; width: root.picker.grid ? 43 : 25; height: width; kind: modelData.icon }
                    Text { x: root.picker.grid ? 5 : 42; y: root.picker.grid ? 52 : 0; width: root.picker.grid ? parent.width-10 : parent.width*.60-42; height: root.picker.grid ? 31 : parent.height
                        text: modelData.title; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 12; verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: root.picker.grid ? Text.AlignHCenter : Text.AlignLeft; wrapMode: root.picker.grid ? Text.Wrap : Text.NoWrap; maximumLineCount: 2; elide: Text.ElideRight }
                    Text { visible: !root.picker.grid; x: parent.width*.61; width: parent.width*.15; height: parent.height; text: modelData.size; verticalAlignment: Text.AlignVCenter; color: Theme.muted; font.pixelSize: 10; elide: Text.ElideRight }
                    Text { visible: !root.picker.grid; x: parent.width*.77; width: parent.width*.22; height: parent.height; text: modelData.modified; verticalAlignment: Text.AlignVCenter; color: Theme.muted; font.pixelSize: 10; elide: Text.ElideRight }
                    Rectangle { visible: !root.picker.grid; x: 7; width: parent.width-14; height: 1; y: parent.height-1; color: Theme.rim; opacity: .12 }
                    TapHandler { onTapped: { if(modelData.directory)root.picker.activate(index,"list"); else root.picker.select(index) } }
                }
            }
            Column { anchors.centerIn: parent; width: parent.width-30; spacing: 9; visible: !contents.count
                FilesIcon { anchors.horizontalCenter: parent.horizontalCenter; width: 48; height: 48; kind: "folder"; opacity: .65 }
                Text { width: parent.width; text: root.picker.busy ? "Opening folder…" : root.picker.error || "This folder is empty"; textFormat: Text.PlainText; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 13; color: Theme.muted }
            }
        }
        Rectangle { x: 23; y: panel.height-66; width: panel.width-46; height: 1; color: Theme.rim; opacity: .23 }
        Text { x: 25; y: panel.height-53; width: panel.width-320; height: 34; verticalAlignment: Text.AlignVCenter; textFormat: Text.PlainText
            text: root.picker.error || (root.picker.busy ? "Loading…" : root.picker.selectionName || "Select a file")
            font.pixelSize: 12; color: Theme.muted; elide: Text.ElideMiddle }
        CapButton { x: panel.width-284; y: panel.height-53; width: 104; height: 35; label: "Cancel"; textSize: 13; tint: Theme.paper
            selected: root.picker.zone==="actions"&&root.picker.focusIndex===0; onActivated: root.picker.cancel() }
        CapButton { x: panel.width-168; y: panel.height-53; width: 143; height: 35; label: root.picker.choosingFolder ? "Choose folder" : "Choose file"; textSize: 13; tint: Theme.green
            enabled: root.picker.canChoose; opacity: enabled ? 1 : .45
            selected: root.picker.zone==="actions"&&root.picker.focusIndex===1; onActivated: root.picker.choose() }
    }
}
