import QtQuick

Item {
    id: root
    required property var shell
    readonly property var hall: shell.hall
    readonly property var summary: hall.gallery
    readonly property bool detailOpen: hall.route === "achievement-detail"
    readonly property bool active: visible && !shell.serviceOpen && !shell.menuOpen && !shell.drawerOpen
        && !shell.quickAppsOpen && !shell.keyboard.open && !shell.notice.length && !hall.account.open
    readonly property bool browsing: active && !detailOpen && !hall.galleryMenuOpen
    objectName: "achievement-gallery"
    Item {
        anchors.fill: parent; visible: !root.detailOpen
        Item {
            id: header; width: parent.width; height: root.height < 300 ? 52 : 68
            Text {
                x: 8; y: 0; width: parent.width-230; height: 32
                text: root.summary.title || root.shell.home.adventure || "Your achievements"
                textFormat: Text.PlainText; elide: Text.ElideRight
                font.family: Theme.displayFamily; font.pixelSize: 25; color: Theme.ink
            }
            Text {
                x: 8; y: 32; width: parent.width-230; text: root.summary.status
                color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight
            }
            Text {
                anchors.right: parent.right; anchors.rightMargin: 10; y: 1
                text: root.summary.total > 0
                    ? root.summary.earned + " / " + root.summary.total + " earned" : "— / —"
                color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 21
            }
            Text {
                anchors.right: parent.right; anchors.rightMargin: 10; y: 32; width: 210
                horizontalAlignment: Text.AlignRight; textFormat: Text.PlainText; elide: Text.ElideRight
                text: root.summary.unknown > 0 ? root.summary.unknown + " not yet confirmed" : root.summary.account || "Connect an RA account"
                color: Theme.muted; font.pixelSize: 11
            }
            Rectangle {
                x: 8; y: header.height-5; width: parent.width-18; height: 3; radius: 1.5
                color: Qt.alpha(Theme.rim,.25)
                Rectangle {
                    height: parent.height; radius: parent.radius; color: Theme.rim
                    width: root.summary.complete ? parent.width * root.summary.earned / root.summary.total : 0
                }
            }
        }
        GridView {
            id: grid; objectName: "achievement-grid"
            x: 0; y: header.height; width: parent.width; height: Math.max(80,parent.height-y-Theme.adventureCutoutHeight-22)
            cellWidth: width/6; cellHeight: height/2
            clip: true; model: root.hall.rows; currentIndex: root.hall.rowIndex
            interactive: root.browsing; boundsBehavior: Flickable.StopAtBounds
            snapMode: GridView.SnapToRow; cacheBuffer: cellHeight*2
            highlightMoveDuration: Theme.motion(130)
            function reveal() { if(currentIndex>=0) positionViewAtIndex(currentIndex,GridView.Contain) }
            onCurrentIndexChanged: reveal()
            onVisibleChanged: if(visible)Qt.callLater(reveal)
            onHeightChanged: Qt.callLater(reveal)
            Connections { target: root.hall; function onRowsChanged() { Qt.callLater(grid.reveal) } }
            onMovementEnded: {
                if(!root.browsing || !count)return
                const first = Math.max(0,Math.round(contentY/cellHeight)*6)
                if(currentIndex<first || currentIndex>=first+12)root.hall.focusAchievement(Math.min(count-1,first))
            }
            delegate: Item {
                id: trophy
                required property var modelData
                required property int index
                width: grid.cellWidth; height: grid.cellHeight
                readonly property bool selected: root.browsing && index===root.hall.rowIndex
                readonly property bool earned: modelData.earnedState==="earned"
                readonly property bool locked: modelData.earnedState==="locked"
                readonly property color metal: earned ? Theme.yellow : locked ? "#bac9c0" : "#cbbdd0"
                objectName: "achievement-card-"+index
                onSelectedChanged: if(selected)forceActiveFocus()
                Component.onCompleted: if(selected)forceActiveFocus()
                Rectangle {
                    x: medal.x-4; y: medal.y-4; width: medal.width+8; height: medal.height+8; radius: 20; color: "transparent"
                    border.width: 5; border.color: Qt.alpha(Theme.focusGlow,.14); visible: trophy.selected
                }
                Rectangle {
                    x: medal.x-2; y: medal.y-2; width: medal.width+4; height: medal.height+4; radius: 19; color: "transparent"
                    border.width: 1; border.color: Theme.focusGlow; visible: trophy.selected
                }
                Item {
                    id: medal
                    width: Math.min(78,Math.max(32,trophy.height-(trophy.height < 100 ? 50 : 55))); height: width
                    anchors.horizontalCenter: parent.horizontalCenter; y: 6
                    Rectangle { x: 2; y: 4; width: parent.width; height: parent.height; radius: 18; color: "#25283a30" }
                    Rectangle {
                        anchors.fill: parent; radius: 17; border.width: 1; border.color: Qt.darker(trophy.metal,1.3)
                        gradient: Gradient {
                            GradientStop { position: 0; color: Qt.lighter(trophy.metal,1.2) }
                            GradientStop { position: .52; color: trophy.metal }
                            GradientStop { position: 1; color: Qt.darker(trophy.metal,1.3) }
                        }
                    }
                    Rectangle { anchors.fill: parent; anchors.margins: 3; radius: 14; color: "transparent"; border.color: "#bffff8df" }
                    Rectangle { anchors.fill: parent; anchors.margins: 5; radius: 10; color: Qt.darker(trophy.metal,1.25) }
                    Image {
                        id: badge; anchors.fill: parent; anchors.margins: 7
                        source: root.visible ? trophy.modelData.icon : ""; sourceSize: Qt.size(128,128)
                        fillMode: Image.PreserveAspectFit; asynchronous: true
                        opacity: trophy.earned ? 1 : .7
                    }
                    Text {
                        anchors.centerIn: parent; visible: badge.status!==Image.Ready
                        text: trophy.earned ? "★" : trophy.locked ? "◇" : "?"
                        color: Theme.paper; font.pixelSize: Math.min(33,medal.width*.48)
                    }
                    Rectangle {
                        width: 18; height: 18; radius: 9; x: parent.width-17; y: parent.height-17
                        color: trophy.earned ? Theme.green : Theme.paper; border.color: trophy.metal
                        Text { anchors.centerIn: parent; text: trophy.earned ? "✓" : trophy.locked ? "−" : "?"; font.pixelSize: 12; color: Theme.ink }
                    }
                }
                Text {
                    x: 9; y: medal.y+medal.height+4; width: parent.width-18; height: trophy.height < 100 ? 26 : 30
                    text: trophy.modelData.title; textFormat: Text.PlainText; wrapMode: Text.WordWrap
                    maximumLineCount: 2; elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter
                    font.family: Theme.displayFamily; font.pixelSize: trophy.height < 100 ? 10.5 : 12; color: Theme.ink
                }
                Text {
                    y: parent.height-13; width: parent.width; horizontalAlignment: Text.AlignHCenter
                    text: trophy.earned ? "✓ " + (trophy.modelData.mode || "Earned") : trophy.locked ? "Locked" : "Unknown"
                    color: Theme.muted; font.pixelSize: 9
                }
                MouseArea { anchors.fill: parent; enabled: root.browsing; onClicked: root.shell.activate(trophy.index,"list") }
            }
        }
        Column {
            anchors.centerIn: grid; spacing: 14; width: Math.min(430,grid.width-40); visible: !grid.count
            Row {
                anchors.horizontalCenter: parent.horizontalCenter; spacing: 18
                Repeater {
                    model: 3
                    Rectangle {
                        required property int index
                        width: 57; height: 64; radius: 15; rotation: (index-1)*9
                        color: Qt.alpha(index===1 ? Theme.yellow : Theme.rim,.22); border.color: Qt.alpha(Theme.rim,.5)
                        Text { anchors.centerIn: parent; text: "☆"; font.pixelSize: 31; color: Theme.muted; opacity: .55 }
                    }
                }
            }
            Text { width: parent.width; text: root.summary.empty; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; color: Theme.muted; font.pixelSize: 14 }
        }
        Text {
            x: 9; y: grid.y+grid.height+3; width: parent.width-18; horizontalAlignment: Text.AlignRight
            text: root.summary.filter + "  ·  " + root.summary.sort + "   ☰"
            color: Theme.muted; font.pixelSize: 11
            MouseArea { anchors.fill: parent; anchors.topMargin: -7; anchors.bottomMargin: -7; enabled: root.browsing; onClicked: root.hall.openGalleryMenu() }
        }
        Rectangle {
            anchors.right: grid.right; y: grid.y; width: 2; height: grid.height; radius: 1
            color: Qt.alpha(Theme.rim,.2); visible: grid.contentHeight>grid.height+1
            Rectangle { width: 2; radius: 1; color: Theme.rim; height: parent.height*grid.visibleArea.heightRatio; y: parent.height*grid.visibleArea.yPosition }
        }
    }
    HallOfFamePage { anchors.fill: parent; shell: root.shell; visible: root.detailOpen }
    Rectangle {
        anchors.fill: parent; visible: root.hall.galleryMenuOpen; color: "#b3fffef9"; z: 5
        MouseArea { anchors.fill: parent; enabled: root.active; onClicked: root.hall.closeGalleryMenu() }
        Rectangle {
            width: Math.min(290,parent.width-24); height: Math.min(parent.height-12,338)
            anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 6
            radius: 12; color: Theme.paper; border.color: Theme.rim; border.width: 2
            ListView {
                id: options; anchors.fill: parent; anchors.margins: 12; clip: true
                model: root.hall.galleryOptions; currentIndex: root.hall.galleryMenuFocus
                onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
                delegate: Item {
                    required property var modelData
                    required property int index
                    width: options.width; height: index===4 || index===7 ? 39 : 32
                    opacity: modelData.enabled ? 1 : .45
                    Rectangle { width: parent.width; height: 1; color: Qt.alpha(Theme.rim,.4); visible: index===4 || index===7 }
                    Rectangle { anchors.fill: parent; anchors.margins: 2; radius: 6; color: Qt.alpha(Theme.blue,.25); visible: index===root.hall.galleryMenuFocus }
                    Text { x: 10; anchors.verticalCenter: parent.verticalCenter; width: parent.width-20; text: (modelData.checked ? "✓  " : "    ")+modelData.label; color: Theme.ink; font.pixelSize: 13 }
                    MouseArea { anchors.fill: parent; enabled: root.active && modelData.enabled; onClicked: root.hall.chooseGalleryOption(index) }
                }
            }
        }
    }
    AchievementAccountPanel { anchors.fill: parent; shell: root.shell; visible: root.hall.account.open && !root.detailOpen; z: 6 }
}
