import QtQuick
Item {
    id: root
    required property var shell
    readonly property var hall: shell.hall
    readonly property bool active: visible && !hall.account.open && !shell.keyboard.open && !shell.menuOpen
    objectName: "achievement-history"
    Rectangle {anchors.fill: parent; color: Theme.surfaceColor("#650d2525"); radius: 10}
    MouseArea {anchors.fill: parent; enabled: root.active; onClicked: root.hall.closeHistory()}
    Rectangle {
        id: popup; objectName: "achievement-history-popup"; anchors.centerIn: parent
        width: Math.min(550,parent.width-56); height: Math.min(root.hall.historyRows.length ? 330 : 210,parent.height-30)
        radius: 18; color: Theme.paper; border.width: 3; border.color: Theme.rim
        MouseArea {anchors.fill: parent; onClicked: {}}
        Text {x: 23; y: 16; text: "Account history"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 22}
        Text {
            anchors.right: parent.right; anchors.rightMargin: 21; y: 14; text: "\u00d7"; font.pixelSize: 26; color: Theme.ink
            MouseArea {anchors.fill: parent; anchors.margins: -8; enabled: root.active; onClicked: root.hall.closeHistory()}
        }
        Text {x: 23; y: 45; width: parent.width-46; height: 37; text: root.hall.historyStatus; wrapMode: Text.WordWrap; font.pixelSize: 11; color: Theme.muted}
        ListView {
            id: history; objectName: "achievement-history-list"
            x: 23; y: 86; width: parent.width-46; height: parent.height-y-53; clip: true
            model: root.visible?root.hall.historyRows:[]; currentIndex: root.hall.historyFocus
            boundsBehavior: Flickable.StopAtBounds; interactive: root.active; cacheBuffer: 0
            onCurrentIndexChanged: if(currentIndex<count)positionViewAtIndex(currentIndex,ListView.Contain)
            delegate: Item {
                required property var modelData
                required property int index
                width: history.width; height: 52
                Rectangle {anchors.fill: parent; anchors.margins: 1; radius: 7; color: Qt.alpha(Theme.rim,.07); visible: root.active && index===history.currentIndex}
                Text {x: 8; y: 4; width: parent.width-124; text: modelData.title; textFormat: Text.PlainText; font.family: Theme.displayFamily; font.pixelSize: 15; elide: Text.ElideRight; color: Theme.ink}
                Text {x: 8; y: 28; width: parent.width-124; text: modelData.platform+(modelData.date?" \u00b7 "+modelData.date:""); font.pixelSize: 10; elide: Text.ElideRight; color: Theme.muted}
                Text {anchors.right: parent.right; anchors.rightMargin: 8; y: 6; text: modelData.earned+" / "+modelData.total; font.pixelSize: 15; color: Theme.ink}
                Text {anchors.right: parent.right; anchors.rightMargin: 8; y: 29; text: modelData.hardcore?modelData.hardcore+" hardcore":""; font.pixelSize: 10; color: Theme.muted}
                Rectangle {x: 8; y: parent.height-1; width: parent.width-16; height: 1; color: Qt.alpha(Theme.rim,.16)}
            }
        }
        Rectangle {
            x: parent.width-15; y: history.y; width: 3; height: history.height; radius: 1.5
            color: Qt.alpha(Theme.rim,.18); visible: history.contentHeight>history.height
            Rectangle {width: 3; radius: 1.5; color: Theme.rim
                height: Math.max(16,parent.height*history.height/Math.max(1,history.contentHeight))
                y: Math.max(0,Math.min(parent.height-height,(parent.height-height)*history.contentY/Math.max(1,history.contentHeight-history.height)))}
        }
        Text {anchors.centerIn: history; visible: !history.count; text: root.hall.historyBusy?"Loading...":"Your account history will appear here"; width: history.width-30; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; font.pixelSize: 14; color: Theme.muted}
        Row {
            x: 23; anchors.bottom: parent.bottom; anchors.bottomMargin: 14; spacing: 9
            CapButton {width: 116; height: 30; label: !root.hall.historyConfigured?"Connect":root.hall.historyBusy?"Loading...":root.hall.historyMore?"Load more":"Refresh"; tint: Theme.blue; enabled: root.active && !root.hall.historyBusy; selected: root.active && history.currentIndex>=history.count; onActivated: root.hall.historyConfigured ? root.hall.loadHistory(root.hall.historyMore) : root.hall.openAchievementAccount()}
            CapButton {width: 116; height: 30; label: "Account"; visible: root.hall.historyConfigured; tint: Theme.green; enabled: root.active; onActivated: root.hall.openAchievementAccount()}
        }
    }
}
