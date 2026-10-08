import QtQuick

Item {
    id: root
    required property var shell
    readonly property var flow: shell.scraper
    readonly property bool takesFocus: visible && !shell.keyboard.open
    Rectangle { anchors.fill: parent; color: "#770d2525" }
    MouseArea { anchors.fill: parent }
    Rectangle {
        visible: root.flow.selectingSystems
        anchors.centerIn: parent; width: Math.min(680,parent.width-40); height: Math.min(414,parent.height-30)
        radius: 18; color: "#e8edda"; border.color: "#35544f"; border.width: 3
        Text { x: 22; y: 14; text: "ScreenScraper"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 26; font.bold: true }
        Text { x: 22; y: 51; width: parent.width-44; text: root.flow.detail; color: Theme.muted; font.pixelSize: 14; lineHeight: 1.25 }
        CapButton {
            x: parent.width-179; y: 17; width: 157; height: 31; textSize: 14; tint: Theme.blue
            label: root.flow.rows[0] ? root.flow.rows[0].label : "Select all"
            enabled: root.flow.rows[0] ? root.flow.rows[0].enabled : false
            selected: root.takesFocus && root.flow.focusIndex===0
            onActivated: root.shell.activate(0)
        }
        ListView {
            id: systems; x: 22; y: 101; width: parent.width-44; height: parent.height-y-91
            clip: true; spacing: 6; model: root.flow.systemRows; currentIndex: root.flow.focusIndex-1; keyNavigationEnabled: false
            onCurrentIndexChanged: if(currentIndex>=0&&currentIndex<count) Qt.callLater(function(){systems.positionViewAtIndex(systems.currentIndex,ListView.Contain)})
            delegate: Rectangle {
                required property int index; required property var modelData
                width: systems.width-8; height: 49; radius: 9
                readonly property bool focused: root.takesFocus && root.flow.focusIndex===index+1
                color: modelData.checked?"#c5ded4":"#dde5d8"; border.color: focused?Theme.ink:"#b4c5b8"; border.width: focused?2:1
                opacity: modelData.enabled?1:0.6
                Rectangle {
                    x: 12; y: 13; width: 23; height: 23; radius: 5; color: modelData.checked?Theme.green:"#f7f3e5"; border.color: "#55796c"
                    Text { anchors.centerIn: parent; text: modelData.checked?"✓":""; color: Theme.ink; font.pixelSize: 19; font.bold: true }
                }
                PlatformBadge { x: 47; y: 5; scale: 0.75; transformOrigin: Item.TopLeft; shape: modelData.shape || "console"; label: modelData.id.toUpperCase() }
                Text { x: 116; y: 6; width: parent.width-130; text: modelData.label; color: Theme.ink; elide: Text.ElideRight; font.pixelSize: 16; font.bold: true }
                Text { x: 116; y: 28; width: parent.width-130; text: modelData.count+" games"+(modelData.enabled?"":" · Unavailable on ScreenScraper"); color: Theme.muted; font.pixelSize: 12; elide: Text.ElideRight }
                TapHandler { enabled: modelData.enabled; onTapped: root.shell.activate(index+1) }
            }
            Rectangle { anchors.right: parent.right; width: 3; height: parent.height; color: "#b8cbbf"; visible: systems.contentHeight>systems.height
                Rectangle { width: parent.width; height: Math.max(16,systems.visibleArea.heightRatio*parent.height); y: systems.visibleArea.yPosition*parent.height; color: Theme.muted; radius: 2 }
            }
            Text { anchors.centerIn: parent; width: parent.width-24; visible: systems.count===0; text: "No ROM games found. Add games to your library first."; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: 16 }
        }
        Text { x: 22; y: parent.height-85; width: parent.width-44; height: 31; text: root.flow.status || "Uses your ScreenScraper download settings."; color: Theme.muted; font.pixelSize: 13; maximumLineCount: 2; elide: Text.ElideRight; wrapMode: Text.WordWrap }
        Row {
            x: 22; y: parent.height-48; spacing: 10; width: parent.width-44
            CapButton { width: parent.width-130; height: 33; textSize: 15; label: "Download · "+root.flow.selectedGameCount+" games"; tint: Theme.green; enabled: root.flow.selectedGameCount>0; selected: root.takesFocus && root.flow.focusIndex===systems.count+1; onActivated: root.shell.activate(systems.count+1) }
            CapButton { width: 120; height: 33; textSize: 15; label: "Cancel"; tint: Theme.pink; selected: root.takesFocus && root.flow.focusIndex===systems.count+2; onActivated: root.shell.activate(systems.count+2) }
        }
    }
    Rectangle {
        visible: !root.flow.selectingSystems
        anchors.centerIn: parent; width: Math.min(680,parent.width-40); height: Math.min(content.implicitHeight+36,parent.height-30)
        radius: 18; color: "#e8edda"; border.color: "#35544f"; border.width: 3
        Column {
            id: content
            x: 22; y: 18; width: parent.width-44; spacing: 10
            Text { width: parent.width; text: root.flow.title; textFormat: Text.PlainText; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 26; font.bold: true }
            Text { width: parent.width; height: 78; text: root.flow.detail; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 16; wrapMode: Text.WordWrap; maximumLineCount: 4; elide: Text.ElideRight }
            ListView {
                id: list; width: parent.width; height: Math.min(190,contentHeight); clip: true; spacing: 7
                model: root.flow.rows; currentIndex: root.flow.focusIndex; keyNavigationEnabled: false
                function reveal(){if(count)positionViewAtIndex(currentIndex,ListView.Contain)}
                onCurrentIndexChanged: Qt.callLater(reveal)
                onCountChanged: Qt.callLater(reveal)
                delegate: SettingControl {
                    required property int index; required property var modelData
                    objectName: "scraper-action-"+index; width: list.width; height: modelData.detail.length?56:40
                    title: modelData.label; detail: modelData.detail; kind: modelData.enabled?"action":"unavailable"
                    selected: root.takesFocus && index===list.currentIndex
                    onActivated: root.shell.activate(index)
                }
            }
            Text { width: parent.width; visible: text.length>0; height: Math.min(48,implicitHeight); text: root.flow.status; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 14; wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight }
        }
    }
}
