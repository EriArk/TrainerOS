import QtQuick

Item {
    id: root
    required property var shell
    required property var flow
    required property var session
    readonly property bool selecting: !flow.connections && !flow.service.length && flow.stage !== "trainer"
    readonly property bool takesFocus: visible && !shell.keyboard.open
    Row {
        x: 225; y: 26; width: parent.width-x-24
        Repeater {
            model: root.flow.stepLabels
            Item {
                required property int index
                required property string modelData
                width: parent.width/root.flow.stepLabels.length; height: 45
                Rectangle { x: 10; y: 8; width: parent.width; height: 2; color: index<root.flow.step ? "#b7a56d" : "#dce2d6"; visible: index<root.flow.stepLabels.length-1 }
                Rectangle {
                    width: 19; height: 19; radius: 10; color: index<=root.flow.step ? "#f1ce74" : "#e0e7db"; border.color: "#997e42"
                    Text { anchors.centerIn: parent; text: index<root.flow.step ? "✓" : index+1; font.pixelSize: 10; color: Theme.ink }
                }
                Text { x: -4; y: 24; width: parent.width; text: modelData; color: index===root.flow.step ? Theme.ink : Theme.muted; font.pixelSize: 11; font.bold: index===root.flow.step; elide: Text.ElideRight }
            }
        }
    }
    Item {
        x: 24; y: 84; width: parent.width-48; height: parent.height-y-83
        visible: root.selecting
        Rectangle { anchors.fill: parent; color: "#fffdf4"; radius: 12 }
        Rectangle { width: parent.width*0.30; height: parent.height; color: "#e4eddc"; radius: 12 }
        Text { x: 18; y: 20; text: "YOUR ADVENTURE KIT"; font.family: Theme.brandFamily; font.pixelSize: 13; color: "#5a7d70" }
        SetupIllustration { x: 0; y: 62; width: parent.width*0.30; height: Math.min(245,parent.height-100); stage: root.flow.stage; checkedControls: root.flow.checkedControls }
        Item {
            x: parent.width*0.33; y: 18; width: parent.width-x-24; height: parent.height-30
            Text { id: heading; width: parent.width; text: root.flow.title; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 28; wrapMode: Text.WordWrap }
            Text { id: subtitle; y: heading.height+9; width: parent.width; text: root.flow.description; color: Theme.muted; font.pixelSize: 14; wrapMode: Text.WordWrap }
            Item {
                y: subtitle.y+subtitle.height+16; width: parent.width; height: parent.height-y-32
                StorageChoices { anchors.fill: parent; flow: root.flow; visible: root.flow.stage==="storage"; takesFocus: root.takesFocus }
                ClockPane { anchors.fill: parent; clock: root.flow.clock; externalError: root.flow.error; compact: true; visible: root.flow.stage==="clock"; takesFocus: root.takesFocus }
                Item {
                    anchors.fill: parent; visible: root.flow.stage==="controls"
                    Row {
                        width: parent.width; spacing: 8
                        Repeater {
                            model: ["↑", "↓", "←", "→", "Confirm", "Back"]
                            Rectangle {
                                required property int index
                                required property string modelData
                                readonly property bool done: (root.flow.checkedControls & (1<<index))!==0
                                width: (parent.width-40)/6; height: 65; radius: 12
                                color: done ? "#f2d275" : "#e4ecdf"; border.color: "#abbfaf"
                                Text { anchors.centerIn: parent; text: modelData; color: Theme.ink; font.pixelSize: index<4?25:14 }
                                Text { anchors.horizontalCenter: parent.horizontalCenter; y: 72; text: done ? "✓" : "·"; color: Theme.muted; font.pixelSize: 22 }
                            }
                        }
                    }
                    CapButton { objectName: "setup-use-touch"; y: 111; width: parent.width; height: 55; label: "Continue with touch"; detail: "Controller checks remain available in Settings"; onActivated: root.flow.useTouch() }
                }
                ListView {
                    id: choices; anchors.fill: parent; clip: true; spacing: 9
                    visible: !["storage","controls","clock"].includes(root.flow.stage)
                    model: root.flow.rows; currentIndex: root.flow.focusIndex; keyNavigationEnabled: false
                    function reveal() { if(count)positionViewAtIndex(currentIndex,ListView.Contain) }
                    onCurrentIndexChanged: Qt.callLater(reveal)
                    onCountChanged: Qt.callLater(reveal)
                    onHeightChanged: Qt.callLater(reveal)
                    Rectangle {
                        anchors.right: parent.right; width: 3; height: parent.height; radius: 2; z: 2
                        visible: choices.contentHeight>choices.height; color: "#b5c7b8"
                        Rectangle { width: parent.width; height: Math.max(16,choices.visibleArea.heightRatio*parent.height); y: choices.visibleArea.yPosition*parent.height; radius: 2; color: Theme.muted }
                    }
                    delegate: CapButton {
                        required property int index
                        required property var modelData
                        objectName: "setup-choice-"+index; width: choices.width-4; height: 59
                        label: modelData.label; detail: modelData.detail; textSize: 18
                        tint: index===choices.count-1?Theme.yellow:Theme.blue
                        selected: root.takesFocus && index===choices.currentIndex && !root.flow.busy
                        enabled: !root.flow.busy && modelData.enabled !== false; onActivated: root.flow.activate(index)
                    }
                }
            }
            Text { anchors.bottom: parent.bottom; width: parent.width; height: 28; text: root.flow.busy ? "Preparing your library…" : root.flow.error; textFormat: Text.PlainText; color: "#853b24"; font.pixelSize: 13; wrapMode: Text.WordWrap }
        }
    }
    FirstRunServicePanel {
        x: 35; y: 83; width: parent.width-70; height: parent.height-y-85
        visible: root.flow.service.length>0
        shell: root.shell; flow: root.flow; session: root.session
    }
    ConnectionsPane { x: 40; y: 83; width: parent.width-80; height: parent.height-y-85; shell: root.shell; onboarding: true; visible: root.flow.connections }
    CapButton {
        objectName: "setup-back"; x: 26; anchors.bottom: parent.bottom; anchors.bottomMargin: 34; width: 170; height: 38
        visible: root.flow.connections || root.flow.service.length>0 || ["network","clock","storage","trainer","preferences","ready"].includes(root.flow.stage)
        enabled: !root.flow.busy && !root.shell.keyboard.open && !root.shell.settings.saving
        label: "Back"; textSize: 16; onActivated: root.session.backInSetup()
    }
}
