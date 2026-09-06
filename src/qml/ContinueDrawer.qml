import QtQuick
import QtQuick.Shapes

Item {
    id: root
    required property var shell
    property real expandedWidth: 932
    readonly property bool expanded: shell.drawerOpen
    width: 338; height: 53
    onExpandedChanged: {
        opening.stop(); closing.stop();
        if (expanded) opening.start(); else closing.start();
    }
    SequentialAnimation {
        id: opening
        NumberAnimation { target: root; property: "width"; to: root.expandedWidth; duration: Theme.motion(130); easing.type: Easing.OutCubic }
        NumberAnimation { target: root; property: "height"; to: 229; duration: Theme.motion(180); easing.type: Easing.OutCubic }
    }
    SequentialAnimation {
        id: closing
        NumberAnimation { target: root; property: "height"; to: 53; duration: Theme.motion(140); easing.type: Easing.InCubic }
        NumberAnimation { target: root; property: "width"; to: 338; duration: Theme.motion(130); easing.type: Easing.OutCubic }
    }
    Shape {
        anchors.fill: parent
        ShapePath {
            strokeColor: Theme.rim; strokeWidth: 2; fillColor: Theme.chassis
            startX: 0; startY: root.height
            PathLine { x: 0; y: 9 }
            PathQuad { x: 9; y: 0; controlX: 0; controlY: 0 }
            PathLine { x: root.width - 36; y: 0 }
            PathLine { x: root.width; y: 37 }
            PathLine { x: root.width; y: root.height }
        }
    }
    CapButton {
        objectName: "continue-toggle"
        x: 12; y: 8; width: 284; height: 36
        label: "Y   Continue Adventure"; textSize: 17
        selected: !root.expanded && !root.shell.menuOpen && root.shell.notice.length === 0
        onActivated: root.shell.activate(0)
    }
    Text {
        x: 330; y: 18; text: root.shell.sampleLibrary ? "RECENT TRAILS · SAMPLE DATA" : "RECENT TRAILS"; color: "#d2e8d9"; font.pixelSize: 12
        visible: root.expanded && root.width > 700
    }
    Item {
        x: 14; y: 58; width: root.width - 28; height: Math.max(0, root.height - 64); clip: true
        visible: root.expanded || root.height > 60
        Row {
            x: 5; y: 5
            spacing: 14
            Repeater {
                model: root.shell.resumePoints
                delegate: CapButton {
                    required property int index
                    required property var modelData
                    objectName: "resume-" + index
                    width: (root.expandedWidth - 72) / 3; height: 150
                    tint: [Theme.green, Theme.blue, Theme.pink][index % 3]
                    selected: root.expanded && !root.shell.menuOpen && root.shell.notice.length === 0 && root.shell.focusIndex === index
                    onActivated: root.shell.activate(index)
                    Rectangle {
                        x: 8; y: 7; width: parent.width - 16; height: 60; radius: 6
                        color: Qt.darker(parent.tint, 1.5); clip: true
                        Repeater {
                            model: 4
                            delegate: Rectangle {
                                required property int index
                                x: 110 + index * 34; y: 16 + index * 8; width: 85; height: 85
                                radius: 25; rotation: 45; color: "#60e4edd5"
                            }
                        }
                        Text { x: 12; y: 10; text: modelData.world; color: "#fffef9"; font.pixelSize: 18; font.bold: true }
                        Text { x: 12; y: 35; text: "Preview placeholder"; color: "#f4f5ec"; font.pixelSize: 11 }
                    }
                    Column {
                        x: 12; y: 75; width: parent.width - 24; spacing: 4
                        Text { text: modelData.title; color: Theme.ink; font.pixelSize: 17; font.bold: true }
                        Text { text: modelData.location + " · " + modelData.time; color: Theme.ink; font.pixelSize: 11 }
                        Text { text: modelData.summary; color: Theme.muted; font.pixelSize: 12 }
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
