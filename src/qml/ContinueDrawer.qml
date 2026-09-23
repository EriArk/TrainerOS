import QtQuick
import QtQuick.Shapes
import QtQuick.Effects

Item {
    id: root
    required property var shell
    // Translate the complete drawer; its expanded right end still meets the bevel.
    property real expandedWidth: Theme.screenBounds.x + Theme.screenBounds.width - Theme.screenBevel - x
    readonly property real closedWidth: 292
    readonly property bool expanded: shell.drawerOpen
    readonly property real leftJoin: Theme.screenBounds.x + Theme.screenBevel
    readonly property real lowerJoin: 53 - (Theme.footerTop - Theme.screenBounds.y - Theme.screenBounds.height + Theme.screenBevel)
    function shoulder() {
        // The shallow left cut seats against the body, over the screen bevel.
        // The expanded right diagonal still ends directly at the screen lip.
        return "M " + leftJoin + " " + lowerJoin + " L " + (leftJoin + 6) + " 6"
            + " Q " + (leftJoin + 7) + " 0 " + (leftJoin + 11) + " 0"
            + " H " + (width - 45)
            + " Q " + (width - 42) + " 0 " + (width - 38) + " 3"
            + " L " + width + " " + lowerJoin;
    }
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
    // Fade the existing bevel into the contact shadow at each mounting point.
    // These strips stay on the bevel itself, before the drawer's face is drawn.
    Rectangle {
        x: Theme.screenBounds.x - root.x; y: root.lowerJoin - 24; width: Theme.screenBevel; height: 24
        opacity: 1 - Math.max(0, Math.min(1, (root.height - 53) / 18))
        gradient: Gradient {
            GradientStop { position: 0; color: "#00101e1d" }
            GradientStop { position: 1; color: "#98101e1d" }
        }
    }
    Rectangle {
        x: root.width; y: root.lowerJoin
        width: Math.min(20, Math.max(0, root.expandedWidth - root.width)); height: Theme.screenBevel
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: "#88101e1d" }
            GradientStop { position: 1; color: "#00101e1d" }
        }
    }
    Repeater {
        model: 2
        delegate: Item {
            id: sideContact
            required property int index
            x: index === 0 ? Theme.screenBounds.x - root.x : root.expandedWidth; y: root.lowerJoin - 18
            width: Theme.screenBevel
            height: Math.max(18, root.height - (53 - root.lowerJoin) - y)
            opacity: Math.max(0, Math.min(1, (root.height - 53) / 18))
            Repeater {
                model: 3
                delegate: Rectangle {
                    required property int index
                    x: index * sideContact.width / 3; width: sideContact.width / 3; height: sideContact.height
                    opacity: sideContact.index === 0 ? [0.2, 0.55, 1][index] : [1, 0.55, 0.2][index]
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#00101e1d" }
                        GradientStop { position: Math.min(1, 18 / sideContact.height); color: "#88101e1d" }
                        GradientStop { position: 1; color: "#88101e1d" }
                    }
                }
            }
        }
    }
    Shape {
        anchors.fill: parent
        // A dark contact fades through wider, lighter contour bands.
        // The filled body covers the inner halves; there is no lower seam.
        ShapePath {
            strokeColor: "#08101e1d"; strokeWidth: 22; fillColor: "transparent"
            capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.shoulder() }
        }
        ShapePath {
            strokeColor: "#0c101e1d"; strokeWidth: 18; fillColor: "transparent"
            capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.shoulder() }
        }
        ShapePath {
            strokeColor: "#14101e1d"; strokeWidth: 14; fillColor: "transparent"
            capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.shoulder() }
        }
        ShapePath {
            strokeColor: "#24101e1d"; strokeWidth: 10; fillColor: "transparent"
            capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.shoulder() }
        }
        ShapePath {
            strokeColor: "#44101e1d"; strokeWidth: 6; fillColor: "transparent"
            capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.shoulder() }
        }
        ShapePath {
            strokeColor: "#70101e1d"; strokeWidth: 3; fillColor: "transparent"
            capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.shoulder() }
        }
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
            PathSvg {
                path: root.shoulder() + " V " + root.height + " H " + root.leftJoin + " Z"
            }
        }
    }
    Item {
        id: toggle
        objectName: "continue-toggle"
        width: root.width; height: 53
        Accessible.role: Accessible.Button; Accessible.name: "Y Choose Adventure"
        // An inset follows the outer slope instead of a rectangular cap
        // floating on top of it. Its width follows the unfolding body.
        Shape {
            anchors.fill: parent
            ShapePath {
                strokeColor: "#966c28"; strokeWidth: 1
                fillGradient: LinearGradient {
                    x1: 0; y1: 8; x2: 0; y2: 46
                    GradientStop { position: 0; color: Qt.lighter(Theme.yellow, 1.2) }
                    GradientStop { position: 1; color: Theme.yellow }
                }
                startX: root.leftJoin + 20; startY: 8
                PathLine { x: toggle.width - 44; y: 8 }
                PathQuad { x: toggle.width - 37; y: 11; controlX: toggle.width - 40; controlY: 8 }
                PathLine { x: toggle.width - 14; y: 34 }
                PathQuad { x: toggle.width - 11; y: 41; controlX: toggle.width - 11; controlY: 37 }
                PathQuad { x: toggle.width - 16; y: 46; controlX: toggle.width - 11; controlY: 46 }
                PathLine { x: root.leftJoin + 13; y: 46 }
                PathQuad { x: root.leftJoin + 7; y: 40; controlX: root.leftJoin + 7; controlY: 46 }
                PathLine { x: root.leftJoin + 13; y: 14 }
                PathQuad { x: root.leftJoin + 20; y: 8; controlX: root.leftJoin + 14; controlY: 8 }
            }
        }
        Text { x: 44; y: 12; text: "Y"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 21; font.weight: Font.DemiBold }
        Text { x: 66; y: 15; text: "Choose Adventure"; color: Theme.ink; font.pixelSize: 17; font.weight: Font.DemiBold }
        Item {
            x: 306; y: 18; width: Math.max(0, toggle.width - x - 44); height: 20; clip: true
            // The legend is printed at its final position on the yellow inset.
            // Widening reveals it; neither the text nor its opacity animates.
            Text {
                width: Math.max(0, root.expandedWidth - parent.x - 44)
                horizontalAlignment: Text.AlignRight
                text: root.shell.sampleLibrary ? "CHOOSE ADVENTURE · SAMPLE DATA" : "CHOOSE ADVENTURE · PLAY FROM HOME"
                color: Theme.ink; font.pixelSize: 12
            }
        }
        MouseArea { anchors.fill: parent; onClicked: root.shell.activate(1, "continue") }
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
                                asynchronous: false; cache: false
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
            label: root.shell.page === 0 && root.shell.multiverseHome ? "No Multiverse choices yet" : "No recent Adventures yet"; detail: ""
            selected: visible && root.expanded && !root.shell.menuOpen && root.shell.notice.length === 0
            onActivated: root.shell.activate(0)
        }
    }
}
