import QtQuick

// Plain app icons in the shared chassis drawer; no keycaps or wells.
Item {
    id: root
    objectName: "apps-chassis-dock"
    required property var flow
    clip: true
    readonly property var favorites: flow.rail.filter(row => row.railPinned)
    readonly property var running: flow.rail.filter(row => !row.railPinned)
    readonly property real sectionGap: favorites.length && running.length ? 20 : 0
    Item {
        id: row
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(root.width,(root.favorites.length+root.running.length)*62+root.sectionGap)
        height: 54
        ListView {
            id: pins; objectName: "quick-apps-favorites"
            width: root.favorites.length*62; height: 54
            model: root.favorites; orientation: ListView.Horizontal
            interactive: false; clip: true
            delegate: AppIcon {}
        }
        Item {
            x: pins.width; width: root.sectionGap; height: 54
            visible: root.sectionGap>0
            Rectangle { x: 8; y: 7; width: 2; height: 36; radius: 1; color: Theme.chassisDark }
            Rectangle { x: 10; y: 7; width: 1; height: 36; radius: .5; color: Theme.rim; opacity: .65 }
        }
        ListView {
            id: runningList; objectName: "quick-apps-list"
            x: pins.width+root.sectionGap; width: Math.max(0,parent.width-x); height: 54
            model: root.running; orientation: ListView.Horizontal
            interactive: contentWidth>width; clip: true
            currentIndex: root.flow.railFocused && root.flow.focusIndex>=root.favorites.length ? root.flow.focusIndex-root.favorites.length : -1
            function reveal() { if(currentIndex>=0)positionViewAtIndex(currentIndex,ListView.Contain) }
            onCurrentIndexChanged: Qt.callLater(reveal)
            onWidthChanged: Qt.callLater(reveal)
            delegate: AppIcon { railIndex: index+root.favorites.length }
        }
    }
    component AppIcon: Item {
            id: slot
            required property var modelData
            required property int index
            property int railIndex: index
            objectName: "quick-app-slot-"+railIndex
            width: 62; height: 54
            readonly property bool selected: root.flow.railFocused && root.flow.focusIndex===railIndex && root.enabled
            function claimFocus() { if(selected && visible)forceActiveFocus(Qt.OtherFocusReason) }
            onSelectedChanged: Qt.callLater(claimFocus)
            onVisibleChanged: Qt.callLater(claimFocus)
            Component.onCompleted: Qt.callLater(claimFocus)
            Accessible.role: Accessible.Button; Accessible.name: modelData.name
            Accessible.onPressAction: root.flow.open(railIndex, true)
            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                y: pointer.pressed ? 4 : 1; width: 44; height: 44
                fillMode: Image.PreserveAspectFit
                source: "image://desktop-app/" + encodeURIComponent(modelData.icon || "application-x-executable")
                sourceSize: Qt.size(88,88)
                opacity: slot.selected ? 1 : .85
                Behavior on y { NumberAnimation { duration: Theme.motion(65) } }
            }
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                y: 49; width: 30; height: 3; radius: 1.5
                visible: slot.selected; color: Theme.yellow
            }
            Rectangle {
                x: parent.width-10; y: 4; width: 4; height: 4; radius: 2
                visible: modelData.running; color: "#b6ef9d"
            }
            MouseArea {
                id: pointer; anchors.fill: parent
                onClicked: root.flow.open(railIndex, true)
                onPressAndHold: root.flow.options(railIndex, true)
            }
        }
    Column {
        anchors.centerIn: parent; spacing: 7; visible: !root.favorites.length && !root.running.length
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Quick Apps"; color: "#fff4ce"; font.family: Theme.displayFamily; font.pixelSize: 20 }
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Pin favourites in Apps"; color: "#e2eee6"; font.pixelSize: 13 }
    }
}
