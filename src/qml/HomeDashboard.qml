import QtQuick

Item {
    id: root
    required property var shell
    readonly property var flow: shell.dashboard
    readonly property bool generic: flow.genericLayout
    readonly property bool active: visible && shell.unobstructed && !shell.quickAppsOpen && !shell.drawerOpen && !shell.keyboard.open
    readonly property real unit: 1
    readonly property real gap: 16 * unit
    readonly property real split: width * .33
    readonly property real usableHeight: height - (shell.chooseAdventureAvailable ? Theme.adventureCutoutHeight : 0) - 10
    function hints(h) { if (!visible) return null; return flow.focus === "play" ? [h("A",shell.liveGame.id ? "Return" : shell.homeGame.id ? shell.home.actionHint : "Choose a game"),h("Select","Game actions"),h("B","Back")] : [h("A","Open"),h("B","Play focus")] }

    // One quiet social area, with individual controls rather than a wall of tiles.
    Item {
        id: activity
        x: root.gap; y: 12 * root.unit
        width: root.split - 2 * root.gap
        height: root.usableHeight - (root.generic ? 0 : awards.height + root.gap) - y
        Text { id: activityTitle; text: "Activity"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 23 * root.unit }
        ListView {
            id: chatRow; objectName: "home-people"
            y: 38 * root.unit; width: parent.width; height: 80 * root.unit
            orientation: ListView.Horizontal; clip: true; spacing: 8 * root.unit
            model: root.flow.people
            currentIndex: { for(let i=0;i<model.length;i++)if(model[i].key===root.flow.focus)return i;return -1 }
            onCurrentIndexChanged: if(currentIndex>=0)positionViewAtIndex(currentIndex,ListView.Contain)
            delegate: Item {
                    required property var modelData
                    required property int index
                    objectName: "home-person-"+index
                    width: (chatRow.width-3*chatRow.spacing)/4; height: chatRow.height
                    readonly property real diameter: Math.min(46,width-4)
                    Rectangle { x: (parent.width-width)/2; y: 3; width: parent.diameter; height: width; radius: width / 2; color: "#88998d" }
                    Rectangle {
                        x: (parent.width-width)/2; width: parent.diameter; height: width; radius: width / 2
                        color: [Theme.blue,Theme.pink,Theme.green,Theme.yellow][index%4]
                        border.color: root.active && root.flow.focus === modelData.key ? Theme.focus : "#f8fff1"
                        border.width: root.active && root.flow.focus === modelData.key ? 3 : 2
                        Image { id: avatar; anchors.fill: parent; anchors.margins: 5; source: modelData.avatar || ""; asynchronous: true; sourceSize: Qt.size(96,96); fillMode: Image.PreserveAspectFit; visible: status === Image.Ready }
                        Text { anchors.centerIn: parent; visible: avatar.status !== Image.Ready; text: (modelData.name || "?").substring(0,1); color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 25 * root.unit }
                        Rectangle { anchors.right: parent.right; anchors.top: parent.top; width: 11 * root.unit; height: width; radius: width / 2; color: Theme.yellow; border.width: 2; border.color: Theme.paper; visible: modelData.unread > 0 }
                    }
                    Text { objectName: "home-person-name"; y: parent.diameter+7; width: parent.width; text: modelData.name || "?"; color: Theme.ink; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight; maximumLineCount: 1 }
                    MouseArea { anchors.fill: parent; onClicked: root.shell.activateDashboard(modelData.key) }
                    Accessible.role: Accessible.Button; Accessible.name: modelData.name
                    Accessible.onPressAction: root.shell.activateDashboard(modelData.key)
            }
        }
        Item {
            visible: root.flow.people.length === 0 && root.flow.activities.length === 0
            x: 3*root.unit; y: 48*root.unit; width: parent.width; height: 92*root.unit
            Rectangle {
                x: 3; y: 9*root.unit; width: 67*root.unit; height: 55*root.unit; radius: 17*root.unit
                color: "#b484a5"; rotation: -10; border.color: "#edd3d9"; border.width: 2
            }
            Rectangle {
                x: 17*root.unit; y: 2*root.unit; width: 65*root.unit; height: 55*root.unit; radius: 17*root.unit
                color: Theme.blue; rotation: 7; border.color: "#e8faff"; border.width: 2
                Image { anchors.centerIn: parent; width: 36*root.unit; height: width; source: "qrc:/social-icons/users.svg" }
            }
            Column {
                x: 98*root.unit; y: 8*root.unit; width: parent.width-x; spacing: 7*root.unit
                Text { width: parent.width; text: "Better together"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 19*root.unit; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
                Text { width: parent.width; text: "Friends, messages and invitations\nmeet here."; color: Theme.muted; font.pixelSize: 13*root.unit; wrapMode: Text.WordWrap }
            }
        }
        ListView {
            id: events; objectName: "home-activity"
            y: root.flow.people.length ? chatRow.y + chatRow.height + 8 * root.unit : 44 * root.unit
            width: parent.width; height: Math.max(0, parent.height-y)
            clip: true; spacing: 9 * root.unit; model: root.flow.activities
            currentIndex: { for(let i=0;i<model.length;i++) if(model[i].key===root.flow.focus)return i;return -1 }
            onCurrentIndexChanged: if(currentIndex>=0)positionViewAtIndex(currentIndex,ListView.Contain)
            Connections { target: root.flow; function onScrollRequested(direction) { events.contentY=Math.max(events.originY,Math.min(events.originY+Math.max(0,events.contentHeight-events.height),events.contentY+direction*35*root.unit)) } }
            delegate: Item {
                id: eventRow
                required property var modelData
                width: events.width; height: 52
                readonly property bool selected: root.active && root.flow.focus===modelData.key
                Rectangle { anchors.fill: parent; anchors.margins: 1; radius: 8; color: eventRow.selected ? "#30e8c66c" : eventPointer.containsMouse ? "#182f716b" : "transparent"; border.width: eventRow.selected ? 2 : 0; border.color: Theme.focus }
                Rectangle { x: 5; y: 14; width: 24; height: 24; radius: 12; color: modelData.request ? Theme.pink : modelData.activity ? Theme.green : Theme.blue; opacity: .6 }
                Text { x: 5; y: 13; width: 24; horizontalAlignment: Text.AlignHCenter; text: modelData.request ? "+" : modelData.activity ? "↗" : "·"; color: Theme.ink; font.pixelSize: 21 }
                Text { x: 38; y: 6; width: parent.width-x-22; text: modelData.name || "Activity"; color: Theme.ink; font.pixelSize: 14; elide: Text.ElideRight }
                Text { x: 38; y: 27; width: parent.width-x-22; text: modelData.detail || ""; color: Theme.muted; font.pixelSize: 12; elide: Text.ElideRight }
                Text { anchors.right: parent.right; anchors.rightMargin: 6; anchors.verticalCenter: parent.verticalCenter; text: "›"; color: eventRow.selected ? Theme.ink : Theme.muted; font.pixelSize: 24 }
                Rectangle { x: 38; y: parent.height-1; width: parent.width-x-8; height: 1; color: "#183c655e" }
                MouseArea { id: eventPointer; anchors.fill: parent; hoverEnabled: true; onClicked: root.shell.activateDashboard(modelData.key) }
                Accessible.role: Accessible.Button; Accessible.name: (modelData.name || "Activity")+", "+(modelData.detail || "")
                Accessible.onPressAction: root.shell.activateDashboard(modelData.key)
            }
            Row {
                y: 9 * root.unit; spacing: 9 * root.unit; visible: !events.count && root.flow.people.length > 0
                Text { text: "✓"; color: Theme.green; font.pixelSize: 18 * root.unit }
                Text { text: "All caught up"; color: Theme.muted; font.pixelSize: 14 * root.unit }
            }
        }
    }
    Item {
        id: awards
        objectName: "home-awards"
        readonly property bool compact: root.generic && showcase.height < 330
        parent: root.generic ? showcase : root
        x: root.generic ? 20 : activity.x
        y: root.generic ? parent.height-height-13 : root.usableHeight-height
        width: root.generic ? parent.width-play.width-60 : activity.width
        height: root.generic ? (compact ? 128 : 148) : 139
        Rectangle { visible: !root.generic; width: parent.width; height: 1; color: "#70c6d6b6" }
        Rectangle { visible: !root.generic; y: 1; width: parent.width; height: 1; color: "#203a4635" }
        Text { y: root.generic ? 6 : 13; width: parent.width; elide: Text.ElideRight; text: "Recent achievements"; color: root.generic ? "#fff4ce" : Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 19 * root.unit }
        ListView {
            id: trophies; objectName: "home-achievements"
            y: root.generic ? (awards.compact ? 29 : 34) : 45; width: parent.width; height: root.generic ? (awards.compact ? 84 : 103) : 70
            orientation: ListView.Horizontal; clip: true; spacing: root.generic ? 6 : 10; model: root.flow.achievements
            currentIndex: { for(let i=0;i<model.length;i++)if(model[i].key===root.flow.focus)return i;return -1 }
            onCurrentIndexChanged: if(currentIndex>=0)positionViewAtIndex(currentIndex,ListView.Contain)
            delegate: Item {
                required property var modelData
                required property int index
                objectName: "home-award-"+index
                width: root.generic ? (trophies.width-4*trophies.spacing)/5 : 62; height: trophies.height
                Rectangle {
                    id: medallion
                    x: root.generic ? (parent.width-width)/2 : 2; y: 5; width: root.generic ? Math.min(parent.width-6,awards.compact ? 46 : 58) : 58; height: width; radius: width/2
                    color: "#234b46"; border.color: "#769f87"; border.width: 2
                    Rectangle {
                        anchors.fill: parent; anchors.margins: 4; radius: width/2
                        gradient: Gradient { GradientStop { position: 0; color: "#ffe1a0" } GradientStop { position: 1; color: "#ae773f" } }
                        border.color: "#f3cf85"; border.width: 2
                        Image {
                            id: badge; anchors.fill: parent; anchors.margins: 7
                            source: root.visible ? modelData.icon || "" : ""
                            asynchronous: true; cache: true; sourceSize: Qt.size(96,96); fillMode: Image.PreserveAspectFit
                        }
                        Text { anchors.centerIn: parent; visible: badge.status !== Image.Ready; text: "★"; font.pixelSize: 26*root.unit; color: "#80542e" }
                    }
                    Rectangle { anchors.fill: parent; anchors.margins: -2; radius: width/2; color: "transparent"; border.color: Theme.focus; border.width: 3; visible: root.active && root.flow.focus===modelData.key }
                }
                Text {
                    objectName: "home-achievement-caption"
                    visible: root.generic; y: medallion.y+medallion.height+6; width: parent.width
                    text: modelData.name; color: "#fff4ce"; font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                    maximumLineCount: 2; elide: Text.ElideRight
                }
                MouseArea { anchors.fill: parent; onClicked: root.shell.activateDashboard(modelData.key) }
                Accessible.role: Accessible.Button; Accessible.name: modelData.name + ", " + modelData.game
                Accessible.onPressAction: root.shell.activateDashboard(modelData.key)
            }
            Row {
                visible: !trophies.count; spacing: root.generic ? trophies.spacing : 13; y: 5
                Repeater {
                    model: root.generic ? 5 : 3
                    Item {
                        width: root.generic ? (trophies.width-4*trophies.spacing)/5 : 54; height: root.generic ? 80 : 54
                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: root.generic ? Math.min(parent.width-6,awards.compact ? 46 : 58) : 54
                            height: width; radius: width/2; color: "#80244844"; border.color: "#9eae89"; border.width: 2
                            Rectangle { anchors.fill: parent; anchors.margins: 5; radius: width/2; color: "transparent"; border.color: "#7d916f" }
                            Text { anchors.centerIn: parent; text: "☆"; color: "#b9c6a2"; font.pixelSize: 29*root.unit }
                        }
                    }
                }
            }
        }
        Text {
            visible: !root.generic || trophies.count === 0
            y: root.generic ? parent.height-18 : 113; width: parent.width
            text: {
                const rows = root.flow.achievements
                let chosen = rows.length ? rows[0] : null
                for (let i=0; i<rows.length; ++i) if(rows[i].key===root.flow.focus) chosen=rows[i]
                return chosen ? chosen.name + (chosen.offline ? " · Offline" : "") : "Your first achievements await"
            }
            color: root.generic ? "#e1ebd4" : Theme.muted; font.pixelSize: 12*root.unit; elide: Text.ElideRight
        }
    }
    // The adapter owns the showcase; the shell keeps a single compact launch control.
    Rectangle {
        id: showcase; objectName: "home-game-card"
        x: root.split + 2; y: 7 * root.unit
        width: parent.width-x-12*root.unit; height: parent.height-y-13*root.unit
        radius: 18 * root.unit; color: Theme.homeLandscape[6]; border.color: Theme.homeLandscape[4]; border.width: 2
        HomeCardBackdrop {
            objectName: "home-card-illustration"
            anchors.fill: parent; anchors.margins: 3
        }
        Rectangle { anchors.fill: parent; anchors.margins: 4; radius: parent.radius-4; color: "transparent"; border.color: Theme.homeLandscape[5]; opacity: .35 }
        Item {
            objectName: "home-description"
            x: 14; y: 14
            width: root.generic ? parent.width*.54-24 : parent.width-28
            height: root.generic ? Math.max(0,metadata.y-y-8) : parent.height-y-107
            clip: true; enabled: root.generic
            ExperienceHost { anchors.fill: parent; shell: root.shell; showcase: true }
        }
        HomeMetadataChips {
            id: metadata
            objectName: "home-metadata"
            visible: root.generic && !!root.shell.homeGame.id
            x: 20; y: awards.y-height-12; width: parent.width*.54-28
            compact: awards.compact
            game: root.shell.homeGame
        }
        Rectangle { visible: root.generic; x: 20; y: awards.y; width: parent.width-40; height: 1; color: "#70c6d6b6" }
        AdventureButton {
            id: play
            anchors { right: parent.right; bottom: parent.bottom; rightMargin: 16*root.unit; bottomMargin: 10*root.unit }
            width: root.generic ? (awards.compact ? 116 : 132) : 96; height: width
            showGlyph: !root.generic
            shell: root.shell; selected: root.active && root.flow.focus === "play"
        }
        HomeScreensCollage {
            objectName: "home-screens"
            x: root.generic ? parent.width*.55 : 22
            y: root.generic ? 18 : play.y+(play.height-height)/2
            width: root.generic ? parent.width*.45-22 : play.x-x-24
            height: root.generic ? awards.y-36 : 98
            balanced: root.generic
            source: root.shell.home.exitPreviewLabel ? root.shell.home.exitPreview : ""
            selected: root.active && root.flow.focus === "screens"
            onActivated: root.shell.activateDashboard("screens")
        }
    }
}
