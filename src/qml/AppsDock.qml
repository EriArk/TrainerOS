import QtQuick

// Mounted in the expanding outer chassis, beyond the screen's continuous bevel.
Item {
    id: root
    objectName: "apps-chassis-dock"
    required property var flow
    clip: true
    ListView {
        id: dock
        anchors.fill: parent; anchors.topMargin: 3; anchors.bottomMargin: 3
        model: root.flow.rail; spacing: 2; clip: true
        currentIndex: root.flow.railFocused ? root.flow.focusIndex : -1
        onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)
        delegate: Item {
            id: slot
            required property var modelData
            required property int index
            width: dock.width; height: 66
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
            MouseArea {
                id: pointer; anchors.fill: parent
                onClicked: root.flow.open(index, true)
                onPressAndHold: root.flow.options(index, true)
            }
        }
    }
}
