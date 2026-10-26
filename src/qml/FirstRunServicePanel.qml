import QtQuick

Item {
    id: root
    required property var shell
    required property var flow
    required property var session
    readonly property bool achievements: flow.service === "achievements"
    readonly property bool communication: ["social","communication"].includes(flow.service)
    readonly property bool scraper: flow.service === "scraper"
    readonly property bool takesFocus: visible && !shell.keyboard.open
    readonly property var account: shell.hall.account
    Rectangle { anchors.fill: parent; radius: 12; color: Theme.paper }
    CommunicationPane { anchors.fill: parent; anchors.margins: 16; flow: root.shell.settings.communication; visible: root.communication; takesFocus: root.takesFocus && visible }
    ScrapeSettingsPane { anchors.fill: parent; anchors.margins: 16; flow: root.shell.scraper; visible: root.scraper; takesFocus: root.takesFocus && visible }
    Item {
        anchors.fill: parent; anchors.margins: 18; visible: !root.communication && !root.scraper
        Text { id: title; text: root.achievements?"RetroAchievements":root.shell.settings.categories[root.shell.settings.category]; font.family: Theme.displayFamily; font.pixelSize: 26; color: Theme.ink }
        ListView {
            id: list; y: 43; width: parent.width; height: parent.height-y-55; clip: true; spacing: 8; keyNavigationEnabled: false
            model: root.achievements ? (root.account.available ? root.account.rows.map(r => ({title:r.label,detail:r.detail,kind:r.enabled?"action":"unavailable"})) : []) : root.shell.settings.controls
            currentIndex: root.achievements?root.account.focusIndex:root.shell.settings.rowFocus
            function reveal() { if(count)positionViewAtIndex(currentIndex,ListView.Contain) }
            onCurrentIndexChanged: Qt.callLater(reveal)
            onCountChanged: Qt.callLater(reveal)
            onHeightChanged: Qt.callLater(reveal)
            Rectangle {
                anchors.right: parent.right; width: 3; height: parent.height; radius: 2; z: 2
                visible: list.contentHeight>list.height; color: Theme.surfaceColor("#b5c7b8")
                Rectangle { width: parent.width; height: Math.max(16,list.visibleArea.heightRatio*parent.height); y: list.visibleArea.yPosition*parent.height; radius: 2; color: Theme.muted }
            }
            delegate: SettingControl {
                required property int index
                required property var modelData
                objectName: "setup-service-row-"+index
                width: list.width-5; height: 62; title: modelData.title; detail: modelData.detail; kind: modelData.kind
                selected: root.takesFocus && index===list.currentIndex
                checked: modelData.checked !== undefined ? modelData.checked : root.shell.settings.category===2?root.shell.settings.videoPreviews:root.shell.settings.reducedMotion
                level: kind==="volume"?root.shell.device.rows[0].level:kind==="brightness"?root.shell.device.rows[1].level:-1
                muted: kind==="volume" && root.shell.device.rows[0].muted
                onActivated: root.session.activateSetupService(index)
                onAdjustmentRequested: direction => root.shell.settings.adjustDisplay(index,direction)
                onLevelRequested: value => root.shell.device.setQuickLevel(kind==="volume"?0:1,value)
            }
        }
        Text {
            anchors.bottom: parent.bottom; width: parent.width; height: 48; font.pixelSize: 13; color: Theme.muted; wrapMode: Text.WordWrap; textFormat: Text.PlainText
            text: root.achievements ? (root.account.available?root.account.status:"Account access is unavailable in this build. You can continue setup offline.") : root.shell.settings.error || root.shell.device.error || "Changes are saved in Settings. Back returns to setup."
        }
    }
}
