import QtQuick

Item {
    id: root
    required property var clock
    property bool takesFocus: true
    property bool compact: false
    property string externalError: ""
    Text { x: 8; y: 4; text: root.clock.mode==="main" ? root.clock.time : root.clock.subtitle; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: root.clock.mode==="main" ? (root.compact ? 34 : 44) : 28 }
    Text { x: root.compact ? 125 : 162; y: 14; width: parent.width-x-8; text: root.clock.mode==="main" ? root.clock.date : ""; color: Theme.muted; font.pixelSize: 14; wrapMode: Text.WordWrap }
    Text { x: 8; y: root.compact ? 45 : 56; width: parent.width-16; text: root.clock.mode==="main" ? root.clock.subtitle : ""; color: Theme.muted; font.pixelSize: 14; elide: Text.ElideRight }
    ListView {
        id: choices
        objectName: "clock-choices"
        x: 2; y: root.clock.mode==="main" ? (root.compact ? 65 : 87) : 45
        width: parent.width-4; height: parent.height-y-40
        clip: true; spacing: root.compact ? 5 : 7; model: root.clock.rows
        currentIndex: root.clock.focusIndex
        onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
        onModelChanged: Qt.callLater(function(){ choices.positionViewAtIndex(choices.currentIndex,ListView.Contain) })
        delegate: Rectangle {
            required property int index; required property var modelData
            width: choices.width; height: root.clock.mode==="manual" ? (root.compact ? 26 : 39) : root.compact ? 36 : 57; radius: 9
            objectName: "clock-row-"+index
            readonly property bool selected: root.visible && root.takesFocus && root.clock.focusIndex===index
            onSelectedChanged: if(selected) forceActiveFocus(Qt.OtherFocusReason)
            onVisibleChanged: if(selected && visible) forceActiveFocus(Qt.OtherFocusReason)
            Component.onCompleted: if(selected) forceActiveFocus(Qt.OtherFocusReason)
            color: !modelData.enabled ? "#dce3d9" : selected ? "#f4df99" : index%2 ? "#c6dfdf" : "#e5ead5"
            border.width: selected ? 3 : 1; border.color: selected ? Theme.focus : "#a5b8a7"
            Text { x: 14; anchors.verticalCenter: parent.verticalCenter; width: parent.width*.54-15; text: modelData.label; font.family: Theme.displayFamily; font.pixelSize: root.compact ? 16 : 18; color: Theme.ink; elide: Text.ElideRight }
            Text { x: parent.width*.54; width: parent.width-x-14; anchors.verticalCenter: parent.verticalCenter; text: modelData.detail; font.pixelSize: root.compact ? 12 : 14; horizontalAlignment: Text.AlignRight; color: Theme.muted; elide: Text.ElideRight }
            TapHandler { enabled: !root.clock.busy; onTapped: root.clock.activate(index) }
        }
    }
    Text { x: 8; anchors.bottom: parent.bottom; width: parent.width-16; height: 34; text: root.clock.busy ? "Updating…" : root.externalError || root.clock.error; color: "#853b24"; font.pixelSize: 13; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
}
