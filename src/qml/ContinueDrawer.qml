import QtQuick
import QtQuick.Shapes
import QtQuick.Effects

Item {
    id: root
    required property var shell
    property real expandedWidth: Theme.viewportWidth - Theme.screenBounds.x
    readonly property real closedWidth: 292
    readonly property bool expanded: shell.drawerOpen
    width: closedWidth; height: 53
    onExpandedChanged: {
        opening.stop(); closing.stop();
        if (expanded) opening.start(); else closing.start();
    }
    SequentialAnimation {
        id: opening
        NumberAnimation { target: root; property: "width"; to: root.expandedWidth; duration: Theme.motion(180); easing.type: Easing.InOutCubic }
        NumberAnimation { target: root; property: "height"; to: 229; duration: Theme.motion(220); easing.type: Easing.OutCubic }
    }
    SequentialAnimation {
        id: closing
        NumberAnimation { target: root; property: "height"; to: 53; duration: Theme.motion(170); easing.type: Easing.InOutCubic }
        NumberAnimation { target: root; property: "width"; to: root.closedWidth; duration: Theme.motion(160); easing.type: Easing.OutCubic }
    }
    Shape {
        anchors.fill: parent
        ShapePath {
            // Fill joins both the sidewall and footer; neither gets a seam.
            strokeColor: "transparent"
            fillGradient: LinearGradient {
                x1: 0; y1: -root.y; x2: 0; y2: Theme.viewportHeight - root.y
                GradientStop { position: 0; color: Theme.chassisCrown }
                GradientStop { position: 0.035; color: Theme.chassisTop }
                GradientStop { position: 0.24; color: Theme.chassisTop }
                GradientStop { position: 0.75; color: Theme.chassis }
                GradientStop { position: 0.94; color: Theme.chassis }
                GradientStop { position: 1; color: Theme.chassisFoot }
            }
            startX: 4; startY: root.height
            PathLine { x: 4; y: -6 }
            PathLine { x: Theme.screenBounds.x; y: -6 }
            PathQuad { x: Theme.screenBounds.x + 6; y: 0; controlX: Theme.screenBounds.x; controlY: 0 }
            PathLine { x: root.width - 39; y: 0 }
            PathQuad { x: root.width - 32; y: 3; controlX: root.width - 36; controlY: 0 }
            PathLine { x: root.width - 3; y: 32 }
            PathQuad { x: root.width; y: 39; controlX: root.width; controlY: 35 }
            PathLine { x: root.width; y: root.height }
        }
        ShapePath {
            strokeColor: Theme.edgeLight; strokeWidth: 2; fillColor: "transparent"
            startX: Theme.screenBounds.x; startY: -6
            PathQuad { x: Theme.screenBounds.x + 6; y: 0; controlX: Theme.screenBounds.x; controlY: 0 }
            PathLine { x: root.width - 39; y: 0 }
            PathQuad { x: root.width - 32; y: 3; controlX: root.width - 36; controlY: 0 }
            PathLine { x: root.width - 3; y: 32 }
            PathQuad { x: root.width; y: 39; controlX: root.width; controlY: 35 }
            PathLine { x: root.width; y: root.height - 5 }
        }
    }
    Item {
        id: toggle
        objectName: "continue-toggle"
        width: root.closedWidth; height: 53
        Accessible.role: Accessible.Button; Accessible.name: "Y Continue Adventure"
        // An inset follows the outer slope instead of a rectangular cap
        // floating on top of it. It stays seated while the body unfolds.
        Shape {
            anchors.fill: parent
            ShapePath {
                strokeColor: "#966c28"; strokeWidth: 1
                fillGradient: LinearGradient {
                    x1: 0; y1: 8; x2: 0; y2: 46
                    GradientStop { position: 0; color: Qt.lighter(Theme.yellow, 1.2) }
                    GradientStop { position: 1; color: Theme.yellow }
                }
                startX: 18; startY: 8
                PathLine { x: toggle.width - 44; y: 8 }
                PathQuad { x: toggle.width - 37; y: 11; controlX: toggle.width - 40; controlY: 8 }
                PathLine { x: toggle.width - 14; y: 34 }
                PathQuad { x: toggle.width - 11; y: 41; controlX: toggle.width - 11; controlY: 37 }
                PathQuad { x: toggle.width - 16; y: 46; controlX: toggle.width - 11; controlY: 46 }
                PathLine { x: 18; y: 46 }
                PathQuad { x: 12; y: 40; controlX: 12; controlY: 46 }
                PathLine { x: 12; y: 14 }
                PathQuad { x: 18; y: 8; controlX: 12; controlY: 8 }
            }
            ShapePath {
                strokeColor: "#fff0b5"; strokeWidth: 1; fillColor: "transparent"
                startX: 16; startY: 37
                PathLine { x: 16; y: 16 }
                PathQuad { x: 20; y: 12; controlX: 16; controlY: 12 }
                PathLine { x: toggle.width - 45; y: 12 }
            }
        }
        Text { x: 24; y: 12; text: "Y"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 21; font.weight: Font.DemiBold }
        Text { x: 58; y: 16; text: "Continue Adventure"; color: "#fff0b5"; font.pixelSize: 17; font.weight: Font.DemiBold }
        Text { x: 58; y: 15; text: "Continue Adventure"; color: Theme.ink; font.pixelSize: 17; font.weight: Font.DemiBold }
        MouseArea { anchors.fill: parent; onClicked: root.shell.activate(1, "continue") }
    }
    Text {
        x: 306; y: 18; text: root.shell.sampleLibrary ? "CHOOSE FOR HOME · SAMPLE DATA" : "CHOOSE FOR HOME · START FROM THE BIG BUTTON"; color: "#d2e8d9"; font.pixelSize: 12
        opacity: root.expanded && root.width > 700 ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.motion(100) } }
    }
    Item {
        x: 14; y: 58; width: root.width - 28; height: Math.max(0, root.height - 64); clip: true
        visible: root.expanded || root.height > 60
        ListView {
            id: cards
            x: 5; y: 5; width: parent.width - 10; height: 158
            orientation: ListView.Horizontal
            interactive: false
            clip: true
            spacing: 14
            model: root.shell.resumePoints
            currentIndex: root.expanded ? root.shell.focusIndex : 0
            function reveal() {
                positionViewAtIndex(currentIndex, ListView.Contain)
                const card = itemAtIndex(currentIndex)
                if (card) {
                    if (card.x - 4 < contentX) contentX = card.x - 4
                    else if (card.x + card.width + 4 > contentX + width) contentX = card.x + card.width + 4 - width
                }
            }
            onCurrentIndexChanged: Qt.callLater(reveal)
            onWidthChanged: Qt.callLater(reveal)
            onCountChanged: Qt.callLater(reveal)
            Connections { target: root; function onExpandedChanged() { Qt.callLater(cards.reveal) } }
                delegate: Item {
                    required property int index
                    required property var modelData
                    width: (root.expandedWidth - 72) / 3; height: 158
                    CapButton {
                        id: resumeCap
                        objectName: "resume-" + index
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
                        Text { x: 12; y: 13; width: parent.width - 24; elide: Text.ElideRight; textFormat: Text.PlainText; text: modelData.world; color: Theme.ink; font.pixelSize: 18; font.bold: true }
                        Text { x: 12; y: 38; text: modelData.previewLabel; color: Theme.muted; font.pixelSize: 11 }
                        Item {
                            id: ribbon
                            x: 6; y: 63; width: parent.width - 12; height: 34
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
            label: "No recent Adventures yet"; detail: "A / B · Close"
            selected: visible && root.expanded && !root.shell.menuOpen && root.shell.notice.length === 0
            onActivated: root.shell.activate(0)
        }
    }
}
