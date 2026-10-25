import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic

Item {
    id: root
    required property var flow
    property bool takesFocus: false
    property string applicationStatus: ""
    readonly property var state: flow.view
    readonly property int columns: Math.max(3, Math.floor(contents.width / 120))
    function point(px,py) {
        if(px<sidebar.width && py>=sidebar.y) {
            const i=places.indexAt(px,py-sidebar.y+places.contentY);if(i>=0)flow.focus("places",i)
        } else if(px>=contents.x && py>=contents.y) {
            const i=files.indexAt(px-contents.x,py-contents.y+files.contentY);if(i>=0)flow.select(i)
        } else if(py<crumbs.height) {
            const p=breadcrumbRow.mapFromItem(root,px,py)
            for(let i=0;i<breadcrumbRepeater.count;i++) {const item=breadcrumbRepeater.itemAt(i);if(p.x>=item.x && p.x<item.x+item.width){flow.focus("crumbs",i);break}}
        }
    }
    onColumnsChanged: flow.setColumns(columns)
    Component.onCompleted: flow.setColumns(columns)
    Rectangle { anchors.fill: parent; color: Theme.paper }
    Rectangle {
        id: crumbs; x: 0; y: 0; width: parent.width; height: 28
        color: Qt.rgba(Theme.rim.r, Theme.rim.g, Theme.rim.b, .09)
        Flickable {
            id: breadcrumbScroll; anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12
            contentWidth: breadcrumbRow.width; clip: true; boundsBehavior: Flickable.StopAtBounds
            onContentWidthChanged: contentX = Math.max(0, contentWidth-width)
            Row { id: breadcrumbRow; height: 28; spacing: 6
                Repeater { id: breadcrumbRepeater; model: root.flow.breadcrumbs
                    Row { required property var modelData; required property int index; height: 28; spacing: 6
                        Text { text: index ? "›" : ""; color: Theme.muted; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
                        Text { text: modelData.label; font.pixelSize: 12; color: root.takesFocus && root.state.area === "crumbs" && root.state.controlIndex === index ? Theme.chassis : Theme.ink; font.bold: root.takesFocus && root.state.area === "crumbs" && root.state.controlIndex === index; anchors.verticalCenter: parent.verticalCenter
                            MouseArea { anchors.fill: parent; anchors.topMargin: -6; anchors.bottomMargin: -6; onClicked: root.flow.navigate(modelData.url) }
                        }
                    }
                }
            }
        }
    }
    Item {
        id: sidebar; x: 0; y: crumbs.height+10; width: Math.min(170,root.width*.22); height: root.height-y-60
        ListView {
            id: places; anchors.fill: parent; clip: true; model: root.flow.places; spacing: 3
            currentIndex: root.state.placeIndex
            onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
            ScrollBar.vertical: Basic.ScrollBar { policy: ScrollBar.AsNeeded; width: 5; background: null; contentItem: Rectangle { implicitWidth: 5; implicitHeight: 24; radius: 2.5; color: Theme.rim; opacity: .65 } }
            delegate: Item { required property var modelData; required property int index; width: places.width-10; height: 37
                Rectangle { x: 4; y: 1; width: parent.width-8; height: 35; radius: 7
                    color: root.state.url === modelData.url ? Qt.rgba(Theme.rim.r,Theme.rim.g,Theme.rim.b,.15) : "transparent"
                    border.width: root.takesFocus && root.state.area === "places" && root.state.placeIndex === index ? 1 : 0; border.color: Theme.chassis
                }
                FilesIcon { x: 12; y: 7; width: 24; height: 24; kind: modelData.icon }
                Text { x: 44; anchors.verticalCenter: parent.verticalCenter; width: parent.width-x-7; text: modelData.name; textFormat: Text.PlainText; font.pixelSize: 13; elide: Text.ElideRight; color: Theme.ink }
                MouseArea { anchors.fill: parent; onClicked: { root.flow.focus("places",index); root.flow.navigate(modelData.url) } }
            }
        }
    }
    Rectangle { x: sidebar.width; y: crumbs.height+13; width: 1; height: root.height-y-59; color: Theme.rim; opacity: .24 }
    Item {
        id: contents; x: sidebar.width+13; y: crumbs.height+9; width: parent.width-x-9; height: parent.height-y-60
        GridView {
            id: files; anchors.fill: parent; clip: true; model: root.flow.rows
            cellWidth: root.state.grid ? width/root.columns : width
            cellHeight: root.state.grid ? 101 : 42
            currentIndex: root.state.index
            onCurrentIndexChanged: if(currentIndex>=0)positionViewAtIndex(currentIndex,GridView.Contain)
            property real previousY: 0
            onContentYChanged: { if(moving)root.flow.hideControls(contentY>previousY && contentY>5); previousY=contentY }
            ScrollBar.vertical: Basic.ScrollBar { policy: ScrollBar.AsNeeded; width: 5; background: null; contentItem: Rectangle { implicitWidth: 5; implicitHeight: 24; radius: 2.5; color: Theme.rim; opacity: .65 } }
            delegate: Item {
                required property var modelData; required property int index
                width: files.cellWidth; height: files.cellHeight
                Accessible.role: Accessible.Button; Accessible.name: modelData.name
                Accessible.onPressAction: root.flow.activate(index)
                Rectangle { anchors.fill: parent; anchors.margins: 3; radius: 8; color: modelData.selected ? Theme.surfaceColor("#22f4cc62") : "transparent"
                    border.width: root.takesFocus && root.state.area === "files" && root.state.index === index ? 1.5 : modelData.selected ? 1 : 0
                    border.color: modelData.selected ? Theme.chassis : Theme.focusGlow
                }
                FilesIcon { id: icon; x: root.state.grid ? (parent.width-width)/2 : 11; y: root.state.grid ? 7 : 6
                    width: root.state.grid ? 55 : 29; height: width; kind: modelData.directory ? "folder" : modelData.icon; preview: modelData.preview || "" }
                Text { x: root.state.grid ? 8 : 51; y: root.state.grid ? 67 : 5; width: root.state.grid ? parent.width-16 : parent.width*.57-51; height: root.state.grid ? 31 : 32
                    text: modelData.name; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 12
                    horizontalAlignment: root.state.grid ? Text.AlignHCenter : Text.AlignLeft; verticalAlignment: Text.AlignVCenter
                    wrapMode: root.state.grid ? Text.Wrap : Text.NoWrap; maximumLineCount: root.state.grid ? 2 : 1; elide: Text.ElideRight
                }
                Text { visible: !root.state.grid; x: parent.width*.59; width: parent.width*.15; anchors.verticalCenter: parent.verticalCenter; text: modelData.sizeLabel; color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight }
                Text { visible: !root.state.grid; x: parent.width*.77; width: parent.width*.22; anchors.verticalCenter: parent.verticalCenter; text: modelData.modified; color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight }
                Rectangle { visible: !root.state.grid; x: 9; y: parent.height-1; width: parent.width-18; height: 1; color: Theme.rim; opacity: .19 }
                Text { visible: modelData.selected; x: parent.width-21; y: 5; text: "✓"; color: Theme.chassis; font.pixelSize: 15 }
                TapHandler { onTapped: root.flow.activate(index); onLongPressed: root.flow.context(index) }
            }
        }
        Column { anchors.centerIn: parent; width: parent.width-40; spacing: 8; visible: !files.count
            FilesIcon { anchors.horizontalCenter: parent.horizontalCenter; width: 56; height: 56; kind: "folder"; opacity: .65 }
            Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: root.state.busy ? "Opening folder…" : root.state.error || (root.state.query.length ? "No matching files" : "This folder is empty"); textFormat: Text.PlainText; wrapMode: Text.Wrap; font.pixelSize: 14; color: Theme.muted }
        }
        Connections { target: root.flow; function onScrollRequested(direction) { files.contentY=Math.max(0,Math.min(Math.max(0,files.contentHeight-files.height),files.contentY+direction*55)) } }
    }
    Text { x: sidebar.width+22; y: root.height-50; width: parent.width-x-12; height: 18; color: root.state.error.length ? Theme.textColor("#96502d") : Theme.muted; font.pixelSize: 11; elide: Text.ElideRight; textFormat: Text.PlainText
        text: root.state.error || root.applicationStatus || (root.state.selected ? root.state.selected+" selected" : root.state.count+" items") + (root.state.query.length ? " · "+root.state.query : "") }
}
