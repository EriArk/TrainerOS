import QtQuick

// Actual app icons stand above a shared perspective shelf; no per-icon wells.
Item {
    id: root
    objectName: "apps-chassis-dock"
    required property var flow
    property int hoveredIndex: -1
    ListView {
        id: dock; objectName: "quick-apps-list"
        anchors.fill: parent
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
            width: 72; height: 126
            z: selected ? 2 : 1
            readonly property bool selected: dock.currentIndex===index && root.enabled
            readonly property string iconSource: "image://desktop-app/" + encodeURIComponent(modelData.icon || "application-x-executable")
            function claimFocus() { if(selected && visible)forceActiveFocus(Qt.OtherFocusReason) }
            onSelectedChanged: Qt.callLater(claimFocus)
            onVisibleChanged: Qt.callLater(claimFocus)
            Component.onCompleted: Qt.callLater(claimFocus)
            Accessible.role: Accessible.Button; Accessible.name: modelData.name
            Accessible.onPressAction: root.flow.open(index,true)
            Rectangle {
                x: 10; y: 87; width: 52; height: 9; radius: 5
                color: slot.selected ? "#70ffe37a" : "#28101e1d"
            }
            Image {
                x: 7; y: pointer.pressed ? 29 : slot.selected ? 23 : 27
                width: 58; height: 58; fillMode: Image.PreserveAspectFit
                source: slot.iconSource; sourceSize: Qt.size(96,96)
                Behavior on y { NumberAnimation { duration: Theme.motion(100); easing.type: Easing.OutCubic } }
            }
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                y: 105; width: 6; height: 4; radius: 2
                visible: modelData.running
                color: "#e5fffdf3"; border.color: Theme.chassisDark; border.width: .5
            }
            MouseArea {
                id: pointer; anchors.fill: parent; hoverEnabled: true
                onEntered: root.hoveredIndex=index
                onExited: if(root.hoveredIndex===index)root.hoveredIndex=-1
                onClicked: root.flow.open(index,true)
                onPressAndHold: root.flow.options(index,true)
            }
        }
    }
    Rectangle {
        id: caption
        readonly property int index: dock.currentIndex>=0 ? dock.currentIndex : root.hoveredIndex
        readonly property var entry: root.flow.rail[index] || ({})
        width: Math.min(root.width,captionText.implicitWidth+16); height: 22; radius: 6
        x: Math.max(0,Math.min(root.width-width,index*72-dock.contentX+36-width/2)); y: 0
        visible: index>=0 && dock.count>0
        color: "#e6193f3b"; border.color: "#60fffef9"; border.width: .5
        Text { id: captionText; anchors.centerIn: parent; width: Math.min(implicitWidth,parent.width-12); text: caption.entry.name || ""; color: Theme.paper; font.pixelSize: 12; elide: Text.ElideRight }
    }
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter; y: 0; width: 244; height: 51; radius: 9
        visible: dock.count===0; color: "#e6193f3b"; border.color: "#70fffef9"
        Column {
            anchors.centerIn: parent; spacing: 3
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Quick Apps"; color: Theme.paper; font.family: Theme.displayFamily; font.pixelSize: 18 }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Pin favourites in Apps"; color: Theme.paper; font.pixelSize: 12 }
        }
    }
}
