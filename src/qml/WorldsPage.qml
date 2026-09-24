import QtQuick

Item {
    id: root
    required property var shell
    readonly property var worlds: shell.worlds
    readonly property bool takesFocus: visible && !shell.libraryTools.open && !shell.menuOpen && !shell.keyboard.open && shell.notice.length === 0
    readonly property bool regionsOpen: worlds.route === "regions"
    readonly property bool listOpen: worlds.route === "adventures"
    readonly property bool detailOpen: worlds.route === "detail"

    CapButton {
        objectName: "world-edit"; z: 2
        anchors.right: parent.right; anchors.rightMargin: 20; y: 14; width: 48; height: 42
        visible: root.shell.canEditWorld; label: ""; tint: Theme.blue
        onActivated: root.shell.activate(0,"world-edit")
        Canvas {anchors.centerIn: parent; width: 26; height: 26
            onPaint: {
                const c=getContext("2d");c.reset();c.strokeStyle=Theme.ink;c.lineWidth=5;c.lineCap="round";
                c.beginPath();c.moveTo(5,21);c.lineTo(17,9);c.stroke();
                c.lineWidth=4;c.beginPath();c.arc(19,7,5,0,Math.PI*1.5);c.stroke();
            }
        }
    }
    Item {
        anchors.fill: parent; visible: root.regionsOpen
        PageHeader { id: regionsHeader; title: "Worlds"; compact: true; trailing: root.shell.canEditWorld ? "" : (root.worlds.regionTileIndex + 1) + " / " + root.worlds.regionTiles.length }
        MountedPanel {
            x: 0; y: regionsHeader.height; width: parent.width; height: parent.height - y
            color: "#d4e2d6"
            Rectangle { width: parent.width; height: 2; color: "#b6cbbb" }
            GridView {
                id: regionGrid
                x: 22; y: 10; width: parent.width-44; height: parent.height-20
                cellWidth: width/3; cellHeight: height/2; clip: true; interactive: false; keyNavigationEnabled: false
                currentIndex: root.worlds.regionTileIndex
                function revealCurrent() {
                    if (count && visible) positionViewAtIndex(currentIndex, GridView.Contain)
                    if (root.takesFocus && root.regionsOpen && currentItem) currentItem.control.forceActiveFocus(Qt.OtherFocusReason)
                }
                onCurrentIndexChanged: Qt.callLater(revealCurrent)
                onCurrentItemChanged: Qt.callLater(revealCurrent)
                onModelChanged: Qt.callLater(revealCurrent)
                onVisibleChanged: Qt.callLater(revealCurrent)
                Connections { target: root; function onTakesFocusChanged() { Qt.callLater(regionGrid.revealCurrent) } }
                    model: root.worlds.regionTiles
                    delegate: Item {
                        required property int index
                        required property var modelData
                        width: regionGrid.cellWidth; height: regionGrid.cellHeight
                        readonly property bool paired: modelData.members.length===2
                        property var control: paired && root.worlds.regionIndex===modelData.members[1].index ? second : first
                        WorldCard {
                            id: first; x: 6; y: 6; width: parent.width-12; height: parent.height-14
                            objectName: "world-" + entry.index
                            entry: modelData.members[0]; portion: parent.paired ? "upper" : "whole"
                            selected: root.regionsOpen && root.takesFocus && root.worlds.focusIndex===entry.index
                        }
                        WorldCard {
                            id: second; x: first.x; y: first.y; width: first.width; height: first.height
                            visible: parent.paired; entry: parent.paired ? modelData.members[1] : ({})
                            objectName: parent.paired ? "world-" + entry.index : ""
                            portion: "lower"
                            selected: visible && root.regionsOpen && root.takesFocus && root.worlds.focusIndex===entry.index
                        }
                        MouseArea {
                            anchors.fill: first
                            onClicked: event => root.shell.activate(parent.modelData.members[parent.paired && event.y > height*.60-event.x/width*height*.20 ? 1 : 0].index)
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
        PageHeader { id: adventuresHeader; title: root.worlds.region.name; subtitle: "Choose an Adventure" }
        MountedPanel {
            y: adventuresHeader.height; width: parent.width; height: parent.height - y; color: "#d4e2d6"
            LibraryWheel {
                objectName: "adventure-list"
                anchors.fill: parent
                entries: root.worlds.adventures; entry: root.worlds.detail; selectionIndex: root.worlds.adventureIndex
                takesFocus: root.takesFocus && root.listOpen
                previewsEnabled: root.shell.settings.videoPreviews && !root.shell.serviceOpen && !adventureLaunch.active && !sessionState.blocked
                wheelFocused: root.worlds.focusIndex < entries.length
                showBack: false; idsInNames: true; itemPrefix: "adventure-"
                emptyName: "world-list-back"; emptyTitle: "No matching Adventures"; emptyDetail: "Back to Worlds"
                actionLabel: "Open"; filterText: root.worlds.filterLabel + (root.worlds.query ? " · " + root.worlds.query : "")
                onActivated: index => root.shell.activate(index)
                onEmptyActivated: root.shell.activate(entries.length)
                onBackActivated: root.shell.activate(entries.length)
            }
        }
    }

    Item {
        id: detailView
        anchors.fill: parent; visible: root.detailOpen
        function revealAction() {
            if (!root.detailOpen || !root.takesFocus) return
            const control = detailActions.itemAt(root.worlds.focusIndex)
            if (control && control.visible && control.enabled) control.forceActiveFocus(Qt.OtherFocusReason)
        }
        Connections {
            target: root
            function onDetailOpenChanged() { Qt.callLater(detailView.revealAction) }
            function onTakesFocusChanged() { Qt.callLater(detailView.revealAction) }
        }
        PageHeader {
            id: detailHeader
            objectName: "world-detail-header"
            multilineTitle: true
            eyebrow: "WORLDS / " + root.worlds.region.name.toUpperCase()
            title: root.worlds.detail.title
            subtitle: root.worlds.detail.kind + " · " + (root.shell.sampleLibrary ? root.worlds.detail.status + " · sample data" : root.worlds.detail.platform + (root.worlds.detail.variant ? " · " + root.worlds.detail.variant : ""))
        }
        Text {
            x: 29; y: detailHeader.height + 14; width: 491; height: Math.min(102, 219-y)
            text: root.worlds.detail.limitation || root.worlds.detail.description || "Your own journey, ready to become part of the collection."; textFormat: Text.PlainText; wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight
            color: Theme.ink; font.pixelSize: 17
        }
        Row {
            x: 29; y: 231; spacing: 14
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
        MountedPanel {
            x: 568; y: 119; width: parent.width - x; height: 213; color: "#e1ecde"
            AdventureArtwork { id: detailArtwork; x: 20; y: 10; width: parent.width - 40; height: parent.height - 20; media: root.worlds.detail.artwork || ({}) }
            Text { visible: !detailArtwork.available; x: 23; y: 21; text: "YOUR NEXT STEP"; color: Theme.muted; font.pixelSize: 12; font.letterSpacing: 1 }
            Text { visible: !detailArtwork.available; x: 23; y: 54; width: parent.width - 46; text: root.worlds.detail.availability; wrapMode: Text.WordWrap; color: Theme.ink; font.pixelSize: 22; font.weight: Font.DemiBold }
            Text { visible: !detailArtwork.available; x: 23; y: 151; width: parent.width - 46; text: root.worlds.detail.resume; elide: Text.ElideRight; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 15 }
        }
        MountedPanel {
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
            height: 78; color: "#c6dcca"
            Row {
                x: 30; y: 18; spacing: 16
                Repeater {
                    id: detailActions
                    model: root.worlds.actions
                    onItemAdded: Qt.callLater(detailView.revealAction)
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
