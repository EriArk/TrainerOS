import QtQuick

Item {
    id: root
    required property var flow
    property bool active: false
    readonly property real margin: 27
    readonly property real gap: 16
    readonly property real cardsTop: 39
    readonly property real cardWidth: (width-margin*2-gap*2)/3
    readonly property real cardHeight: (height-cardsTop-28-gap)/2
    Text {
        x: root.margin; y: 0; height: 30; width: parent.width-180-root.margin
        text: "‹   Crates   /   "+(root.flow.current.title || ""); textFormat: Text.PlainText
        font.family: Theme.displayFamily; font.pixelSize: 22; color: Theme.ink; elide: Text.ElideRight
    }
    MouseArea { x: root.margin-5; y: 0; width: 108; height: 32; onClicked: root.flow.back(); cursorShape: Qt.PointingHandCursor }
    Text { anchors.right: parent.right; anchors.rightMargin: root.margin; y: 7
        text: root.flow.projectCount+" projects"; color: Theme.muted; font.pixelSize: 12 }
    Repeater {
        model: root.flow.projects
        CratesProjectCard {
            required property int index
            required property var modelData
            objectName: "crate-project-"+index
            entry: modelData; accent: root.flow.current.color || Theme.rim
            x: root.margin+(index%3)*(root.cardWidth+root.gap)
            y: root.cardsTop+Math.floor(index/3)*(root.cardHeight+root.gap)
            width: root.cardWidth; height: root.cardHeight
            selected: root.active && !root.flow.detailOpen && root.flow.projectIndex===modelData.index
            onActivated: root.flow.openProject(modelData.index)
        }
    }
    Row {
        visible: root.flow.pageCount>1; anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: 1; spacing: 14
        Text { text: "‹"; font.pixelSize: 23; color: Theme.ink; opacity: root.flow.page>0 ? 1 : .25
            MouseArea { anchors.fill: parent; anchors.margins: -5; enabled: root.flow.page>0; onClicked: root.flow.turnPage(-1) } }
        Text { text: (root.flow.page+1)+" / "+root.flow.pageCount; anchors.verticalCenter: parent.verticalCenter; color: Theme.muted; font.pixelSize: 12 }
        Text { text: "›"; font.pixelSize: 23; color: Theme.ink; opacity: root.flow.page+1<root.flow.pageCount ? 1 : .25
            MouseArea { anchors.fill: parent; anchors.margins: -5; enabled: root.flow.page+1<root.flow.pageCount; onClicked: root.flow.turnPage(1) } }
    }
    MouseArea {
        anchors.fill: parent; z: -1
        onWheel: function(event) { root.flow.turnPage(event.angleDelta.y<0 ? 1 : -1); event.accepted=true }
    }
    Item {
        visible: root.flow.projectCount===0; anchors.centerIn: parent
        width: Math.min(parent.width-70,570); height: Math.min(parent.height-65,225)
        CrateTile { x: 0; y: 9; width: parent.width*.43; height: parent.height-18; entry: root.flow.current; enabled: false }
        Column {
            x: parent.width*.48; width: parent.width-x; anchors.verticalCenter: parent.verticalCenter; spacing: 13
            Text { width: parent.width; text: "A little room for discovery"; font.family: Theme.displayFamily; font.pixelSize: 26; wrapMode: Text.WordWrap; color: Theme.ink }
            Text { width: parent.width; text: "This crate is waiting for its first projects."; font.pixelSize: 17; wrapMode: Text.WordWrap; color: Theme.muted }
        }
    }
}
