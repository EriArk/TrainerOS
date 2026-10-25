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
        anchors.centerIn: parent; width: Math.min(640,parent.width-40); height: Math.min(420,parent.height-24)
        radius: 18; color: Theme.surfaceColor("#e8edda"); border.color: Theme.surfaceColor("#35544f"); border.width: 3
        Column {
            x: 20; y: 16; width: parent.width-40; spacing: 8
            Text { width: parent.width; text: root.model.title; textFormat: Text.PlainText; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 25; font.bold: true; elide: Text.ElideRight }
            Text { width: parent.width; text: root.model.detail; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 14; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight }
            ListView {
                id: list; width: parent.width; height: 254; clip: true; spacing: 6
                interactive: true; keyNavigationEnabled: false
                model: root.model.rows; currentIndex: root.model.focusIndex
                function reveal() {
                    if(!root.takesFocus || root.picker || !currentItem)return
                    positionViewAtIndex(currentIndex,ListView.Contain)
                    currentItem.forceActiveFocus(Qt.OtherFocusReason)
                }
                onCurrentIndexChanged: Qt.callLater(reveal)
                onCurrentItemChanged: Qt.callLater(reveal)
                Connections { target: root; function onTakesFocusChanged() { Qt.callLater(list.reveal) } }
                delegate: CapButton {
                    required property int index; required property var modelData
                    objectName: "collection-option-"+index
                    width: list.width; height: 38; textSize: 17; label: modelData.label
                    tint: Theme.blue; selected: root.takesFocus && !root.picker && root.model.focusIndex===index
                    onActivated: root.shell.activate(index)
                }
            }
            Text { width: parent.width; text: root.model.error; visible: text.length>0; color: Theme.textColor("#803724"); font.pixelSize: 14; wrapMode: Text.Wrap; textFormat: Text.PlainText }
        }
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
