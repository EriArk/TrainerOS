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
        Text { anchors.right: parent.right; anchors.rightMargin: 7; y: 9; visible: root.flow.updateCount===0; text: root.flow.syncStatus || root.flow.crates.length+" crates"; color: Theme.muted; font.pixelSize: 13 }
        CapButton { anchors.right: parent.right; y: 0; width: 172; height: 28; visible: root.flow.updateCount>0
            label: "Update all · "+root.flow.updateCount; textSize: 13; claimsFocus: false; tint: Theme.green; onActivated: root.flow.updateAll() }
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
    Loader {
        anchors.fill: parent; active: root.visible && root.flow.opened
        sourceComponent: CratesCataloguePage { flow: root.flow; active: root.active }
    }
    Rectangle {
        anchors.fill: parent; visible: root.flow.updatePrompt; color: "#55000000"; z: 5
        MouseArea { anchors.fill: parent }
        Rectangle {
            anchors.centerIn: parent; width: Math.min(380,parent.width-30); height: 160
            color: Theme.paper; radius: 14; border.color: Theme.rim; border.width: 2
            Text { x: 20; y: 16; text: "Update all?"; font.family: Theme.displayFamily; font.pixelSize: 23; color: Theme.ink }
            Text { x: 20; y: 49; width: parent.width-40; text: "Some updates have not been verified for this device."; wrapMode: Text.WordWrap; font.pixelSize: 15; color: Theme.muted }
            Row { x: 20; anchors.bottom: parent.bottom; anchors.bottomMargin: 16; spacing: 12
                CapButton { width: 152; height: 34; label: "Cancel"; tint: Theme.blue; centered: true; selected: root.flow.updateChoice===0; onActivated: root.flow.confirmUpdates(false) }
                CapButton { width: 152; height: 34; label: "Update anyway"; tint: Theme.green; centered: true; selected: root.flow.updateChoice===1; onActivated: root.flow.confirmUpdates(true) }
            }
        }
    }
}
