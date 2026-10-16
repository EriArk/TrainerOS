import QtQuick

// Plain app icons in the shared chassis drawer; no keycaps or wells.
Item {
    id: root
    objectName: "apps-chassis-dock"
    required property var flow
    clip: true
    ListView {
        id: dock; objectName: "quick-apps-list"
        width: parent.width; height: 54
        orientation: ListView.Horizontal; interactive: true
        model: root.flow.rail; spacing: 0; clip: true
        currentIndex: root.flow.railFocused ? root.flow.focusIndex : -1
        function reveal() { if(currentIndex>=0)positionViewAtIndex(currentIndex,ListView.Contain) }
        onCurrentIndexChanged: Qt.callLater(reveal)
        onWidthChanged: Qt.callLater(reveal)
        delegate: Item {
            id: slot
            required property var modelData
            required property int index
            objectName: "quick-app-slot-"+index
            width: 62; height: 54
            readonly property bool selected: dock.currentIndex===index && root.enabled
            function claimFocus() { if(selected && visible)forceActiveFocus(Qt.OtherFocusReason) }
            onSelectedChanged: Qt.callLater(claimFocus)
            onVisibleChanged: Qt.callLater(claimFocus)
            Component.onCompleted: Qt.callLater(claimFocus)
            Accessible.role: Accessible.Button; Accessible.name: modelData.name
            Accessible.onPressAction: root.flow.open(index, true)
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
                onClicked: root.flow.open(index, true)
                onPressAndHold: root.flow.options(index, true)
            }
        }
    }
    Column {
        anchors.centerIn: parent; spacing: 7; visible: dock.count===0
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Quick Apps"; color: "#fff4ce"; font.family: Theme.displayFamily; font.pixelSize: 20 }
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Pin favourites in Apps"; color: "#e2eee6"; font.pixelSize: 13 }
    }
}
