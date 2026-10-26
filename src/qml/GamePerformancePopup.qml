import QtQuick

Item {
    id: root
    required property var profile
    Rectangle { anchors.fill: parent; color: "#99102022" }
    MouseArea { anchors.fill: parent }
    Rectangle {
        id: panel
        anchors.centerIn: parent
        width: Math.min(840, parent.width - 32); height: Math.min(472, parent.height - 28)
        color: Theme.surface; radius: 20; border.color: Theme.divider
        Text { x: 26; y: 20; text: "Game settings"; font.family: Theme.displayFamily; font.pixelSize: 25; color: Theme.ink }
        Text { x: 27; y: 53; width: parent.width - 90; text: root.profile.title; textFormat: Text.PlainText; elide: Text.ElideRight; font.pixelSize: 14; color: Theme.muted }
        SocialIconButton { anchors.right: parent.right; anchors.rightMargin: 15; y: 14; icon: "x"; label: "Close"; onClicked: root.profile.close() }
        Rectangle { x: 26; y: 83; width: parent.width - 52; height: 1; color: Theme.divider }
        Column {
            x: 18; y: 102; width: 168; spacing: 8
            Repeater {
                model: ["Performance", "Compatibility"]
                delegate: Rectangle {
                    required property int index
                    required property string modelData
                    width: 168; height: 42; radius: 8
                    color: root.profile.page === index ? Theme.selection : "transparent"
                    Rectangle { width: 3; height: 22; y: 10; radius: 1.5; color: Theme.accent; visible: root.profile.page === index }
                    Text { x: 14; anchors.verticalCenter: parent.verticalCenter; text: modelData; color: root.profile.page === index ? Theme.ink : Theme.muted; font.pixelSize: 16 }
                    MouseArea { anchors.fill: parent; onClicked: root.profile.selectPage(index) }
                }
            }
            Text { x: 14; topPadding: 18; width: 146; text: "PER GAME"; font.pixelSize: 10; font.letterSpacing: 1.5; color: Theme.accent }
            Text { x: 14; width: 140; text: root.profile.page === 1 ? "Compatibility changes\napply on the next launch." : root.profile.live ? "Apply power settings\nwithout restarting.\nSystem settings return\nwhen the game ends." : "Applies at launch.\nSystem settings return\nwhen the game ends."; font.pixelSize: 12; lineHeight: 1.3; color: Theme.muted }
            Text { x: 14; width: 140; text: root.profile.capabilities.current ? "Host · " + root.profile.capabilities.current : "Host controls unavailable"; wrapMode: Text.Wrap; font.pixelSize: 12; color: Theme.muted }
        }
        ListView {
            id: list
            x: 208; y: 98; width: parent.width - x - 24; height: parent.height - y - 108
            clip: true; spacing: 2; boundsBehavior: Flickable.StopAtBounds
            model: root.profile.rows; currentIndex: root.profile.focusIndex
            onCurrentIndexChanged: if(currentIndex < count)positionViewAtIndex(currentIndex,ListView.Contain)
            delegate: Rectangle {
                required property int index
                required property var modelData
                width: list.width; height: 70; radius: 9
                color: root.profile.focusIndex === index ? Theme.selection : "transparent"
                opacity: modelData.enabled ? 1 : 0.55
                Text { x: 12; y: 8; text: modelData.title; font.pixelSize: 15; color: Theme.ink }
                Text { x: 12; y: 37; width: parent.width - 24; text: modelData.detail; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight; font.pixelSize: 11; color: Theme.muted }
                Row {
                    anchors.right: parent.right; anchors.rightMargin: 8; y: 1; height: 36
                    Rectangle {
                        width: 32; height: 36; color: "transparent"; visible: modelData.key !== "compatibility"
                        Text { anchors.centerIn: parent; text: "‹"; font.pixelSize: 24; color: Theme.accent }
                        MouseArea { anchors.fill: parent; enabled: modelData.enabled; onClicked: root.profile.adjust(index,-1) }
                    }
                    Rectangle {
                        width: Math.min(174, value.implicitWidth + 12); height: 36; color: "transparent"
                        Text { id: value; anchors.centerIn: parent; width: Math.min(166, implicitWidth); text: modelData.value; textFormat: Text.PlainText; elide: Text.ElideRight; font.pixelSize: 13; color: Theme.accent }
                        MouseArea { anchors.fill: parent; enabled: modelData.enabled; onClicked: root.profile.activate(index) }
                    }
                    Rectangle {
                        width: 32; height: 36; color: "transparent"
                        Text { anchors.centerIn: parent; text: "›"; font.pixelSize: 24; color: Theme.accent }
                        MouseArea { anchors.fill: parent; enabled: modelData.enabled; onClicked: root.profile.adjust(index,1) }
                    }
                }
                Item {
                    id: clockSlider
                    objectName: "gpu-clock-track"
                    x: 12; y: 53; width: parent.width - 24; height: 17
                    visible: modelData.key === "gpu" && modelData.enabled
                    readonly property real clockValue: clockTouch.pressed ? clockTouch.preview : Number(root.profile.draft.gpu || 0)
                    readonly property real clockRatio: Math.max(0,Math.min(1,(clockSlider.clockValue - root.profile.capabilities.gpuMin) / Math.max(1,root.profile.capabilities.gpuMax - root.profile.capabilities.gpuMin)))
                    Rectangle { y: 7; width: parent.width; height: 2; radius: 1; color: Theme.divider }
                    Rectangle { y: 7; width: parent.width * clockSlider.clockRatio; height: 2; radius: 1; color: Theme.accent }
                    Rectangle { x: parent.width * clockSlider.clockRatio - 4; y: 4; width: 8; height: 8; radius: 4; color: Theme.accent }
                    MouseArea {
                        id: clockTouch; anchors.fill: parent; preventStealing: true
                        objectName: "gpu-clock-slider"
                        property int preview: 0
                        function track(mouse) { preview = Math.round(root.profile.capabilities.gpuMin + Math.max(0,Math.min(1,mouse.x / width)) * (root.profile.capabilities.gpuMax - root.profile.capabilities.gpuMin)) }
                        onPressed: function(mouse) { track(mouse) }
                        onPositionChanged: function(mouse) { if(pressed)track(mouse) }
                        onReleased: root.profile.setGpuClock(preview)
                    }
                }
            }
        }
        Text { x: 220; y: panel.height - 100; width: panel.width - 250; height: 42; text: root.profile.applying ? "Applying power settings…" : root.profile.busy ? "Reading host capabilities…" : root.profile.status; wrapMode: Text.Wrap; maximumLineCount: 3; elide: Text.ElideRight; font.pixelSize: 11; color: Theme.muted }
        Rectangle { x: 26; y: parent.height - 51; width: parent.width - 52; height: 1; color: Theme.divider }
        Text { x: 27; anchors.bottom: parent.bottom; anchors.bottomMargin: 19; text: "L1 / R1  Section    ← →  Adjust    " + Theme.buttonLabel("B") + "  Close"; color: Theme.muted; font.pixelSize: 11 }
        Row {
            anchors.right: parent.right; anchors.rightMargin: 22; anchors.bottom: parent.bottom; anchors.bottomMargin: 9; spacing: 12
            Repeater {
                model: ["Reset", root.profile.live ? "Apply & save" : "Save profile"]
                delegate: Rectangle {
                    required property int index
                    required property string modelData
                    width: index ? 128 : 66; height: 34; radius: 8
                    opacity: root.profile.busy ? 0.5 : 1
                    color: root.profile.focusIndex === root.profile.rows.length + (index ? 0 : 1) ? Theme.selection : "transparent"
                    Text { anchors.centerIn: parent; text: modelData; font.pixelSize: 14; color: index ? Theme.accent : Theme.muted }
                    MouseArea { anchors.fill: parent; enabled: !root.profile.busy; onClicked: index ? root.profile.save() : root.profile.reset() }
                }
            }
        }
    }
}
