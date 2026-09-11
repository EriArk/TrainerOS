import QtQuick

Item {
    id: root
    required property var shell
    readonly property var worlds: shell.worlds
    readonly property bool takesFocus: visible && !shell.menuOpen && shell.notice.length === 0
    readonly property bool regionsOpen: worlds.route === "regions"
    readonly property bool listOpen: worlds.route === "adventures"
    readonly property bool detailOpen: worlds.route === "detail"

    Item {
        anchors.fill: parent; visible: root.regionsOpen
        Text { x: 29; y: 26; text: "Worlds"; color: Theme.ink; font.pixelSize: 35; font.weight: Font.DemiBold }
        Text { x: 31; y: 77; text: "Choose a region. Every World has its own stories."; color: Theme.muted; font.pixelSize: 16 }
        Rectangle {
            x: 0; y: 119; width: parent.width; height: parent.height - y
            color: "#d4e2d6"
            Rectangle { width: parent.width; height: 2; color: "#b6cbbb" }
            GridView {
                id: regionGrid
                x: 26; y: 18; width: 864; height: 252
                cellWidth: 288; cellHeight: 84; clip: true; interactive: false; keyNavigationEnabled: false
                currentIndex: root.worlds.regionIndex
                function revealCurrent() {
                    if (count && visible) positionViewAtIndex(currentIndex, GridView.Contain)
                    if (root.takesFocus && root.regionsOpen && currentItem) currentItem.control.forceActiveFocus(Qt.OtherFocusReason)
                }
                onCurrentIndexChanged: Qt.callLater(revealCurrent)
                onCurrentItemChanged: Qt.callLater(revealCurrent)
                onModelChanged: Qt.callLater(revealCurrent)
                onVisibleChanged: Qt.callLater(revealCurrent)
                Connections { target: root; function onTakesFocusChanged() { Qt.callLater(regionGrid.revealCurrent) } }
                    model: root.worlds.regions
                    delegate: Item {
                        required property int index
                        required property var modelData
                        width: regionGrid.cellWidth; height: regionGrid.cellHeight
                        property alias control: regionButton
                        CapButton {
                        id: regionButton; x: 4; y: 4
                        objectName: "world-" + index
                        width: 272; height: 68
                        label: modelData.name
                        detail: modelData.count === 0 ? "No Adventures yet" : root.shell.sampleLibrary ? modelData.count + " Adventures · " + modelData.status : modelData.owned + " linked / " + modelData.count + " Adventures"
                        tint: Theme.tabColors[index % Theme.tabColors.length]
                        selected: root.regionsOpen && root.takesFocus && root.worlds.focusIndex === index
                        onActivated: root.shell.activate(index)
                    }
                }
            }
            Column {
                x: 30; y: 30; spacing: 24; visible: root.worlds.regions.length === 0
                Text { text: "Your Worlds will appear here."; color: Theme.muted; font.pixelSize: 22 }
                CapButton {
                    objectName: "worlds-empty"
                    width: 310; height: 55; label: "Return Home"
                    selected: visible && root.regionsOpen && root.takesFocus
                    onActivated: root.shell.activate(0)
                }
            }
        }
    }

    Item {
        anchors.fill: parent; visible: root.listOpen
        Text { x: 29; y: 23; width: parent.width - 58; elide: Text.ElideRight; textFormat: Text.PlainText; text: root.worlds.region.name; color: Theme.ink; font.pixelSize: 35; font.weight: Font.DemiBold }
        Text { x: 31; y: 73; text: root.worlds.region.count + (root.shell.sampleLibrary ? " Adventures · sample library" : " Adventures · your library"); color: Theme.muted; font.pixelSize: 16 }
        Rectangle {
            x: 0; y: 104; width: 550; height: 228
            color: "#d4e2d6"
            Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: "#b6cbbb" }
            ListView {
                id: adventureList
                objectName: "adventure-list"
                x: 24; y: 6; width: 500; height: 216
                model: root.worlds.adventures
                currentIndex: root.worlds.adventureIndex
                clip: true; interactive: false; keyNavigationEnabled: false
                boundsBehavior: Flickable.StopAtBounds
                highlightMoveDuration: 0; highlightResizeDuration: 0
                cacheBuffer: 216
                function revealCurrent() {
                    if (count > 0 && root.listOpen) positionViewAtIndex(currentIndex, ListView.Contain);
                }
                onCurrentIndexChanged: Qt.callLater(revealCurrent)
                onModelChanged: Qt.callLater(revealCurrent)
                Connections { target: root; function onListOpenChanged() { Qt.callLater(adventureList.revealCurrent); } }
                delegate: Item {
                    required property int index
                    required property var modelData
                    width: adventureList.width; height: 72
                    CapButton {
                        objectName: "adventure-" + modelData.id
                        x: 5; y: 5; width: parent.width - 10; height: 58
                        label: modelData.title
                        detail: modelData.missing ? "Missing · A to link a file" : modelData.variant || modelData.kind + " · " + modelData.status
                        platform: root.shell.sampleLibrary ? "" : modelData.platform
                        platformShape: modelData.platformShape
                        tint: modelData.missing ? "#c4cdc7" : modelData.kind === "ROM hack" ? Theme.pink : modelData.kind === "Remake" ? Theme.blue : Theme.green
                        selected: root.listOpen && root.takesFocus && root.worlds.focusIndex === index
                        onActivated: root.shell.activate(index)
                    }
                }
            }
            Text {
                x: 30; y: 43; width: 474; wrapMode: Text.WordWrap
                visible: root.worlds.adventures.length === 0
                text: "No Adventures here yet.\nChoose another World to keep exploring."
                color: Theme.muted; font.pixelSize: 21; lineHeight: 1.3
            }
            Rectangle {
                x: 533; y: 12; width: 4; height: parent.height - 24; radius: 2; color: "#b0c6b6"
                visible: adventureList.contentHeight > adventureList.height
                Rectangle {
                    width: 4; radius: 2; color: Theme.chassis
                    height: parent.height * Math.min(1, adventureList.height / Math.max(1, adventureList.contentHeight))
                    y: (parent.height - height) * Math.max(0, Math.min(1, adventureList.contentY / Math.max(1, adventureList.contentHeight - adventureList.height)))
                }
            }
        }
        Rectangle {
            x: 568; y: 0; width: parent.width - x; height: 332; color: "#e1ecde"
            Text { x: 24; y: 26; text: "ADVENTURE RECORD"; color: Theme.muted; font.pixelSize: 12; font.letterSpacing: 1.3 }
            TrainerEmblem { x: 111; y: 65; width: 126; height: 126; emblem: root.worlds.detail.kind === "ROM hack" ? "spark" : "compass" }
            Text { x: 24; y: 209; width: parent.width - 48; text: root.worlds.detail.title; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 23; font.weight: Font.DemiBold }
            Text { x: 24; y: 275; width: parent.width - 48; text: root.worlds.detail.availability; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: 14 }
        }
        Rectangle {
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
            height: 78; color: "#c6dcca"
            CapButton {
                objectName: "world-list-back"
                x: 30; y: 18; width: 255; height: 42; label: "Back to Worlds"; tint: Theme.blue
                selected: root.listOpen && root.takesFocus && root.worlds.focusIndex === root.worlds.adventures.length
                onActivated: root.shell.activate(root.worlds.adventures.length)
            }
            Text {
                x: 310; y: 30; text: root.worlds.adventures.length > 0 ? (root.worlds.adventureIndex + 1) + " / " + root.worlds.adventures.length + "  ·  ↑ ↓ Adventures   A Open   B Back" : "B · Back to Worlds"
                color: Theme.muted; font.pixelSize: 14
            }
        }
    }

    Item {
        anchors.fill: parent; visible: root.detailOpen
        Text { x: 30; y: 17; width: parent.width - 60; elide: Text.ElideRight; textFormat: Text.PlainText; text: "WORLDS / " + root.worlds.region.name.toUpperCase(); color: Theme.muted; font.pixelSize: 12; font.letterSpacing: 1.3 }
        Text { x: 29; y: 42; width: parent.width - 58; text: root.worlds.detail.title; textFormat: Text.PlainText; elide: Text.ElideRight; color: Theme.ink; font.pixelSize: 32; font.weight: Font.DemiBold }
        Text { x: 31; y: 89; width: parent.width - 62; elide: Text.ElideRight; text: root.worlds.detail.kind + " · " + (root.shell.sampleLibrary ? root.worlds.detail.status + " · sample data" : root.worlds.detail.platform + (root.worlds.detail.variant ? " · " + root.worlds.detail.variant : "")); color: Theme.muted; font.pixelSize: 15 }
        Text {
            x: 31; y: 133; width: 491; height: 82
            text: root.worlds.detail.limitation || root.worlds.detail.description || "Your own journey, ready to become part of the collection."; textFormat: Text.PlainText; wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight
            color: Theme.ink; font.pixelSize: 17
        }
        Row {
            x: 31; y: 239; spacing: 14
            Repeater {
                model: [{value: root.worlds.detail.badges, label: "BADGES"}, {value: root.worlds.detail.caught, label: "CAUGHT"}]
                delegate: Rectangle {
                    required property var modelData
                    width: 147; height: 70; radius: 12; color: "#edf2e7"; border.color: "#c5d6c4"
                    Text { x: 15; y: 4; text: modelData.value; color: Theme.ink; font.pixelSize: 30; font.weight: Font.DemiBold }
                    Text { x: 16; y: 46; text: modelData.label; color: Theme.muted; font.pixelSize: 11; font.letterSpacing: 1 }
                }
            }
        }
        Rectangle {
            x: 568; y: 119; width: parent.width - x; height: 213; color: "#e1ecde"
            Text { x: 23; y: 21; text: "YOUR NEXT STEP"; color: Theme.muted; font.pixelSize: 12; font.letterSpacing: 1 }
            Text { x: 23; y: 54; width: parent.width - 46; text: root.worlds.detail.availability; wrapMode: Text.WordWrap; color: Theme.ink; font.pixelSize: 22; font.weight: Font.DemiBold }
            Text { x: 23; y: 151; width: parent.width - 46; text: root.worlds.detail.resume; elide: Text.ElideRight; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 15 }
        }
        Rectangle {
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
            height: 78; color: "#c6dcca"
            Row {
                x: 30; y: 18; spacing: 16
                Repeater {
                    model: root.worlds.actions
                    delegate: CapButton {
                        required property int index
                        required property var modelData
                        objectName: "world-action-" + modelData.id
                        width: Math.min(270, (root.width - 60 - 16 * (root.worlds.actions.length - 1)) / root.worlds.actions.length); height: 42; label: modelData.label; textSize: 16
                        enabled: modelData.enabled; opacity: enabled ? 1 : 0.5
                        tint: modelData.id === "resume" ? Theme.yellow : modelData.id === "launch" ? Theme.green : Theme.blue
                        selected: root.detailOpen && root.takesFocus && enabled && root.worlds.focusIndex === index
                        onActivated: root.shell.activate(index)
                    }
                }
            }
        }
    }
}
