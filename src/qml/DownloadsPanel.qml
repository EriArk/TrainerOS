import QtQuick

Item {
    id: root
    required property var flow
    readonly property var currentTask: flow.tasks[flow.focusIndex] || ({})
    Rectangle { anchors.fill: parent; color: "#7a000000" }
    MouseArea { anchors.fill: parent }

    component QuietAction: Rectangle {
        property string label
        signal activated()
        implicitWidth: 120; implicitHeight: 32
        color: "transparent"; opacity: enabled ? 1 : .35
        Text { anchors.centerIn: parent; text: parent.label; color: Theme.ink; font.pixelSize: 12 }
        MouseArea { anchors.fill: parent; onClicked: parent.activated() }
        Accessible.role: Accessible.Button
        Accessible.name: label
        Accessible.onPressAction: if(enabled) activated()
    }

    Rectangle {
        id: panel
        objectName: "downloads-panel"
        anchors.centerIn: parent
        width: Math.min(760,parent.width-32)
        height: Math.min(440,parent.height-24,Math.max(270,192+root.flow.tasks.length*58))
        radius: 18; color: Theme.paper; border.color: Theme.divider
        CrtTexture { anchors.fill: parent; anchors.margins: 2 }
        Item {
            anchors.fill: parent
            enabled: !root.flow.actionsFocused
            Text { x: 24; y: 17; text: "Downloads"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 25 }
            Text { x: 24; y: 49; text: root.flow.summary; color: Theme.muted; font.pixelSize: 12 }
            SocialIconButton { objectName: "downloads-close"; anchors.right: parent.right; anchors.rightMargin: 18; y: 18; icon: "x"; label: "Close downloads"; onClicked: root.flow.close() }
            Row {
                x: 18; y: 72; spacing: 6
                Repeater {
                    model: [{id:"pause-all",key:"X",label:"Pause all"},{id:"resume-all",key:"Y",label:"Resume all"},{id:"cancel-all",key:"Select",label:"Cancel all"}]
                    QuietAction { required property var modelData; objectName: "downloads-"+modelData.id
                        width: modelData.id==="cancel-all"?145:120
                        label: Theme.buttonLabel(modelData.key)+"  "+modelData.label
                        enabled: root.flow.tasks.some(t => !t.terminal)
                        onActivated: root.flow.controlAll(modelData.id)
                    }
                }
            }
            Rectangle { x: 24; y: 108; width: parent.width-48; height: 1; color: Theme.divider }
            ListView {
                id: list; objectName: "downloads-list"
                x: 18; y: 115; width: parent.width-36; height: parent.height-y-51
                clip: true; spacing: 2
                model: root.flow.tasks; currentIndex: root.flow.focusIndex; keyNavigationEnabled: false
                section.property: "section"
                section.delegate: Item {
                    required property string section
                    width: list.width; height: section.length ? 26 : 0
                    Text { x: 8; anchors.verticalCenter: parent.verticalCenter; text: section; color: Theme.muted; font.pixelSize: 11 }
                }
                onCurrentIndexChanged: Qt.callLater(function(){list.positionViewAtIndex(list.currentIndex,ListView.Contain)})
                delegate: Rectangle {
                    required property int index; required property var modelData
                    objectName: "downloads-task-"+modelData.id
                    width: list.width-6; height: 56; radius: 7
                    color: index===list.currentIndex ? Theme.selection : "transparent"
                    border.width: index===list.currentIndex ? 1 : 0; border.color: Theme.focusGlow
                    Rectangle { x: 62; anchors.bottom: parent.bottom; width: parent.width-x-16; height: 1; color: Theme.divider; visible: index!==list.currentIndex }
                    Item {
                        x: 8; y: 7; width: 42; height: 42
                        Image { id: thumbnail; anchors.fill: parent; source: modelData.picture || ""; asynchronous: true; sourceSize: Qt.size(84,84); fillMode: Image.PreserveAspectFit }
                        MenuGlyph { anchors.centerIn: parent; width: 24; height: 24; glyph: "file"; ink: Theme.muted; visible: thumbnail.status!==Image.Ready && !modelData.platform }
                        PlatformBadge { anchors.centerIn: parent; scale: .6; label: modelData.platform || ""; shape: modelData.shape || "console"; visible: thumbnail.status!==Image.Ready && !!modelData.platform }
                    }
                    Text { x: 62; y: 6; width: parent.width-x-32; text: modelData.title; textFormat: Text.PlainText; elide: Text.ElideRight; color: Theme.ink; font.pixelSize: 15 }
                    Text { x: 62; y: 26; width: parent.width-x-32; text: modelData.detail || modelData.state; textFormat: Text.PlainText; elide: Text.ElideRight; color: Theme.muted; font.pixelSize: 11 }
                    Text { anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter; text: "›"; font.pixelSize: 22; color: Theme.muted; visible: (modelData.actions || []).length>0 }
                    Rectangle {
                        id: progressTrack; x: 62; y: 48; width: parent.width-x-16; height: 2; color: Theme.divider
                        visible: ["running","queued","paused"].includes(modelData.state)
                        Rectangle {
                            id: progressFill
                            readonly property bool indeterminate: modelData.state==="running" && modelData.progress===undefined
                            onIndeterminateChanged: if(!indeterminate) x=0
                            height: parent.height
                            width: parent.width*(modelData.state==="running"&&modelData.progress===undefined ? .2 : Math.max(0,Math.min(1,Number(modelData.progress || 0))))
                            color: Theme.accent
                            NumberAnimation on x { running: progressFill.visible && progressFill.indeterminate; from: 0; to: progressTrack.width-progressTrack.width*.2; duration: 1700; loops: Animation.Infinite }
                        }
                    }
                    TapHandler { onTapped: root.flow.showActions(index) }
                    Accessible.role: Accessible.Button
                    Accessible.name: modelData.title+" "+(modelData.detail || modelData.state)
                    Accessible.onPressAction: root.flow.showActions(index)
                }
                Rectangle { anchors.right: parent.right; width: 2; height: parent.height; color: Theme.divider; visible: list.contentHeight>list.height; z: 2
                    Rectangle { width: parent.width; height: Math.max(16,list.visibleArea.heightRatio*parent.height); y: list.visibleArea.yPosition*parent.height; color: Theme.muted; radius: 1 }
                }
                Text { anchors.centerIn: parent; width: parent.width-32; visible: list.count===0; text: "Nothing in the queue\nNew downloads will appear here"; horizontalAlignment: Text.AlignHCenter; lineHeight: 1.5; color: Theme.muted; font.pixelSize: 14 }
            }
            Text { x: 24; y: parent.height-43; width: parent.width-48; text: root.currentTask.source || "Closing this window keeps the queue running"; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight }
            Text { x: 24; y: parent.height-25; text: (root.flow.actions.length ? Theme.buttonLabel("A")+" Actions · " : "")+Theme.buttonLabel("B")+" Close"; color: Theme.muted; font.pixelSize: 11 }
        }
    }

    Rectangle { anchors.fill: parent; color: "#66000000"; visible: root.flow.actionsFocused
        MouseArea { anchors.fill: parent }
    }
    Rectangle {
        id: actionPanel
        objectName: "downloads-actions"
        visible: root.flow.actionsFocused
        anchors.centerIn: panel; width: Math.min(530,panel.width-32)
        height: Math.min(panel.height-24,Math.max(188,130+root.flow.actions.length*46))
        radius: 16; color: Theme.paper; border.color: Theme.divider
        Text { x: 22; y: 19; width: parent.width-80; text: root.currentTask.title || "Download"; textFormat: Text.PlainText; font.family: Theme.displayFamily; font.pixelSize: 20; color: Theme.ink; elide: Text.ElideRight }
        Text { x: 22; y: 47; width: parent.width-44; text: root.currentTask.detail || root.currentTask.state || ""; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight }
        SocialIconButton { objectName: "downloads-actions-back"; anchors.right: parent.right; anchors.rightMargin: 14; y: 16; icon: "x"; label: "Back to downloads"; onClicked: root.flow.dismissActions() }
        Rectangle { x: 22; y: 72; width: parent.width-44; height: 1; color: Theme.divider }
        ListView {
            id: commands; x: 16; y: 82; width: parent.width-32; height: parent.height-y-38
            clip: true; spacing: 2; keyNavigationEnabled: false
            model: root.flow.actions; currentIndex: root.flow.actionIndex
            onCurrentIndexChanged: Qt.callLater(function(){commands.positionViewAtIndex(commands.currentIndex,ListView.Contain)})
            delegate: Rectangle {
                required property int index; required property var modelData
                objectName: "downloads-command-"+modelData.id
                width: commands.width; height: Math.max(42,commandLabel.implicitHeight+14); radius: 6
                color: commands.currentIndex===index ? Theme.selection : "transparent"
                border.width: commands.currentIndex===index ? 1 : 0; border.color: Theme.focusGlow
                enabled: modelData.enabled!==false; opacity: enabled ? 1 : .35
                Text { id: commandLabel; x: 12; anchors.verticalCenter: parent.verticalCenter; width: parent.width-24; text: modelData.label; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 14; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight }
                MouseArea { anchors.fill: parent; onClicked: root.flow.activate(index) }
                Accessible.role: Accessible.Button
                Accessible.name: modelData.label
                Accessible.onPressAction: if(enabled) root.flow.activate(index)
            }
        }
        Text { x: 22; anchors.bottom: parent.bottom; anchors.bottomMargin: 13; text: Theme.buttonLabel("A")+" Select · "+Theme.buttonLabel("B")+" Back"; color: Theme.muted; font.pixelSize: 11 }
    }
}
