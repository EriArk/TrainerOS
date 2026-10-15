import QtQuick

Item {
    id: root
    required property var shell
    readonly property var flow: shell.dashboard
    readonly property bool active: visible && shell.unobstructed && !shell.quickAppsOpen && !shell.drawerOpen && !shell.keyboard.open
    readonly property real split: width * .49
    readonly property real usableHeight: height - (shell.chooseAdventureAvailable ? Theme.adventureCutoutHeight : 0) - 5
    function hints(h) { if (!visible) return null; return flow.focus === "play" ? [h("A",shell.liveGame.id ? "Return" : shell.homeGame.id ? shell.home.actionHint : "Choose a game"),h("Select","Game actions"),h("B","Back")] : [h("A","Open"),h("B","Play focus")] }
    MountedPanel {
        id: activity
        width: root.split - 12; height: root.usableHeight * .65; color: "#e9e5d9"
        Text { x: 14; y: 9; text: "Activity"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 20 }
        Row {
            x: 13; y: 38; spacing: 7
            Repeater {
                model: root.flow.chats
                Rectangle {
                    required property var modelData
                    width: Math.min(49,(activity.width-62)/6); height: width; radius: width/2
                    color: modelData.kind === "groups" ? Theme.pink : modelData.guild ? Theme.green : Theme.blue
                    border.color: Theme.focus; border.width: root.active && root.flow.focus === modelData.key ? 3 : 0
                    Image { id: avatar; anchors.fill: parent; anchors.margins: 5; source: modelData.avatar || ""; asynchronous: true; sourceSize: Qt.size(64,64); fillMode: Image.PreserveAspectFit; visible: status === Image.Ready }
                    Text { anchors.centerIn: parent; visible: avatar.status !== Image.Ready; text: (modelData.name || "?").substring(0,1); color: Theme.ink; font.pixelSize: 21 }
                    Rectangle { anchors.right: parent.right; anchors.top: parent.top; width: 9; height: 9; radius: 5; color: Theme.yellow; visible: modelData.unread > 0 }
                    MouseArea { anchors.fill: parent; onClicked: root.shell.activateDashboard(modelData.key) }
                    Accessible.role: Accessible.Button; Accessible.name: modelData.name
                    Accessible.onPressAction: root.shell.activateDashboard(modelData.key)
                }
            }
        }
        ListView {
            id: events; objectName: "home-activity"
            x: 12; y: root.flow.chats.length ? 96 : 38; width: parent.width-24; height: parent.height-y-10
            clip: true; spacing: 5; model: root.flow.activities
            currentIndex: { for(let i=0;i<model.length;i++) if(model[i].key===root.flow.focus)return i;return -1 }
            onCurrentIndexChanged: if(currentIndex>=0)positionViewAtIndex(currentIndex,ListView.Contain)
            Connections { target: root.flow; function onScrollRequested(direction) { events.contentY=Math.max(events.originY,Math.min(events.originY+Math.max(0,events.contentHeight-events.height),events.contentY+direction*35)) } }
            delegate: CapButton {
                required property var modelData
                width: events.width; height: 48; textSize: 13; tint: Theme.blue
                label: modelData.name || "Activity"; detail: modelData.detail || ""
                selected: root.active && root.flow.focus===modelData.key; claimsFocus: false
                onActivated: root.shell.activateDashboard(modelData.key)
            }
            Text { anchors.centerIn: parent; visible: !events.count; text: "All caught up"; color: Theme.muted; font.pixelSize: 15 }
        }
    }
    MountedPanel {
        x: 0; y: activity.height+10; width: activity.width; height: root.usableHeight-y; color: "#e1e7d7"
        Text { x: 13; y: 8; text: "Recent achievements"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 18 }
        ListView {
            id: trophies; objectName: "home-achievements"
            x: 12; y: 35; width: parent.width-24; height: parent.height-y-8
            orientation: ListView.Horizontal; clip: true; spacing: 8; model: root.flow.achievements
            currentIndex: { for(let i=0;i<model.length;i++)if(model[i].key===root.flow.focus)return i;return -1 }
            onCurrentIndexChanged: if(currentIndex>=0)positionViewAtIndex(currentIndex,ListView.Contain)
            delegate: CapButton {
                required property var modelData
                width: 122; height: Math.min(62,trophies.height); textSize: 12; tint: Theme.yellow
                label: "★  " + modelData.name; detail: modelData.game + (modelData.offline ? " · Offline" : "")
                selected: root.active && root.flow.focus===modelData.key; claimsFocus: false
                onActivated: root.shell.activateDashboard(modelData.key)
            }
            Text { anchors.centerIn: parent; visible: !trophies.count; text: "No confirmed achievements yet"; color: Theme.muted; font.pixelSize: 13 }
        }
    }
    MountedPanel {
        id: showcase; x: root.split; width: parent.width-x; height: parent.height; color: "#e0e8dd"
        Item {
            anchors { fill: parent; margins: 12; bottomMargin: 92 }
            clip: true; enabled: false
            ExperienceHost { anchors.fill: parent; shell: root.shell; showcase: true }
        }
        AdventureButton {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 14 }
            height: 63; shell: root.shell
            selected: root.active && root.flow.focus === "play"
        }
    }
}
