import QtQuick

// Recessed square app keys inside the frame-mounted Quick Apps panel.
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
            // A shallow, theme-tinted seat keeps the key integrated with the casing.
            Rectangle { anchors.horizontalCenter: parent.horizontalCenter; y: 2; width: 50; height: 50; radius: 13; color: Theme.rim; opacity: .5 }
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter; y: 0; width: 50; height: 50; radius: 13
                gradient: Gradient {
                    GradientStop { position: 0; color: Theme.screenContact }
                    GradientStop { position: 1; color: Theme.chassis }
                }
            }
            Rectangle { anchors.horizontalCenter: parent.horizontalCenter; y: 4; width: 46; height: 46; radius: 11; color: Theme.chassisDark }
            Rectangle {
                id: key
                anchors.horizontalCenter: parent.horizontalCenter
                y: pointer.pressed ? 4 : 1; width: 46; height: 46; radius: 11
                gradient: Gradient {
                    GradientStop { position: 0; color: dock.currentIndex === index ? "#fff1ae" : Qt.lighter(Theme.chassisCrown, 1.22) }
                    GradientStop { position: .52; color: dock.currentIndex === index ? Theme.yellow : Theme.chassisCrown }
                    GradientStop { position: 1; color: dock.currentIndex === index ? "#d4ad57" : Theme.chassisTop }
                }
                border.color: dock.currentIndex === index ? Theme.focusGlow : Theme.rim
                border.width: 1
                Behavior on y { NumberAnimation { duration: Theme.motion(65) } }
                Rectangle { x: 10; y: 2; width: parent.width - 20; height: 1; color: "#baffffff" }
                Image {
                    anchors.centerIn: parent; width: 32; height: 32; fillMode: Image.PreserveAspectFit
                    source: "image://desktop-app/" + encodeURIComponent(modelData.icon || "application-x-executable")
                    sourceSize.width: 64; sourceSize.height: 64
                }
            }
            Rectangle {
                x: (parent.width - 8) / 2; y: 50; width: 8; height: 2; radius: 1
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
