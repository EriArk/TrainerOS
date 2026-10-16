import QtQuick

// Compact horizontal app tray inside the shared mirrored chassis drawer.
Item {
    id: root
    objectName: "apps-chassis-dock"
    required property var flow
    clip: true
    ListView {
        id: dock; objectName: "quick-apps-list"
        anchors.fill: parent; anchors.margins: 4
        orientation: ListView.Horizontal; interactive: true
        model: root.flow.rail; spacing: 8; clip: true
        currentIndex: root.flow.railFocused ? root.flow.focusIndex : -1
        onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)
        delegate: Item {
            id: slot
            required property var modelData
            required property int index
            objectName: "quick-app-slot-"+index
            width: 94; height: 104
            readonly property bool selected: dock.currentIndex===index && root.enabled
            function claimFocus() { if(selected && visible)forceActiveFocus(Qt.OtherFocusReason) }
            onSelectedChanged: Qt.callLater(claimFocus)
            onVisibleChanged: Qt.callLater(claimFocus)
            Component.onCompleted: Qt.callLater(claimFocus)
            Accessible.role: Accessible.Button; Accessible.name: modelData.name
            Accessible.onPressAction: root.flow.open(index, true)
            // Highlight below the cut-out and a dark well provide the recessed socket.
            Rectangle { anchors.horizontalCenter: parent.horizontalCenter; y: 1; width: 64; height: 64; radius: 17; color: Theme.edgeLight; opacity: .55 }
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter; y: 0; width: 64; height: 64; radius: 17
                gradient: Gradient {
                    GradientStop { position: 0; color: "#061f23" }
                    GradientStop { position: .65; color: Theme.chassisDark }
                    GradientStop { position: 1; color: Theme.screenContact }
                }
                border.color: Theme.edgeShadow
            }
            Rectangle { anchors.horizontalCenter: parent.horizontalCenter; y: 7; width: 54; height: 54; radius: 12; color: "#072b2d" }
            Rectangle {
                id: key
                anchors.horizontalCenter: parent.horizontalCenter
                y: pointer.pressed ? 7 : 3; width: 54; height: 54; radius: 12
                gradient: Gradient {
                    GradientStop { position: 0; color: dock.currentIndex === index ? "#fff1ae" : "#d8e8df" }
                    GradientStop { position: .52; color: dock.currentIndex === index ? Theme.yellow : "#bbd2c7" }
                    GradientStop { position: 1; color: dock.currentIndex === index ? "#cca24b" : "#8fae9f" }
                }
                border.color: dock.currentIndex === index ? Theme.focusGlow : "#6e9787"
                border.width: dock.currentIndex === index ? 2 : 1
                Behavior on y { NumberAnimation { duration: Theme.motion(65) } }
                Rectangle { x: 10; y: 2; width: parent.width - 20; height: 1; color: "#baffffff" }
                Image {
                    anchors.centerIn: parent; width: 36; height: 36; fillMode: Image.PreserveAspectFit
                    source: "image://desktop-app/" + encodeURIComponent(modelData.icon || "application-x-executable")
                    sourceSize.width: 64; sourceSize.height: 64
                }
            }
            Rectangle {
                x: (parent.width - 12) / 2; y: 63; width: 12; height: 3; radius: 1.5
                color: modelData.running ? "#b6ef9d" : "#356054"
            }
            Text { x: 2; y: 73; width: parent.width-4; height: 31; text: modelData.name; color: "#e2eee6"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; maximumLineCount: 2; wrapMode: Text.WordWrap; elide: Text.ElideRight }
            MouseArea {
                id: pointer; anchors.fill: parent
                onClicked: root.flow.open(index, true)
                onPressAndHold: root.flow.options(index, true)
            }
        }
    }
    Column {
        anchors.centerIn: parent; spacing: 7; visible: dock.count===0
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Your apps, one button away"; color: "#fff4ce"; font.family: Theme.displayFamily; font.pixelSize: 20 }
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Pin favourites in Apps. Running apps appear here too."; color: "#e2eee6"; font.pixelSize: 13 }
    }
}
