import QtQuick
import QtQuick.Effects

ChassisDrawer {
    id: root
    required property var shell
    label: "Recent / ★"
    expanded: shell.drawerOpen
    expandedHeight: 162
    onToggled: shell.activate(1,"continue")
    Item {
        parent: root.contentItem
        anchors.fill: parent
        Row {
            id: sources
            anchors.horizontalCenter: parent.horizontalCenter
            height: 20; spacing: 12
            Text { text: "‹ L2"; color: Theme.rim; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
            Repeater {
                model: [{id: "recent", label: "Recent games"}, {id: "favorites", label: "★ Favorites"}]
                delegate: Item {
                    required property var modelData
                    objectName: "drawer-source-" + modelData.id
                    width: sourceText.implicitWidth + 8; height: 20
                    readonly property bool chosen: root.shell.drawerSource === modelData.id
                    Accessible.role: Accessible.Button; Accessible.name: modelData.label
                    Text {
                        id: sourceText; anchors.centerIn: parent; text: modelData.label
                        font.pixelSize: 12; font.weight: parent.chosen ? Font.DemiBold : Font.Normal
                        color: parent.chosen ? Theme.yellow : Theme.paper; opacity: parent.chosen ? 1 : 0.68
                    }
                    Rectangle { anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; width: sourceText.width; height: 1; color: Theme.yellow; visible: parent.chosen }
                    TapHandler { onTapped: root.shell.setDrawerSource(modelData.id) }
                }
            }
            Text { text: "R2 ›"; color: Theme.rim; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
        }
        ListView {
            id: cards
            objectName: "drawer-cards"
            x: 5; y: 23; width: parent.width - 10; height: 116
            orientation: ListView.Horizontal
            interactive: true
            clip: true
            spacing: 10
            // Closed drawer has no delegates/images to rebuild on page changes.
            // Keep them through the closing animation while its contents show.
            model: visible ? root.shell.drawerGames : []
            readonly property int selectionIndex: root.expanded ? root.shell.focusIndex : 0
            currentIndex: selectionIndex
            property bool restoring: false
            readonly property real stride: (width - 3 * spacing) / 4 + spacing
            function remember() { if(root.expanded && !restoring)root.shell.setDrawerScroll(Math.max(0, contentX) / stride) }
            function restore() {
                restoring = true
                Qt.callLater(function() {
                    cards.forceLayout()
                    if(root.shell.drawerScroll < 0) { restoring = false; reveal(); return }
                    contentX = Math.max(0, Math.min(Math.max(0, contentWidth - width), root.shell.drawerScroll * stride))
                    restoring = false
                    const card = itemAtIndex(selectionIndex)
                    if (card) card.claimSelection()
                })
            }
            function reveal() {
                if(restoring || !root.expanded || selectionIndex < 0 || selectionIndex >= count)return
                forceLayout()
                positionViewAtIndex(selectionIndex, ListView.Contain)
                const card = itemAtIndex(selectionIndex)
                if (card) {
                    if (card.x - 4 < contentX) contentX = card.x - 4
                    else if (card.x + card.width + 4 > contentX + width) contentX = card.x + card.width + 4 - width
                    card.claimSelection()
                }
                remember()
            }
            onSelectionIndexChanged: { if(!restoring)Qt.callLater(reveal) }
            onCurrentItemChanged: Qt.callLater(function() {
                const card = itemAtIndex(selectionIndex)
                if (root.expanded && card) card.claimSelection()
                else if (root.expanded && count > 0 && selectionIndex < 0) root.forceActiveFocus(Qt.OtherFocusReason)
            })
            onWidthChanged: restore()
            onCountChanged: restore()
            onMovementEnded: remember()
            Connections { target: root; function onExpandedChanged() { if(root.expanded)cards.restore() } }
            Connections { target: root.shell; function onDrawerSourceChanged() { cards.restore() } }
                delegate: Item {
                    required property int index
                    required property var modelData
                    function claimSelection() { resumeCap.requestFocus() }
                    width: (cards.width-3*cards.spacing)/4; height: 116
                    CapButton {
                        id: resumeCap
                        objectName: "resume-" + index
                        deferredFocus: true
                        x: 4; y: 4; width: parent.width - 8; height: 108
                        tint: [Theme.green, Theme.blue, Theme.pink][index % 3]
                        selected: root.expanded && !root.shell.menuOpen && root.shell.notice.length === 0 && root.shell.focusIndex === index
                        onActivated: root.shell.selectDrawerGame(modelData.id)
                        Item {
                            id: cardBackground
                            x: 5; y: 5; width: parent.width - 10; height: parent.height - 10; clip: true
                            Image {
                                id: cardImage
                                objectName: "resume-background-" + index
                                anchors.fill: parent; source: modelData.preview
                                asynchronous: false; cache: true
                                fillMode: Image.PreserveAspectCrop; opacity: 0.18
                                sourceSize: Qt.size(640, 400)
                            }
                            Repeater {
                                model: modelData.preview.length ? 0 : 4
                                delegate: Rectangle {
                                    required property int index
                                    x: 110 + index * 34; y: 16 + index * 8; width: 85; height: 85
                                    radius: 25; rotation: 45; color: "#35fffef9"
                                }
                            }
                        }
                        Text { x: 10; y: 37; width: parent.width - 24; elide: Text.ElideRight; textFormat: Text.PlainText; text: modelData.world; color: Theme.ink; font.pixelSize: 14; font.bold: true }
                        Text { x: 10; y: 56; width: parent.width-20; elide: Text.ElideRight; text: modelData.previewLabel; color: Theme.muted; font.pixelSize: 10 }
                        Item {
                            id: ribbon
                            x: 10; y: 7; width: parent.width - 20; height: 28
                            readonly property color tint: [Theme.yellow, Theme.pink, Theme.blue][index % 3]
                            Canvas {
                                id: ribbonArt
                                anchors.fill: parent
                                antialiasing: true
                                onWidthChanged: requestPaint()
                                onHeightChanged: requestPaint()
                                Connections { target: ribbon; function onTintChanged() { ribbonArt.requestPaint() } }
                                onPaint: {
                                    const ctx = getContext("2d");
                                    ctx.reset();
                                    const gradient = ctx.createLinearGradient(0, 0, 0, height);
                                    gradient.addColorStop(0, Qt.lighter(ribbon.tint, 1.13).toString());
                                    gradient.addColorStop(1, ribbon.tint.toString());
                                    ctx.beginPath(); ctx.moveTo(1, 1);
                                    ctx.lineTo(width - 1, 1); ctx.lineTo(width - 9, height / 2);
                                    ctx.lineTo(width - 1, height - 1); ctx.lineTo(1, height - 1);
                                    ctx.lineTo(4, height / 2); ctx.closePath();
                                    ctx.fillStyle = gradient; ctx.fill();
                                    ctx.strokeStyle = Qt.darker(ribbon.tint, 1.45).toString();
                                    ctx.lineWidth = 1; ctx.lineJoin = "round"; ctx.stroke();
                                }
                            }
                            Rectangle { x: 8; y: 2; width: parent.width - 23; height: 1; color: "#90fffef9" }
                            Text {
                                x: 9; y: 4; width: parent.width - 26; elide: Text.ElideRight; textFormat: Text.PlainText
                                text: modelData.title; color: Theme.ink; font.pixelSize: 14; font.bold: true
                            }
                        }
                        Item {
                            id: readingWell
                            x: 5; y: 71; width: parent.width - 10; height: parent.height - y - 5; clip: true
                            Rectangle { anchors.fill: parent; color: resumeCap.capTint }
                            Loader {
                                anchors.fill: parent
                                active: root.visible && root.expanded && cardImage.status === Image.Ready
                                        && GraphicsInfo.api !== GraphicsInfo.Software
                                        && GraphicsInfo.api !== GraphicsInfo.Unknown
                                        && GraphicsInfo.api !== GraphicsInfo.Null
                                sourceComponent: Item {
                                    ShaderEffectSource {
                                        id: croppedPreview
                                        width: parent.width; height: parent.height
                                        sourceItem: cardBackground
                                        sourceRect: Qt.rect(0, readingWell.y - cardBackground.y, readingWell.width, readingWell.height)
                                        textureSize: Qt.size(Math.ceil(readingWell.width), Math.ceil(readingWell.height))
                                        live: false; visible: false
                                        Component.onCompleted: scheduleUpdate()
                                    }
                                    MultiEffect {
                                        anchors.fill: parent; source: croppedPreview
                                        blurEnabled: true; blurMax: 8; blur: 0.65; autoPaddingEnabled: false
                                    }
                                }
                            }
                            Rectangle {
                                anchors.fill: parent
                                gradient: Gradient {
                                    GradientStop { position: 0; color: "#70fffef9" }
                                    GradientStop { position: 0.2; color: "#a0fffef9" }
                                    GradientStop { position: 1; color: "#b0fffef9" }
                                }
                            }
                        }
                        Column {
                            x: 10; y: 73; width: parent.width - 20; spacing: 0
                            Text { width: parent.width; elide: Text.ElideRight; textFormat: Text.PlainText; text: (modelData.location.length ? modelData.location + " · " : "") + modelData.time; color: Theme.ink; font.pixelSize: 10 }
                            Text { width: parent.width; elide: Text.ElideRight; textFormat: Text.PlainText; text: modelData.summary; color: Theme.muted; font.pixelSize: 10 }
                        }
                    }
                }
        }
        Item {
            id: emptyState
            objectName: "resume-empty"
            x: 5; y: 27; width: parent.width-10; height: 105
            visible: root.shell.drawerGames.length === 0
            readonly property bool selected: visible && root.expanded && !root.shell.menuOpen && root.shell.notice.length === 0
            onSelectedChanged: if(selected)Qt.callLater(function() { if(selected)forceActiveFocus(Qt.OtherFocusReason) })
            Accessible.role: Accessible.StaticText
            Accessible.name: emptyTitle.text
            Row {
                anchors.centerIn: parent; spacing: 14
                Text { visible: root.shell.drawerSource === "favorites"; text: "☆"; color: Theme.yellow; font.pixelSize: 36; anchors.verticalCenter: parent.verticalCenter }
                Column {
                    spacing: 4
                    Text { id: emptyTitle; text: root.shell.drawerSource === "favorites" ? "Your favorites belong here" : "No recent games yet"; color: Theme.paper; font.pixelSize: 16 }
                    Text { visible: root.shell.drawerSource === "favorites"; text: "Favorite games from across your library"; color: Theme.paper; opacity: .65; font.pixelSize: 12 }
                }
            }
        }
    }
}
