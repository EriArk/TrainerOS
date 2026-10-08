import QtQuick

Item {
    id: root
    required property var flow
    property bool takesFocus: false
    Text { id: heading; x: 5; y: 12; text: "ScreenScraper"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 25 }
    Text { id: subtitle; x: 5; y: 45; width: parent.width-10; text: "Account, downloads & game artwork"; color: Theme.muted; font.pixelSize: 14 }
    ListView {
        id: list; x: 0; y: 73; width: parent.width; height: parent.height-y-61; clip: true; spacing: 7
        model: root.flow.settingsRows; currentIndex: root.flow.settingsFocus; keyNavigationEnabled: false
        function reveal() { if(count)positionViewAtIndex(currentIndex,ListView.Contain) }
        onCurrentIndexChanged: Qt.callLater(reveal)
        Rectangle {
            anchors.right: parent.right; width: 4; height: parent.height; radius: 2
            visible: list.contentHeight>list.height; color: "#b5c7b8"; z: 2
            Rectangle { width: parent.width; height: Math.max(16,list.visibleArea.heightRatio*parent.height); y: list.visibleArea.yPosition*parent.height; radius: 2; color: Theme.muted }
        }
        delegate: SettingControl {
            required property int index; required property var modelData
            objectName: "scraper-setting-"+index; width: list.width; height: 56
            title: modelData.label; detail: modelData.detail; kind: modelData.enabled ? "action" : "unavailable"
            selected: root.takesFocus && list.currentIndex===index
            onActivated: root.flow.activateSetting(index)
        }
    }
    Text {
        x: 5; y: parent.height-53; width: parent.width-10; height: 48
        text: root.flow.busy ? "Connecting…" : root.flow.status || "Media and game information provided by ScreenScraper.fr"
        textFormat: Text.PlainText; font.pixelSize: 13; color: Theme.muted; wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight
    }
}
