import QtQuick

Item {
    id: root
    required property var shell
    property bool active: false
    readonly property var album: shell.album
    readonly property var current: album.current
    readonly property var previous: album.items[album.index-1] || ({})
    readonly property var next: album.items[album.index+1] || ({})
    readonly property bool canBrowse: active && !album.filtering
    objectName: "screens-album"
    Item {
        width: parent.width; height: 27
        Text {
            text: "\u2039  My Story"; font.pixelSize: 14; color: Theme.muted
            MouseArea { anchors.fill: parent; anchors.margins: -6; enabled: root.canBrowse; onClicked: root.shell.showOverview("summary") }
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Screens"+(root.album.count ? "  \u00b7  "+(root.album.index+1)+" / "+root.album.count : "")
            font.family: Theme.displayFamily; font.pixelSize: 17; color: Theme.ink
        }
        Text {
            anchors.right: parent.right; width: Math.min(250,parent.width*.3); horizontalAlignment: Text.AlignRight
            text: root.album.filterLabel+"  \u2304"; elide: Text.ElideRight; textFormat: Text.PlainText
            font.pixelSize: 14; color: Theme.muted
            MouseArea { anchors.fill: parent; anchors.margins: -6; enabled: root.active; onClicked: root.album.showFilters() }
        }
    }
    Item {
        id: stage; objectName: "album-triptych"
        x: 28; y: 30; width: parent.width-56; height: Math.max(80,film.y-y-39)
        visible: root.album.count>0
        readonly property real mainWidth: Math.min(width*.61,height*1.9)
        AlbumPhoto {
            width: hero.frameWidth*.82; height: stage.height*.77
            x: stage.width/2-hero.frameWidth*.90; y: stage.height*.025
            rotation: -9; source: root.previous.source || ""; ratio: root.previous.ratio || 16/9; visible: !!root.previous.source
            available: root.canBrowse; onActivated: root.album.select(root.album.index-1)
        }
        AlbumPhoto {
            width: hero.frameWidth*.81; height: stage.height*.75
            x: stage.width/2+hero.frameWidth*.12; y: stage.height*.17
            rotation: 8; source: root.next.source || ""; ratio: root.next.ratio || 16/9; visible: !!root.next.source
            available: root.canBrowse; onActivated: root.album.select(root.album.index+1)
        }
        AlbumPhoto {
            id: hero
            anchors.horizontalCenter: parent.horizontalCenter; y: 0
            width: stage.mainWidth; height: stage.height; prominent: true
            source: root.current.source || ""; ratio: root.current.ratio || 16/9; available: false
        }
    }
    Repeater {
        model: [-1,1]
        Text {
            required property int modelData
            x: modelData<0 ? 0 : root.width-width; y: stage.y+stage.height/2-height/2
            text: modelData<0 ? "\u2039" : "\u203a"; font.pixelSize: 34; color: Theme.ink
            opacity: modelData<0 ? (root.album.index>0? .65 : .15) : (root.album.index+1<root.album.count ? .65 : .15)
            MouseArea {
                anchors.fill: parent; anchors.margins: -6; enabled: root.canBrowse
                onClicked: root.album.select(root.album.index+modelData)
            }
        }
    }
    Item {
        id: caption; y: film.y-37; width: parent.width; height: 34; visible: root.album.count>0
        Text {
            id: gameTitle; anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.width-100,implicitWidth); text: root.current.game || ""; textFormat: Text.PlainText
            font.family: Theme.displayFamily; font.pixelSize: 17; color: Theme.ink; elide: Text.ElideRight
        }
        Text {
            anchors.left: gameTitle.right; anchors.leftMargin: 9; y: -3
            text: root.current.favorite ? "\u2605" : "\u2606"; font.pixelSize: 23
            color: root.current.favorite ? "#b48228" : Theme.muted
            MouseArea { anchors.fill: parent; anchors.margins: -4; enabled: root.canBrowse; onClicked: root.album.toggleFavorite() }
        }
        Text {
            y: 21; width: parent.width; horizontalAlignment: Text.AlignHCenter
            text: (root.current.day || "")+"  \u00b7  "+(root.current.clock || "")+"  \u00b7  Last exit"
            font.pixelSize: 10; color: Theme.muted
        }
    }
    Rectangle {
        id: film; objectName: "album-film"
        x: 9; width: parent.width-18; height: 72; y: parent.height-height
        radius: 9; color: Theme.chassisDark; border.color: Theme.rim; border.width: 1; clip: true
        Rectangle { x: 1; y: 2; width: parent.width-2; height: 1; radius: 1; color: Qt.alpha(Theme.edgeLight,.65) }
        ListView {
            id: strip; objectName: "album-film-strip"
            anchors.fill: parent; anchors.leftMargin: 7; anchors.rightMargin: 7
            orientation: ListView.Horizontal; clip: true
            model: root.album.items; currentIndex: root.album.index
            readonly property real frameStep: 104
            header: Item { width: Math.max(0,(strip.width-strip.frameStep)/2); height: strip.height }
            footer: Item { width: Math.max(0,(strip.width-strip.frameStep)/2); height: strip.height }
            preferredHighlightBegin: (width-frameStep)/2
            preferredHighlightEnd: (width+frameStep)/2
            highlightRangeMode: ListView.StrictlyEnforceRange
            snapMode: ListView.SnapOneItem; boundsBehavior: Flickable.StopAtBounds
            highlightMoveDuration: Theme.motion(150); cacheBuffer: 208
            interactive: root.canBrowse && root.album.count>1
            onCurrentIndexChanged: if(moving && root.canBrowse && currentIndex>=0)root.album.select(currentIndex)
            onMovementEnded: if(root.canBrowse && currentIndex>=0)root.album.select(currentIndex)
            delegate: Item {
                id: frame
                required property var modelData
                required property int index
                width: strip.frameStep; height: strip.height
                readonly property bool selected: index===root.album.index
                Image {
                    anchors.centerIn: parent; width: parent.width-10; height: 48
                    source: root.visible ? frame.modelData.source : ""; sourceSize: Qt.size(200,120)
                    asynchronous: true; fillMode: Image.PreserveAspectFit
                    opacity: frame.selected ? 1 : .66
                }
                Rectangle {
                    x: 3; y: 10; width: parent.width-6; height: parent.height-20; radius: 3
                    color: "transparent"; border.width: 2
                    border.color: frame.selected ? Theme.yellow : Qt.alpha(Theme.rim,.3)
                }
                Text { x: 8; y: 12; text: "\u2605"; visible: !!frame.modelData.favorite; color: Theme.yellow; font.pixelSize: 12 }
                MouseArea { anchors.fill: parent; enabled: root.canBrowse; onClicked: root.album.select(frame.index) }
            }
        }
        // The leader and tail stay film, without invented pictures.
        Row {
            x: -((strip.contentX%14+14)%14); y: 4; spacing: 7
            Repeater { model: Math.ceil(film.width/14)+1; Rectangle { width: 7; height: 4; radius: 1; color: Theme.paper; opacity: .55 } }
        }
        Row {
            x: -((strip.contentX%14+14)%14); y: parent.height-8; spacing: 7
            Repeater { model: Math.ceil(film.width/14)+1; Rectangle { width: 7; height: 4; radius: 1; color: Theme.paper; opacity: .55 } }
        }
    }
    Item {
        visible: !root.album.count; x: 0; y: stage.y; width: parent.width; height: stage.height+30
        HomeScreensCollage { width: Math.min(300,parent.width*.5); height: Math.max(70,parent.height-42); anchors.horizontalCenter: parent.horizontalCenter; balanced: true; frameCount: 3; enabled: false }
        Text {
            anchors.bottom: parent.bottom; width: parent.width; horizontalAlignment: Text.AlignHCenter
            text: root.album.filterLabel==="Starred moments" ? "Star a moment to keep it here." : "Your next adventure leaves a memory."
            color: Theme.muted; font.pixelSize: 14; wrapMode: Text.WordWrap
        }
    }
    Text { anchors.bottom: parent.bottom; anchors.bottomMargin: 77; width: parent.width; text: root.album.error; color: Theme.ink; font.pixelSize: 12; wrapMode: Text.WordWrap; visible: text.length>0 }
    Rectangle {
        anchors.fill: parent; visible: root.album.filtering; color: "#aafffef9"; z: 4
        MouseArea { anchors.fill: parent; enabled: root.active; onClicked: root.album.close() }
        Rectangle {
            anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 8
            width: Math.min(330,parent.width-16); height: Math.min(parent.height-16,filters.contentHeight+24)
            radius: 12; color: Theme.paper; border.color: Theme.rim; border.width: 2
            ListView {
                id: filters; anchors.fill: parent; anchors.margins: 12; clip: true
                model: root.album.filters; currentIndex: root.album.filterFocus
                onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
                delegate: Item {
                    required property var modelData
                    required property int index
                    width: filters.width; height: 37
                    Rectangle { anchors.fill: parent; anchors.margins: 1; radius: 7; color: Qt.alpha(Theme.blue,.25); visible: index===root.album.filterFocus }
                    Text { x: 10; anchors.verticalCenter: parent.verticalCenter; width: parent.width-20; text: modelData.label; textFormat: Text.PlainText; elide: Text.ElideRight; font.pixelSize: 14; color: Theme.ink }
                    MouseArea { anchors.fill: parent; enabled: root.active; onClicked: root.album.chooseFilter(index) }
                }
            }
        }
    }
}
