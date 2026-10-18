import QtQuick

Item {
    id: root
    required property var flow
    property bool active: false
    enabled: active
    onActiveChanged: if(!active) flow.leave()
    Binding { target: root.flow; property: "reducedMotion"; value: Theme.reducedMotion }
    readonly property real gutter: 27
    readonly property real sideMargin: 27
    readonly property real innerWidth: width-2*sideMargin
    readonly property real headerHeight: 37
    readonly property real usableHeight: height-headerHeight-22
    readonly property real featuredHeight: usableHeight*0.54
    Item {
        visible: !flow.opened
        x: root.sideMargin; y: 3; width: root.innerWidth; height: root.height-3
        Text { x: 6; y: 0; text: "Crates"; font.family: Theme.displayFamily; font.pixelSize: 27; color: Theme.ink }
        Text { anchors.right: parent.right; anchors.rightMargin: 7; y: 9; text: root.flow.crates.length+" crates"; color: Theme.muted; font.pixelSize: 13 }
        Repeater {
            model: root.flow.crates
            CrateTile {
                required property int index
                required property var modelData
                entry: modelData; featured: index<2
                x: index<2 ? index*(width+root.gutter) : (index-2)*(width+root.gutter)
                y: index<2 ? root.headerHeight : root.headerHeight+root.featuredHeight+root.gutter
                width: index<2 ? (root.innerWidth-root.gutter)/2 : (root.innerWidth-3*root.gutter)/4
                height: index<2 ? root.featuredHeight : root.usableHeight-root.featuredHeight-root.gutter
                selected: root.active && root.flow.focusIndex===index
                opening: root.flow.opening && root.flow.focusIndex===index
                onActivated: root.flow.open(index,Theme.reducedMotion)
            }
        }
    }
    Item {
        anchors.fill: parent; visible: root.flow.opened
        Text { x: 6; y: 4; text: "‹   Crates   /   "+(root.flow.current.title || ""); font.pixelSize: 19; color: Theme.ink }
        MouseArea { x: 0; y: 0; width: 112; height: 36; onClicked: root.flow.back(); cursorShape: Qt.PointingHandCursor }
        // Honest authored empty state: no fictional projects, ratings or installs.
        Item {
            anchors.centerIn: parent; width: Math.min(parent.width-44,570); height: Math.min(parent.height-65,225)
            CrateTile { x: 0; y: 9; width: parent.width*0.43; height: parent.height-18; entry: root.flow.current; enabled: false }
            Column {
                x: parent.width*0.48; width: parent.width-x; anchors.verticalCenter: parent.verticalCenter; spacing: 13
                Text { width: parent.width; text: "A little room for discovery"; font.family: Theme.displayFamily; font.pixelSize: 26; wrapMode: Text.WordWrap; color: Theme.ink }
                Text { width: parent.width; text: "This crate is waiting for its first projects."; font.pixelSize: 17; wrapMode: Text.WordWrap; color: Theme.muted }
                Text { width: parent.width; text: root.flow.current.subtitle || ""; font.pixelSize: 14; wrapMode: Text.WordWrap; color: Theme.muted }
            }
        }
    }
}
