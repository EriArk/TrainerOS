import QtQuick
import QtQuick.Shapes

Item {
    id: root
    required property var shell
    readonly property var keyboard: shell.keyboard
    readonly property bool takesFocus: keyboard.open && !shell.menuOpen && shell.notice.length === 0
    visible: keyboard.open
    Rectangle { anchors.fill: parent; color: "#65102927" }
    MouseArea { anchors.fill: parent }
    Item {
        id: tray
        width: parent.width; height: 326
        anchors.bottom: parent.bottom
        // The tray remains joined to the lower frame throughout its short motion.
        property real reveal: root.visible ? 1 : 0
        transform: Translate { y: (1 - tray.reveal) * 38 }
        Behavior on reveal { NumberAnimation { duration: Theme.motion(130); easing.type: Easing.OutCubic } }
        Shape {
            anchors.fill: parent
            ShapePath {
                strokeColor: Theme.rim; strokeWidth: 2; fillColor: Theme.chassis
                startX: 0; startY: tray.height
                PathLine { x: 0; y: 12 }
                PathQuad { x: 12; y: 0; controlX: 0; controlY: 0 }
                PathLine { x: tray.width - 36; y: 0 }
                PathLine { x: tray.width; y: 34 }
                PathLine { x: tray.width; y: tray.height }
            }
        }
        Text {
            x: 22; y: 12; text: root.keyboard.title.toUpperCase()
            color: "#e5eee1"; font.pixelSize: 13; font.bold: true; font.letterSpacing: 1
        }
        Text {
            anchors { right: parent.right; rightMargin: 48 }
            y: 12
            text: root.keyboard.count + " / " + root.keyboard.maximumLength
            color: "#e5eee1"; font.pixelSize: 13
        }
        Panel {
            x: 12; y: 36; width: parent.width - 24; height: 62
            Text {
                objectName: "keyboard-text"
                anchors { left: parent.left; right: parent.right; margins: 21; verticalCenter: parent.verticalCenter }
                text: root.keyboard.displayText.length ? root.keyboard.displayText + "│" : "Choose characters…"
                color: root.keyboard.displayText.length ? Theme.ink : Theme.muted
                font.pixelSize: 24; elide: Text.ElideLeft; textFormat: Text.PlainText
            }
        }
        Rectangle { x: 12; y: 105; width: 637; height: tray.height - y; radius: 8; color: "#276b66"; border.color: "#488d7e" }
        Rectangle { x: 663; y: 105; width: tray.width - x - 12; height: tray.height - y; radius: 8; color: "#234f5b"; border.color: "#59929a" }
        Repeater {
            model: root.keyboard.keys
            delegate: CapButton {
                required property int index
                required property var modelData
                objectName: "key-" + modelData.id
                visible: modelData.visible
                x: modelData.numeric ? 677 + (modelData.column - 11) * 79 : 24 + modelData.column * 61
                y: 117 + modelData.row * 49
                width: modelData.span * (modelData.numeric ? 79 : 61) - 10
                height: 38
                label: modelData.label; textSize: 18; centered: true
                tint: modelData.numeric ? Theme.blue
                    : modelData.id === "delete" || modelData.id === "clear" ? Theme.pink
                    : modelData.id === "apply" ? Theme.yellow
                    : modelData.id === "space" ? Theme.green : "#f0edda"
                focusColor: "#fff3b4"
                selected: root.takesFocus && root.keyboard.focusIndex === index
                onActivated: root.shell.activate(index)
            }
        }
        Text {
            x: 25; y: 307; width: 865
            text: root.keyboard.hint.length ? root.keyboard.hint : "A · Type    B · Cancel    " + root.keyboard.layoutHint + "    Apply · Use text"
            color: root.keyboard.hint.length ? "#ffe08c" : "#d4e4d8"
            font.pixelSize: 11; elide: Text.ElideRight
        }
    }
}
