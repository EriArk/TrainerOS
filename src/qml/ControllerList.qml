import QtQuick

// A frame-mounted page supplies its background. This component owns only the
// bounded viewport, focused row and reveal behavior shared by controller lists.
ListView {
    id: root
    property bool takesFocus: false
    property color tint: Theme.green
    property string namePrefix: "row-"
    signal activated(int row)
    interactive: false; keyNavigationEnabled: false; clip: true
    boundsBehavior: Flickable.StopAtBounds
    highlightMoveDuration: 0; cacheBuffer: height
    function revealCurrent() {
        if (count > 0 && visible) positionViewAtIndex(currentIndex, ListView.Contain);
        if (takesFocus && currentItem) currentItem.control.forceActiveFocus(Qt.OtherFocusReason);
    }
    onCurrentIndexChanged: Qt.callLater(revealCurrent)
    onCurrentItemChanged: Qt.callLater(revealCurrent)
    onModelChanged: Qt.callLater(revealCurrent)
    onTakesFocusChanged: Qt.callLater(revealCurrent)
    onVisibleChanged: Qt.callLater(revealCurrent)
    delegate: Item {
        required property int index
        required property var modelData
        property alias control: button
        width: root.width; height: 72
        CapButton {
            id: button
            objectName: root.namePrefix + modelData.id
            x: 5; y: 5; width: parent.width - 10; height: 58
            label: modelData.title; detail: modelData.subtitle; tint: root.tint
            selected: root.takesFocus && root.currentIndex === index
            onActivated: root.activated(index)
        }
    }
}
