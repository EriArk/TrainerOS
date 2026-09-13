import QtQuick
import QtQuick.Shapes

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
                GradientStop { position: 0; color: Theme.chassisTop }
                GradientStop { position: 1; color: Theme.chassis }
            }
            startX: 1; startY: root.height
            PathLine { x: 1; y: -6 }
            PathLine { x: Theme.screenBounds.x; y: -6 }
            PathQuad { x: Theme.screenBounds.x + 6; y: 0; controlX: Theme.screenBounds.x; controlY: 0 }
            PathLine { x: root.width - 39; y: 0 }
            PathQuad { x: root.width - 32; y: 3; controlX: root.width - 36; controlY: 0 }
            PathLine { x: root.width - 3; y: 32 }
            PathQuad { x: root.width; y: 39; controlX: root.width; controlY: 35 }
            PathLine { x: root.width; y: root.height }
        }
        ShapePath {
            strokeColor: Theme.rim; strokeWidth: 2; fillColor: "transparent"
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
        Hint { x: 23; y: 15; button: "Y"; label: ""; tint: Qt.lighter(Theme.yellow, 1.2) }
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
                        objectName: "resume-" + index
                        x: 4; y: 4; width: parent.width - 8; height: 150
                        tint: [Theme.green, Theme.blue, Theme.pink][index % 3]
                        selected: root.expanded && !root.shell.menuOpen && root.shell.notice.length === 0 && root.shell.focusIndex === index
                        onActivated: root.shell.activate(index)
                        Item {
                            x: 5; y: 5; width: parent.width - 10; height: parent.height - 10; clip: true
                            Image {
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
                        Rectangle { x: 12; y: 62; width: parent.width - 24; height: 1; color: "#40718b79" }
                        Column {
                            x: 12; y: 75; width: parent.width - 24; spacing: 4
                            Text { width: parent.width; elide: Text.ElideRight; textFormat: Text.PlainText; text: modelData.title; color: Theme.ink; font.pixelSize: 17; font.bold: true }
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
