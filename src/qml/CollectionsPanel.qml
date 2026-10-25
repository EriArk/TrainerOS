import QtQuick

Item {
    id: root
    required property var shell
    readonly property var model: shell.collectionManager
    readonly property bool takesFocus: visible && !shell.keyboard.open && !shell.menuOpen && !shell.notice.length
    readonly property bool picker: model.route === "membership" || model.route === "membership-new"
    Rectangle { anchors.fill: parent; color: Theme.surfaceColor("#660d2525") }
    TapHandler { onTapped: {} }
    Rectangle {
        visible: !root.picker
        anchors.centerIn: parent; width: Math.min(root.model.smart ? 900 : 620,parent.width-32); height: Math.min(460,parent.height-24)
        radius: 16; color: Theme.paper; border.color: Theme.divider
        Text { x: 22; y: 17; width: parent.width-80; text: root.model.title; textFormat: Text.PlainText; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 24; elide: Text.ElideRight }
        SocialIconButton { anchors.right: parent.right; anchors.rightMargin: 14; y: 12; icon: "x"; label: "Back"; onClicked: root.shell.pressButton("B") }
        Text { x: 22; y: 50; width: parent.width-44; text: root.model.detail; color: Theme.muted; font.pixelSize: 13; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight }
        ListView {
            id: list; x: 18; y: 91; width: root.model.smart ? (parent.width-56)*0.52 : parent.width-36; height: parent.height-y-44; clip: true
            model: root.model.rows; currentIndex: root.model.focusIndex; keyNavigationEnabled: false
            function reveal() { if(root.takesFocus && !root.picker && currentItem) { positionViewAtIndex(currentIndex,ListView.Contain);currentItem.forceActiveFocus(Qt.OtherFocusReason) } }
            onCurrentIndexChanged: Qt.callLater(reveal)
            onCurrentItemChanged: Qt.callLater(reveal)
            Connections { target: root; function onTakesFocusChanged() { Qt.callLater(list.reveal) } }
            delegate: SettingControl { required property int index; required property var modelData
                objectName: "collection-option-"+index; width: list.width; height: 48; flat: true
                title: modelData.label; kind: "action"; selected: root.takesFocus && !root.picker && index===list.currentIndex
                onActivated: root.shell.activate(index)
            }
        }
        Rectangle { x: list.x+list.width+12; y: 92; width: 1; height: list.height; color: Theme.divider; visible: root.model.smart }
        ListView { id: preview; x: list.x+list.width+25; y: 91; width: parent.width-x-18; height: list.height; clip: true
            visible: root.model.smart; model: visible ? root.model.preview : []
            header: Text { width: preview.width; height: 32; text: root.model.route==="preview" ? "Library · tap to change inclusion" : "Preview · "+preview.count+" games"; color: Theme.muted; font.pixelSize: 13 }
            delegate: Item { required property var modelData; width: preview.width; height: 62
                Image { x: 0; y: 5; width: 44; height: 50; source: modelData.picture || ""; fillMode: Image.PreserveAspectFit; asynchronous: true }
                Text { x: 54; y: 9; width: parent.width-60; text: modelData.label; color: Theme.ink; font.pixelSize: 15; elide: Text.ElideRight; textFormat: Text.PlainText }
                Text { x: 54; y: 32; width: parent.width-60; text: modelData.platform+" · "+modelData.state; color: Theme.muted; font.pixelSize: 12; elide: Text.ElideRight }
                TapHandler { onTapped: root.model.overrideGame(modelData.id) }
            }
            Text { anchors.centerIn: parent; width: parent.width-10; visible: !preview.count; text: "No games match yet.\nChange a condition or open Preview & exceptions."; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter; color: Theme.muted; font.pixelSize: 14 }
        }
        Text { x: 22; anchors.bottom: parent.bottom; anchors.bottomMargin: 10; width: parent.width-44; text: root.model.error || "Changes are saved only with Save collection."; color: root.model.error.length ? Theme.pink : Theme.muted; font.pixelSize: 12; elide: Text.ElideRight; textFormat: Text.PlainText }
    }
    Loader {
        anchors.fill: parent
        active: root.picker
        sourceComponent: Component {
            Item {
                readonly property bool creating: root.model.route === "membership-new"
                readonly property int collectionCount: Math.max(0, root.model.rows.length - 2)
                Rectangle {
                    x: panel.x - 2; y: panel.y + 5; width: panel.width + 4; height: panel.height + 3
                    radius: 20; color: Theme.surfaceColor("#50302529")
                }
                Rectangle {
                    id: panel; objectName: "collection-picker"
                    anchors.centerIn: parent
                    width: Math.min(480, parent.width - 40)
                    height: body.implicitHeight + 36
                    radius: 18; color: Theme.paper; border.width: 3; border.color: Theme.chassis
                    Rectangle { anchors.fill: parent; anchors.margins: 4; radius: 14; color: "transparent"; border.color: Theme.rim }
                    Column {
                        id: body
                        x: 22; y: 18; width: parent.width - 44; spacing: 8
                        Text {
                            width: parent.width; text: root.model.title; textFormat: Text.PlainText
                            font.family: Theme.displayFamily; font.pixelSize: 25; color: Theme.ink
                        }
                        Text {
                            width: parent.width; text: root.model.detail; textFormat: Text.PlainText
                            font.pixelSize: 16; color: Theme.muted; elide: Text.ElideRight
                        }
                        Rectangle { width: parent.width; height: 1; color: Theme.rim; opacity: 0.4 }
                        Item {
                            id: content; width: parent.width
                            height: creating ? 54 : collectionCount ? Math.min(192, collectionCount * 48,
                                Math.max(48, root.height - 198 - (problem.visible ? problem.height + 8 : 0))) : 70
                            Text {
                                anchors.centerIn: parent; width: parent.width - 20
                                visible: !creating && collectionCount === 0
                                text: "Your first collection starts here.\nCreate one to keep these games together."
                                color: Theme.muted; font.pixelSize: 16; horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.Wrap
                            }
                            CapButton {
                                objectName: "collection-name"; visible: creating
                                x: 4; y: 5; width: parent.width - 8; height: 40
                                label: root.model.rows.length ? root.model.rows[0].label : ""
                                tint: Theme.blue; selected: root.takesFocus && creating && root.model.focusIndex === 0
                                onActivated: root.shell.activate(0)
                            }
                            ListView {
                                id: choices; objectName: "collection-choices"
                                anchors.fill: parent; anchors.margins: 4
                                visible: !creating; clip: true; keyNavigationEnabled: false
                                model: creating ? [] : root.model.rows.slice(0, -2)
                                currentIndex: root.model.focusIndex < count ? root.model.focusIndex : -1
                                function reveal() {
                                    if (!root.takesFocus || currentIndex < 0) return
                                    positionViewAtIndex(currentIndex, ListView.Contain)
                                    if(currentItem) currentItem.forceActiveFocus(Qt.OtherFocusReason)
                                }
                                onCurrentIndexChanged: Qt.callLater(reveal)
                                onCurrentItemChanged: Qt.callLater(reveal)
                                Connections { target: root; function onTakesFocusChanged() { Qt.callLater(choices.reveal) } }
                                delegate: Item {
                                    required property int index
                                    required property var modelData
                                    objectName: "collection-choice-" + index
                                    width: choices.width; height: 48
                                    readonly property bool selected: root.takesFocus && choices.currentIndex === index
                                    Accessible.role: Accessible.Button
                                    Accessible.name: modelData.label + (modelData.member ? ", already added" : "")
                                    Rectangle {
                                        x: 1; y: 2; width: parent.width - 2; height: parent.height - 4; radius: 10
                                        color: "transparent"; border.width: 2
                                        border.color: parent.selected ? Theme.focusGlow : "transparent"
                                    }
                                    Rectangle {
                                        x: 10; anchors.verticalCenter: parent.verticalCenter; width: 30; height: 30; radius: 15
                                        color: modelData.member ? Theme.green : Theme.blue
                                        Text { anchors.centerIn: parent; text: modelData.member ? "✓" : "+"; color: Theme.ink; font.pixelSize: 22 }
                                    }
                                    Text {
                                        x: 52; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 110
                                        text: modelData.label; textFormat: Text.PlainText; color: Theme.ink
                                        font.pixelSize: 18; elide: Text.ElideRight
                                    }
                                    Text {
                                        anchors.right: parent.right; anchors.rightMargin: 13; anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.count; color: Theme.muted; font.pixelSize: 15
                                    }
                                    Rectangle { x: 52; y: parent.height - 1; width: parent.width - 65; height: 1; color: Theme.rim; opacity: 0.23 }
                                    TapHandler { onTapped: root.shell.activate(index) }
                                }
                            }
                            Rectangle {
                                anchors.right: parent.right; width: 2; radius: 1; color: Theme.rim
                                visible: !creating && choices.contentHeight > choices.height
                                height: Math.max(12, choices.height * choices.height / Math.max(1, choices.contentHeight))
                                y: 4 + Math.max(0, choices.contentY) / Math.max(1, choices.contentHeight - choices.height) * Math.max(0, choices.height - height)
                            }
                        }
                        Text {
                            id: problem; width: parent.width; visible: text.length > 0; text: root.model.error
                            textFormat: Text.PlainText; color: Theme.textColor("#803724"); font.pixelSize: 14; wrapMode: Text.Wrap
                        }
                        Row {
                            width: parent.width; spacing: 12
                            CapButton {
                                objectName: "collection-create"; width: parent.width - 116; height: 38
                                label: creating ? "Create & add game" : "Create collection"; tint: Theme.yellow; centered: true
                                selected: root.takesFocus && root.model.focusIndex === (creating ? 1 : collectionCount)
                                onActivated: root.shell.activate(creating ? 1 : collectionCount)
                            }
                            CapButton {
                                objectName: "collection-cancel"; width: 104; height: 38
                                label: "Cancel"; tint: Theme.blue; centered: true
                                selected: root.takesFocus && root.model.focusIndex === (creating ? 2 : collectionCount + 1)
                                onActivated: root.shell.pressButton("B")
                            }
                        }
                    }
                }
            }
        }
    }
}
