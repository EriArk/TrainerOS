import QtQuick

Item {
    id: root
    required property var flow
    property bool takesFocus: false
    Text { id: heading; x: 4; y: 12; text: root.flow.choosingAvatar ? "Choose your picture" : "Communication"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 26 }
    Image { anchors.right: parent.right; y: 9; width: 42; height: 42; source: root.flow.profile.avatar || ""; asynchronous: true; fillMode: Image.PreserveAspectFit; visible: source.toString().length>0 }
    ListView {
        id: list; x: 0; y: 55; width: parent.width; height: parent.height-y-46; clip: true
        model: root.flow.rows; spacing: 7; currentIndex: root.flow.focusIndex
        highlightMoveDuration: 100
        onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
        onCountChanged: positionViewAtIndex(currentIndex,ListView.Contain)
        delegate: Item {
            required property int index; required property var modelData
            width: list.width-5; height: control.height+(modelData.section ? 23 : 0)
            Text { x: 5; y: 1; text: modelData.section || ""; font.pixelSize: 13; font.bold: true; color: Theme.muted }
            SettingControl {
                id: control; y: modelData.section ? 23 : 0; width: parent.width; height: modelData.kind==="volume"?68:58
                objectName: "communication-control-"+index
                title: modelData.title; textInset: modelData.kind==="picture" ? 68 : 13
                detail: modelData.detail; kind: modelData.kind==="choice" || modelData.kind==="picture" ? "action" : modelData.kind
                checked: modelData.checked || false; level: modelData.level === undefined ? -1 : modelData.level
                selected: root.takesFocus && root.flow.focusIndex===index
                onActivated: root.flow.activate(index)
                onLevelRequested: value => root.flow.setVolume(value)
            }
            Image { x: 10; y: control.y+5; width: 48; height: 48; visible: modelData.kind==="picture"; source: modelData.image || ""; asynchronous: true; sourceSize.width: 96; sourceSize.height: 96; fillMode: Image.PreserveAspectFit }
        }
    }
    Rectangle {
        x: 5; y: parent.height-40; width: root.flow.testing ? parent.width*0.32 : 0; height: 13; radius: 6; color: "#9ebaa9"; visible: root.flow.testing
        Rectangle { width: parent.width*root.flow.microphoneLevel/100; height: parent.height; radius: 6; color: "#6bab72" }
    }
    Text { x: root.flow.testing ? parent.width*0.35 : 5; y: parent.height-39; width: parent.width-x-5; height: 36; text: root.flow.status; textFormat: Text.PlainText; font.pixelSize: 13; color: Theme.muted; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
}
