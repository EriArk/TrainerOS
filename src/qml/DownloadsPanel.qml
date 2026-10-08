import QtQuick

Item {
    id: root
    required property var flow
    Rectangle { anchors.fill: parent; color: "#77152d2b" }
    MouseArea { anchors.fill: parent }
    Rectangle {
        anchors.centerIn: parent; width: Math.min(800,parent.width-32); height: Math.min(366,parent.height-24,Math.max(245,152+root.flow.tasks.length*70))
        radius: 16; color: "#e8edda"; border.color: "#35544f"; border.width: 3
        Image { x: 18; y: 18; width: 25; height: 25; source: "qrc:/social-icons/arrow-down.svg" }
        Text { x: 51; y: 12; text: "Downloads"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 26; font.bold: true }
        Text { x: 18; y: 45; text: root.flow.summary; color: Theme.muted; font.pixelSize: 14 }
        CapButton { x: parent.width-96; y: 15; width: 78; height: 30; label: "Close"; textSize: 14; onActivated: root.flow.close() }
        Row {
            x: 18; y: 73; spacing: 8
            Repeater {
                model: [{id:"pause-all",label:"X  Pause all"},{id:"resume-all",label:"Y  Resume all"},{id:"cancel-all",label:"Select  Cancel all"}]
                CapButton { required property var modelData; width: 137; height: 29; label: modelData.label; textSize: 13; tint: modelData.id==="cancel-all"?Theme.pink:Theme.blue; onActivated: root.flow.controlAll(modelData.id) }
            }
        }
        ListView {
            id: list; x: 18; y: 115; width: parent.width-236; height: parent.height-y-38; clip: true; spacing: 5
            model: root.flow.tasks; currentIndex: root.flow.focusIndex; keyNavigationEnabled: false
            section.property: "section"
            section.delegate: Item {
                required property string section
                width: list.width; height: section.length ? 28 : 0
                Rectangle { anchors.verticalCenter: parent.verticalCenter; width: parent.width; height: 1; color: "#91ac9e"; visible: section.length>0 }
                Text { anchors.verticalCenter: parent.verticalCenter; leftPadding: 8; rightPadding: 8; text: section; color: Theme.muted; font.pixelSize: 13; font.bold: true
                    Rectangle { anchors.fill: parent; color: "#e8edda"; z: -1 }
                }
            }
            Rectangle { anchors.right: parent.right; width: 3; height: parent.height; color: "#b8cbbf"; visible: list.contentHeight>list.height; z: 2
                Rectangle { width: parent.width; height: Math.max(16,list.visibleArea.heightRatio*parent.height); y: list.visibleArea.yPosition*parent.height; color: Theme.muted; radius: 2 }
            }
            onCurrentIndexChanged: Qt.callLater(function(){list.positionViewAtIndex(list.currentIndex,ListView.Contain)})
            delegate: Rectangle {
                required property int index; required property var modelData
                width: list.width; height: 65; radius: 9
                color: index===list.currentIndex?"#c5ded4":"#dde5d8"
                border.width: index===list.currentIndex?2:1; border.color: index===list.currentIndex?Theme.ink:"#b4c5b8"
                Rectangle {
                    x: 7; y: 6; width: 56; height: 53; radius: 5; color: "#f7f3e5"
                    Image { id: thumbnail; anchors.fill: parent; anchors.margins: 2; source: modelData.picture || ""; asynchronous: true; sourceSize: Qt.size(112,106); fillMode: Image.PreserveAspectFit }
                    PlatformBadge { anchors.centerIn: parent; scale: 0.7; label: modelData.platform || ""; shape: modelData.shape || "console"; visible: thumbnail.status!==Image.Ready }
                }
                Text { x: 73; y: 7; width: parent.width-153; text: modelData.title; textFormat: Text.PlainText; elide: Text.ElideRight; color: Theme.ink; font.pixelSize: 16; font.bold: true }
                PlatformBadge { x: parent.width-70; y: 2; scale: 0.72; transformOrigin: Item.TopLeft; label: modelData.platform || ""; shape: modelData.shape || "console" }
                Rectangle { x: 74; y: 38; width: 7; height: 7; radius: 4; color: modelData.state==="failed"?"#bd655f":modelData.state==="done"?"#559469":modelData.state==="running"?"#4e8fb5":modelData.state==="paused"||modelData.state==="attention"?"#bc923c":"#90a399" }
                Text { x: 87; y: 31; width: parent.width-100; text: modelData.detail; textFormat: Text.PlainText; elide: Text.ElideRight; color: Theme.muted; font.pixelSize: 13 }
                Rectangle { x: 74; y: 54; width: parent.width-88; height: 3; radius: 2; color: "#b8cbbf"
                    Rectangle { height: parent.height; width: modelData.state==="done"?parent.width: modelData.state==="running"?parent.width*0.2:0; radius: 2; color: modelData.state==="done"?"#559469":"#4e8fb5"
                        SequentialAnimation on opacity { running: modelData.state==="running"; loops: Animation.Infinite; NumberAnimation { to: 0.35; duration: 650 } NumberAnimation { to: 1; duration: 650 } }
                        NumberAnimation on x { running: modelData.state==="running"; from: 0; to: parent.width-width; duration: 1700; loops: Animation.Infinite }
                    }
                }
                TapHandler { onTapped: root.flow.select(index) }
            }
            Text { anchors.centerIn: parent; width: parent.width-20; visible: list.count===0; text: "Your downloads will appear here."; horizontalAlignment: Text.AlignHCenter; color: Theme.muted; font.pixelSize: 16 }
        }
        ListView {
            id: commands; x: parent.width-202; y: 115; width: 184; height: parent.height-y-38; spacing: 6; clip: true
            model: root.flow.actions; currentIndex: root.flow.actionIndex
            onCurrentIndexChanged: Qt.callLater(function(){commands.positionViewAtIndex(commands.currentIndex,ListView.Contain)})
            delegate: CapButton {
                required property int index; required property var modelData
                width: commands.width; height: 32; label: (modelData.id==="earlier"?"↑  ":modelData.id==="later"?"↓  ":"")+modelData.label; textSize: 14
                selected: root.flow.actionsFocused && commands.currentIndex===index
                tint: modelData.id==="cancel"?Theme.pink:Theme.green
                onActivated: root.flow.activate(index)
            }
        }
        Text { x: 18; y: parent.height-27; width: parent.width-36; text: (root.flow.tasks[root.flow.focusIndex] ? root.flow.tasks[root.flow.focusIndex].source+" · " : "")+"Closing this window keeps the queue running."; color: Theme.muted; font.pixelSize: 13 }
    }
}
