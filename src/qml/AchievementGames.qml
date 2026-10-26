import QtQuick

Item {
    id: root
    required property var shell
    readonly property var hall: shell.hall
    readonly property bool active: visible && (!shell.serviceOpen || (shell.profileContentActive && shell.profileRoute === "ra")) && !shell.menuOpen && !shell.drawerOpen
        && !shell.quickAppsOpen && !shell.keyboard.open && !shell.notice.length && !hall.account.open && !hall.historyOpen
    objectName: "achievement-games"
    Text {
        x: 10; y: 2; width: parent.width-300; height: 32
        text: "Your trophy collection"; font.family: Theme.displayFamily; font.pixelSize: 25; color: Theme.ink
        elide: Text.ElideRight
    }
    Text {
        x: 11; y: 34; width: parent.width-200; text: "Choose a game and explore your achievements"
        color: Theme.muted; font.pixelSize: 12; elide: Text.ElideRight
    }
    Text {
        anchors.right: parent.right; anchors.rightMargin: 12; y: 13; text: "RA account  \u203a"
        font.pixelSize: 13; color: Theme.ink
        MouseArea { anchors.fill: parent; anchors.margins: -10; enabled: root.active; onClicked: root.hall.openAchievementAccount() }
    }
    Text {
        anchors.right: parent.right; anchors.rightMargin: 12; y: 37; text: "Account history  \u203a"
        font.pixelSize: 12; color: Theme.muted
        MouseArea {anchors.fill: parent; anchors.margins: -5; enabled: root.active; onClicked: root.hall.openHistory()}
    }
    GridView {
        id: games; objectName: "achievement-games-grid"
        x: (parent.width-width)/2; y: 54; width: (parent.width-16)*Math.min(5,Math.max(1,count))/5; height: Math.max(140,parent.height-y-Theme.adventureCutoutHeight-14)
        cellWidth: (root.width-16)/5; cellHeight: height; clip: true
        model: root.visible ? root.hall.rows : []; currentIndex: root.hall.rowIndex
        interactive: root.active; boundsBehavior: Flickable.StopAtBounds; snapMode: GridView.SnapToRow
        cacheBuffer: 0
        function reveal() {if(root.visible && count)positionViewAtIndex(currentIndex,GridView.Contain)}
        onCurrentIndexChanged: reveal()
        onVisibleChanged: if(visible)Qt.callLater(reveal)
        onHeightChanged: Qt.callLater(reveal)
        Connections {target: root.hall; function onRowsChanged(){Qt.callLater(games.reveal)}}
        onMovementEnded: {
            const first=Math.max(0,Math.round(contentY/cellHeight)*5)
            if(root.active && count && (currentIndex<first || currentIndex>=first+5))root.hall.focusAchievementGame(Math.min(count-1,first))
        }
        delegate: Item {
            id: card
            required property var modelData
            required property int index
            width: games.cellWidth; height: games.cellHeight
            readonly property bool selected: root.active && games.currentIndex===index
            objectName: "achievement-game-"+index
            onSelectedChanged: if(selected)forceActiveFocus()
            Component.onCompleted: if(selected)forceActiveFocus()
            Item {
                id: cover
                anchors.horizontalCenter: parent.horizontalCenter
                readonly property real aspect: artwork.status===Image.Ready && artwork.implicitHeight>0 ? Math.max(.6,Math.min(1.6,artwork.implicitWidth/artwork.implicitHeight)) : 2/3
                width: Math.min(parent.width-24,(parent.height-79)*aspect); height: width/aspect+26
                y: 9+(parent.height-53)-height
                Rectangle {x: 3; y: 5; width: parent.width; height: parent.height; radius: 11; color: Theme.surfaceColor("#30222c29")}
                Rectangle {
                    anchors.fill: parent; radius: 10; border.color: Qt.darker(Theme.rim,1.3)
                    gradient: Gradient {
                        GradientStop {position: 0; color: Qt.lighter(Theme.rim,1.55)}
                        GradientStop {position: 1; color: Qt.darker(Theme.rim,1.12)}
                    }
                }
                Rectangle {anchors.fill: parent; anchors.margins: 3; radius: 8; color: Theme.paper; border.color: Theme.surfaceColor("#aafff4d1")}
                Image {
                    id: artwork; anchors.fill: parent; anchors.margins: 6; anchors.bottomMargin: 32
                    source: card.modelData.cover || "qrc:/cover-fallbacks/neutral-"+(card.index%2)+".png"
                    sourceSize: Qt.size(300,450); fillMode: Image.PreserveAspectFit; asynchronous: true
                }
                Text {anchors.centerIn: parent; text: "\u2606"; font.pixelSize: 52; color: Theme.yellow; visible: artwork.status!==Image.Ready}
                Rectangle {
                    anchors.fill: parent; anchors.margins: -3; radius: 13; color: "transparent"
                    border.width: 5; border.color: Qt.alpha(Theme.focusGlow,.23); visible: card.selected
                }
                Rectangle {
                    anchors.fill: parent; anchors.margins: -1; radius: 11; color: "transparent"
                    border.width: 1; border.color: Theme.focusGlow; visible: card.selected
                }
                Rectangle {
                    x: 6; y: parent.height-29; width: parent.width-12; height: 23; radius: 4; color: Qt.alpha(Theme.paper,.93)
                    Text {anchors.centerIn: parent; text: card.modelData.complete ? card.modelData.earned+" / "+card.modelData.total : card.modelData.total>0?card.modelData.earned+" \u00b7 ?":"\u2014"; font.pixelSize: 11; color: Theme.ink}
                    Rectangle {x: 3; y: parent.height-3; height: 2; width: (parent.width-6)*(card.modelData.earned || 0)/Math.max(1,card.modelData.total || 0); color: Theme.rim; visible: card.modelData.complete === true}
                }
                Rectangle {
                    width: 32; height: 32; radius: 16; x: parent.width-22; y: parent.height-23
                    color: card.modelData.complete && card.modelData.earned===card.modelData.total ? Theme.yellow : Theme.paper
                    border.width: 2; border.color: Theme.rim
                    Text {anchors.centerIn: parent; text: "\u2605"; color: Theme.ink; font.pixelSize: 18}
                }
            }
            Text {
                id: name; x: 8; y: cover.y+cover.height+10; width: parent.width-16; height: 32
                text: card.modelData.title; textFormat: Text.PlainText; font.family: Theme.displayFamily; font.pixelSize: 13
                wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight; color: Theme.ink; horizontalAlignment: Text.AlignHCenter
            }
            MouseArea {anchors.fill: parent; enabled: root.active; onClicked: root.shell.activate(card.index,"list")}
        }
    }
    Text {
        x: 12; y: games.y+games.height+5; width: parent.width-100; height: 18
        text: games.count ? ((games.model[games.currentIndex] || {}).platform || "")+" \u00b7 "+((games.model[games.currentIndex] || {}).status || "") : ""
        font.pixelSize: 11; color: Theme.muted; elide: Text.ElideRight
    }
    Text {
        anchors.right: parent.right; anchors.rightMargin: 12; y: games.y+games.height+5
        text: games.count ? (Math.floor(games.currentIndex/5)+1)+" / "+Math.ceil(games.count/5)+(games.count>5 ? "  \u2195" : "") : ""
        font.pixelSize: 11; color: Theme.muted
    }
    Column {
        anchors.centerIn: parent; width: Math.min(440,parent.width-40); spacing: 15; visible: !games.count
        Text {anchors.horizontalCenter: parent.horizontalCenter; text: "\u2606   \u2605   \u2606"; font.pixelSize: 44; color: Theme.rim; opacity: .65}
        Text {width: parent.width; text: "Every game has a story"; horizontalAlignment: Text.AlignHCenter; font.family: Theme.displayFamily; font.pixelSize: 24; color: Theme.ink}
        Text {width: parent.width; text: root.hall.emptyMessage; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 14; color: Theme.muted}
        CapButton {anchors.horizontalCenter: parent.horizontalCenter; width: 180; height: 34; label: "RA account"; tint: Theme.blue; selected: root.active; onActivated: root.hall.openAchievementAccount()}
    }
}
