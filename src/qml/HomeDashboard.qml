import QtQuick

Item {
    id: root
    required property var shell
    readonly property var flow: shell.dashboard
    readonly property bool active: visible && shell.unobstructed && !shell.quickAppsOpen && !shell.drawerOpen && !shell.keyboard.open
    readonly property real unit: Math.max(1, Math.min(1.65, width / 920, height / 380))
    readonly property real gap: 16 * unit
    readonly property real split: width * .49
    readonly property real usableHeight: height - (shell.chooseAdventureAvailable ? Theme.adventureCutoutHeight : 0) - 10
    function hints(h) { if (!visible) return null; return flow.focus === "play" ? [h("A",shell.liveGame.id ? "Return" : shell.homeGame.id ? shell.home.actionHint : "Choose a game"),h("Select","Game actions"),h("B","Back")] : [h("A","Open"),h("B","Play focus")] }

    Image { anchors.fill: parent; source: "qrc:/series/multiverse.png"; fillMode: Image.PreserveAspectCrop; asynchronous: true }
    Rectangle { anchors.fill: parent; color: "#55163335" }
    Rectangle {
        width: root.split; height: parent.height
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: "#de153b3c" }
            GradientStop { position: 1; color: "#72153b3c" }
        }
    }

    // One quiet social area, with individual controls rather than a wall of tiles.
    Item {
        id: activity
        x: root.gap; y: 12 * root.unit
        width: root.split - 2 * root.gap
        height: root.usableHeight - awards.height - root.gap - y
        Text { id: activityTitle; text: "Activity"; color: "#fff4ce"; font.family: Theme.displayFamily; font.pixelSize: 23 * root.unit }
        Row {
            id: chatRow
            y: 38 * root.unit; spacing: 11 * root.unit
            Repeater {
                model: root.flow.chats
                Item {
                    required property var modelData
                    width: Math.min(52 * root.unit, (activity.width - chatRow.spacing * 5) / 6); height: width + 9 * root.unit
                    Rectangle { y: 3; width: parent.width; height: width; radius: width / 2; color: "#88998d" }
                    Rectangle {
                        width: parent.width; height: width; radius: width / 2
                        color: modelData.kind === "groups" ? Theme.pink : modelData.guild ? Theme.green : Theme.blue
                        border.color: root.active && root.flow.focus === modelData.key ? Theme.focus : "#f8fff1"
                        border.width: root.active && root.flow.focus === modelData.key ? 3 : 2
                        Image { id: avatar; anchors.fill: parent; anchors.margins: 5; source: modelData.avatar || ""; asynchronous: true; sourceSize: Qt.size(96,96); fillMode: Image.PreserveAspectFit; visible: status === Image.Ready }
                        Text { anchors.centerIn: parent; visible: avatar.status !== Image.Ready; text: (modelData.name || "?").substring(0,1); color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 25 * root.unit }
                        Rectangle { anchors.right: parent.right; anchors.top: parent.top; width: 11 * root.unit; height: width; radius: width / 2; color: Theme.yellow; border.width: 2; border.color: Theme.paper; visible: modelData.unread > 0 }
                    }
                    MouseArea { anchors.fill: parent; onClicked: root.shell.activateDashboard(modelData.key) }
                    Accessible.role: Accessible.Button; Accessible.name: modelData.name
                    Accessible.onPressAction: root.shell.activateDashboard(modelData.key)
                }
            }
        }
        Item {
            visible: root.flow.chats.length === 0 && root.flow.activities.length === 0
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
                Text { width: parent.width; text: "Better together"; color: "#fff4ce"; font.family: Theme.displayFamily; font.pixelSize: 20*root.unit; elide: Text.ElideRight }
                Text { width: parent.width; text: "Friends, messages and invitations\nmeet here."; color: "#d6e8df"; font.pixelSize: 13*root.unit; wrapMode: Text.WordWrap }
            }
        }
        ListView {
            id: events; objectName: "home-activity"
            y: root.flow.chats.length ? chatRow.y + 68 * root.unit : 44 * root.unit
            width: parent.width; height: Math.max(0, parent.height-y)
            clip: true; spacing: 9 * root.unit; model: root.flow.activities
            currentIndex: { for(let i=0;i<model.length;i++) if(model[i].key===root.flow.focus)return i;return -1 }
            onCurrentIndexChanged: if(currentIndex>=0)positionViewAtIndex(currentIndex,ListView.Contain)
            Connections { target: root.flow; function onScrollRequested(direction) { events.contentY=Math.max(events.originY,Math.min(events.originY+Math.max(0,events.contentHeight-events.height),events.contentY+direction*35*root.unit)) } }
            delegate: CapButton {
                required property var modelData
                width: events.width - 4; x: 2; height: 49 * root.unit; textSize: 14 * root.unit; tint: Theme.blue
                label: modelData.name || "Activity"; detail: modelData.detail || ""
                selected: root.active && root.flow.focus===modelData.key; claimsFocus: false
                onActivated: root.shell.activateDashboard(modelData.key)
            }
            Row {
                y: 9 * root.unit; spacing: 9 * root.unit; visible: !events.count && root.flow.chats.length > 0
                Text { text: "✓"; color: Theme.green; font.pixelSize: 18 * root.unit }
                Text { text: "All caught up"; color: "#e7ecd8"; font.pixelSize: 14 * root.unit }
            }
        }
    }
    Item {
        id: awards
        x: activity.x; y: root.usableHeight-height; width: activity.width; height: 139 * root.unit
        Rectangle { width: parent.width; height: 1; color: "#70c6d6b6" }
        Rectangle { y: 1; width: parent.width; height: 1; color: "#203a4635" }
        Text { y: 13 * root.unit; text: "Recent achievements"; color: "#fff4ce"; font.family: Theme.displayFamily; font.pixelSize: 19 * root.unit }
        ListView {
            id: trophies; objectName: "home-achievements"
            y: 45 * root.unit; width: parent.width; height: 70 * root.unit
            orientation: ListView.Horizontal; clip: true; spacing: 10 * root.unit; model: root.flow.achievements
            currentIndex: { for(let i=0;i<model.length;i++)if(model[i].key===root.flow.focus)return i;return -1 }
            onCurrentIndexChanged: if(currentIndex>=0)positionViewAtIndex(currentIndex,ListView.Contain)
            delegate: Item {
                required property var modelData
                width: 62 * root.unit; height: trophies.height
                Rectangle {
                    x: 2; y: 5; width: 58*root.unit; height: width; radius: width/2
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
                    MouseArea { anchors.fill: parent; onClicked: root.shell.activateDashboard(modelData.key) }
                    Accessible.role: Accessible.Button; Accessible.name: modelData.name + ", " + modelData.game
                    Accessible.onPressAction: root.shell.activateDashboard(modelData.key)
                }
            }
            Row {
                visible: !trophies.count; spacing: 13*root.unit; y: 5
                Repeater {
                    model: 3
                    Rectangle {
                        width: 54*root.unit; height: width; radius: width/2; color: "#80244844"; border.color: "#9eae89"; border.width: 2
                        Rectangle { anchors.fill: parent; anchors.margins: 5; radius: width/2; color: "transparent"; border.color: "#7d916f" }
                        Text { anchors.centerIn: parent; text: "☆"; color: "#b9c6a2"; font.pixelSize: 29*root.unit }
                    }
                }
            }
        }
        Text {
            y: 113*root.unit; width: parent.width
            text: {
                const rows = root.flow.achievements
                let chosen = rows.length ? rows[0] : null
                for (let i=0; i<rows.length; ++i) if(rows[i].key===root.flow.focus) chosen=rows[i]
                return chosen ? chosen.name + (chosen.offline ? " · Offline" : "") : "Your first achievements await"
            }
            color: "#e4ebd1"; font.pixelSize: 12*root.unit; elide: Text.ElideRight
        }
    }
    // The adapter owns the showcase; the shell keeps a single compact launch control.
    Rectangle {
        id: showcase
        x: root.split + 2; y: 7 * root.unit
        width: parent.width-x-12*root.unit; height: parent.height-y-13*root.unit
        radius: 18 * root.unit; color: "#b5204c47"; border.color: "#c4d7a9"; border.width: 2
        Image {
            id: gameBackdrop; objectName: "home-exit-picture"
            anchors.fill: parent; anchors.margins: 3
            source: root.shell.home.exitPreview || root.shell.homeGame.preview || ""
            asynchronous: true; cache: true; fillMode: Image.PreserveAspectCrop; opacity: .25
        }
        Rectangle { anchors.fill: parent; anchors.margins: 4; radius: parent.radius-4; color: "transparent"; border.color: "#507b9b83" }
        Item {
            anchors { fill: parent; margins: 14 * root.unit; bottomMargin: 107 * root.unit }
            clip: true; enabled: false
            ExperienceHost { anchors.fill: parent; shell: root.shell; showcase: true }
        }
        AdventureButton {
            id: play
            anchors { right: parent.right; bottom: parent.bottom; rightMargin: 16*root.unit; bottomMargin: 10*root.unit }
            width: 96*root.unit; height: width
            shell: root.shell; selected: root.active && root.flow.focus === "play"
        }
        Text {
            anchors { left: parent.left; leftMargin: 18*root.unit; right: play.left; rightMargin: 9*root.unit; verticalCenter: play.verticalCenter }
            text: root.shell.liveGame.id ? "Return to game" : root.shell.homeGame.id ? root.shell.home.action : "Choose your\nfirst game"
            color: "#fff4ce"; font.family: Theme.displayFamily; font.pixelSize: 21*root.unit; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
        }
    }
}
