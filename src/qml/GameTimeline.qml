import QtQuick

Item {
    id: root
    required property var shell
    property bool active: false
    readonly property var events: shell.gameHistory
    readonly property int total: shell.home.sessionCount || 0
    Text {
        text: "‹  Overview"; font.pixelSize: 13; color: Theme.muted
        MouseArea { anchors.fill: parent; anchors.margins: -8; enabled: root.active; onClicked: root.shell.showOverview("summary") }
    }
    Text {
        anchors.right: parent.right
        text: root.total > root.events.length ? "Latest " + root.events.length + " of " + root.total + " sessions" : root.total + (root.total === 1 ? " session" : " sessions")
        font.pixelSize: 12; color: Theme.muted
    }
    ListView {
        id: timeline; objectName: "game-timeline"
        x: 0; y: 31; width: parent.width; height: parent.height-y
        orientation: ListView.Horizontal; boundsBehavior: Flickable.StopAtBounds
        clip: true; spacing: 0; interactive: root.active
        model: root.events.slice().reverse()
        currentIndex: count ? count-1-root.shell.focusIndex : -1
        function reveal() { if(root.visible && currentIndex >= 0) positionViewAtIndex(currentIndex,ListView.Contain) }
        onCurrentIndexChanged: Qt.callLater(reveal)
        onCountChanged: Qt.callLater(reveal)
        onVisibleChanged: if(visible) Qt.callLater(reveal)
        onWidthChanged: Qt.callLater(reveal)
        delegate: Item {
            id: event
            required property var modelData
            required property int index
            width: Math.max(210,Math.min(260,timeline.width/3.6)); height: timeline.height
            readonly property bool above: index % 2 === 0
            readonly property bool focused: root.active && timeline.currentIndex === index
            readonly property color accent: modelData.outcome === "returned" ? Theme.green : modelData.outcome === "running" ? Theme.blue : Theme.pink
            readonly property real axis: height/2
            Rectangle { y: event.axis-2; width: parent.width; height: 4; radius: 2; color: Qt.alpha(Theme.rim,0.7) }
            Rectangle { x: parent.width/2-1; y: event.above ? card.y+card.height : event.axis; width: 2; height: 21; color: event.accent }
            Rectangle {
                x: parent.width/2-8; y: event.axis-8; width: 16; height: 16; radius: 8
                color: event.accent; border.width: 3; border.color: Theme.paper
            }
            Text {
                x: 10; y: event.above ? event.axis+16 : event.axis-34; width: parent.width-20
                text: modelData.date; horizontalAlignment: Text.AlignHCenter
                font.family: Theme.displayFamily; font.pixelSize: 16; color: Theme.ink
            }
            Rectangle { x: card.x+1; y: card.y+3; width: card.width; height: card.height; radius: 10; color: "#20193f3b" }
            Rectangle {
                id: card; objectName: "timeline-event-" + modelData.id
                x: 10; width: parent.width-20; height: Math.min(110,event.axis-25)
                y: event.above ? event.axis-height-20 : event.axis+20
                radius: 10; border.width: 1; border.color: Qt.alpha(Theme.rim,0.7)
                gradient: Gradient {
                    GradientStop { position: 0; color: Theme.paper }
                    GradientStop { position: 1; color: Qt.tint(Theme.paper,Qt.alpha(event.accent,0.24)) }
                }
                Rectangle { x: 12; y: 13; width: 4; height: parent.height-26; radius: 2; color: event.accent }
                Text { x: 26; y: 9; text: modelData.clock; color: Theme.muted; font.pixelSize: 12 }
                Text {
                    x: 26; y: 29; width: parent.width-36; text: modelData.duration
                    font.family: Theme.displayFamily; font.pixelSize: 19; minimumPixelSize: 13
                    fontSizeMode: Text.HorizontalFit; elide: Text.ElideRight; color: Theme.ink
                }
                Text { x: 26; y: 55; width: parent.width-36; text: modelData.status; font.pixelSize: 12; color: Theme.muted; elide: Text.ElideRight }
                Text {
                    x: 26; y: 75; width: parent.width-36
                    text: modelData.ended ? "Until " + modelData.ended : "End time unavailable"
                    font.pixelSize: 10; color: Theme.muted; elide: Text.ElideRight; visible: parent.height > 95
                }
                Rectangle {
                    anchors.fill: parent; anchors.margins: -3; radius: 13; color: "transparent"
                    border.width: 2; border.color: Theme.focusGlow; visible: event.focused
                }
                MouseArea { anchors.fill: parent; enabled: root.active; onClicked: root.shell.activate(timeline.count-1-event.index,"overview-event") }
            }
        }
        Text {
            anchors.centerIn: parent; width: parent.width*0.7; visible: !timeline.count
            text: "Every adventure begins somewhere.\nYour first session will start this timeline."
            font.family: Theme.displayFamily; font.pixelSize: 22; color: Theme.muted
            wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter
        }
    }
    Text { x: 0; y: timeline.y+timeline.height/2-14; text: "‹"; color: Theme.ink; font.pixelSize: 28; visible: !timeline.atXBeginning }
    Text { anchors.right: parent.right; y: timeline.y+timeline.height/2-14; text: "›"; color: Theme.ink; font.pixelSize: 28; visible: !timeline.atXEnd }
}
