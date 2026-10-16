import QtQuick
import QtQuick.Effects

ChassisDrawer {
    id: root
    required property var shell
    expanded: shell.drawerOpen
    legend: shell.sampleLibrary ? "RECENT GAMES · SAMPLE DATA" : "RECENT GAMES · PLAY FROM HOME"
    onToggled: shell.activate(1,"continue")
    Item {
        parent: root.contentItem
        anchors.fill: parent
        ListView {
            id: cards
            x: 5; y: 5; width: parent.width - 10; height: 158
            orientation: ListView.Horizontal
            interactive: false
            clip: true
            spacing: 14
            // Closed drawer has no delegates/images to rebuild on page changes.
            // Keep them through the closing animation while its contents show.
            model: visible ? root.shell.resumePoints : []
            currentIndex: root.expanded ? root.shell.focusIndex : 0
            function reveal() {
                positionViewAtIndex(currentIndex, ListView.Contain)
                const card = itemAtIndex(currentIndex)
                if (card) {
                    if (card.x - 4 < contentX) contentX = card.x - 4
                    else if (card.x + card.width + 4 > contentX + width) contentX = card.x + card.width + 4 - width
                    card.claimSelection()
                }
            }
            onCurrentIndexChanged: Qt.callLater(reveal)
            onCurrentItemChanged: Qt.callLater(reveal)
            onWidthChanged: Qt.callLater(reveal)
            onCountChanged: Qt.callLater(reveal)
            Connections { target: root; function onExpandedChanged() { Qt.callLater(cards.reveal) } }
            Connections { target: root.shell; function onChanged() { if (root.expanded) Qt.callLater(cards.reveal) } }
                delegate: Item {
                    required property int index
                    required property var modelData
                    function claimSelection() { resumeCap.requestFocus() }
                    width: (root.expandedWidth - 72) / 3; height: 158
                    CapButton {
                        id: resumeCap
                        objectName: "resume-" + index
                        deferredFocus: true
                        x: 4; y: 4; width: parent.width - 8; height: 150
                        tint: [Theme.green, Theme.blue, Theme.pink][index % 3]
                        selected: root.expanded && !root.shell.menuOpen && root.shell.notice.length === 0 && root.shell.focusIndex === index
                        onActivated: root.shell.activate(index)
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
                        Text { x: 12; y: 49; width: parent.width - 24; elide: Text.ElideRight; textFormat: Text.PlainText; text: modelData.world; color: Theme.ink; font.pixelSize: 18; font.bold: true }
                        Text { x: 12; y: 76; text: modelData.previewLabel; color: Theme.muted; font.pixelSize: 11 }
                        Item {
                            id: ribbon
                            x: 14; y: 8; width: parent.width - 28; height: 34
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
                                x: 10; y: 6; width: parent.width - 29; elide: Text.ElideRight; textFormat: Text.PlainText
                                text: modelData.title; color: Theme.ink; font.pixelSize: 17; font.bold: true
                            }
                        }
                        Item {
                            id: readingWell
                            x: 5; y: 103; width: parent.width - 10; height: parent.height - y - 5; clip: true
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
                            x: 12; y: 106; width: parent.width - 24; spacing: 3
                            Text { width: parent.width; elide: Text.ElideRight; textFormat: Text.PlainText; text: (modelData.location.length ? modelData.location + " · " : "") + modelData.time; color: Theme.ink; font.pixelSize: 11 }
                            Text { width: parent.width; elide: Text.ElideRight; textFormat: Text.PlainText; text: modelData.summary; color: Theme.muted; font.pixelSize: 12 }
                        }
                    }
                }
        }
        CapButton {
            objectName: "resume-empty"
            x: 5; y: 5
            visible: root.shell.resumePoints.length === 0
            width: 500; height: 105
            label: "No recent games yet"; detail: ""
            selected: visible && root.expanded && !root.shell.menuOpen && root.shell.notice.length === 0
            onActivated: root.shell.activate(0)
        }
    }
}
