import QtQuick

Item {
    id: root
    required property var shell
    readonly property bool takesFocus: visible && !shell.collectionManager.open && !shell.serviceOpen && !shell.menuOpen && !shell.keyboard.open && !shell.notice.length
    PageHeader { id: header; title: "Collections"; compact: true }
    CapButton {
        objectName: "new-collection"; anchors.right: parent.right; anchors.rightMargin: 20; y: 10
        width: 176; height: 34; label: "+ New collection"; textSize: 15; tint: Theme.blue
        onActivated: root.shell.manageCollection(true)
    }
    GridView {
        id: grid
        objectName: "series-grid"
        x: 14; y: header.height + 4; width: parent.width - 28; height: parent.height - y - 8
        cellWidth: width / 3; cellHeight: height / 2
        clip: true; interactive: true; keyNavigationEnabled: false
        model: root.shell.collections; currentIndex: root.shell.focusIndex
        function reveal() {
            if (!root.takesFocus || !currentItem) return
            positionViewAtIndex(currentIndex, GridView.Contain)
            currentItem.control.forceActiveFocus(Qt.OtherFocusReason)
        }
        onCurrentIndexChanged: Qt.callLater(reveal)
        onCurrentItemChanged: Qt.callLater(reveal)
        onVisibleChanged: Qt.callLater(reveal)
        Connections { target: root; function onTakesFocusChanged() { Qt.callLater(grid.reveal) } }
        delegate: Item {
            required property int index
            required property var modelData
            width: grid.cellWidth; height: grid.cellHeight
            property alias control: cap
            CapButton {
                id: cap
                objectName: "series-card-" + modelData.id
                x: 7; y: 7; width: parent.width - 14; height: parent.height - 17
                tint: modelData.colour; label: ""; deferredFocus: true
                selected: root.takesFocus && grid.currentIndex === index
                Accessible.name: modelData.name
                onActivated: root.shell.activate(index)
                Item {
                    anchors.fill: parent; anchors.margins: 5; clip: true
                    Rectangle { anchors.fill: parent; color: modelData.colour }
                    Text { anchors.centerIn: parent; text: modelData.id.startsWith("auto:") || modelData.dynamic ? "\u21bb" : "\u25a4"; color: "#fff5d4"; font.pixelSize: 68; opacity: 0.6; visible: !modelData.art }
                    Image { anchors.fill: parent; source: modelData.art; sourceSize.width: 600; fillMode: Image.PreserveAspectCrop; asynchronous: true }
                    Rectangle {
                        anchors.bottom: parent.bottom; width: parent.width; height: parent.height * 0.55
                        gradient: Gradient { GradientStop { position: 0; color: "transparent" } GradientStop { position: 1; color: "#ed10252c" } }
                    }
                    Text {
                        x: 12; anchors.bottom: parent.bottom; anchors.bottomMargin: 10; width: parent.width - 24
                        text: modelData.name; color: "#fff5d4"; font.family: Theme.displayFamily
                        font.pixelSize: 23; font.bold: true; style: Text.Outline; styleColor: "#5b17272d"
                        minimumPixelSize: 17; fontSizeMode: Text.Fit
                    }
                    Rectangle {
                        anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 8
                        width: Math.max(33,countText.implicitWidth+14); height: 23; radius: 7; color: "#ce142b32"
                        visible: true
                        Text { id: countText; anchors.centerIn: parent; text: modelData.count; color: "#fff5d4"; font.pixelSize: 13; font.bold: true }
                    }
                }
            }
            CapButton {
                objectName: "edit-collection-"+modelData.id
                visible: modelData.id.startsWith("user:")
                x: 16; y: 16; width: 38; height: 30; textSize: 20; label: "\u2026"; tint: Theme.blue
                Accessible.name: "Edit " + modelData.name
                onActivated: root.shell.editCollection(index)
            }
        }
    }
}
