import QtQuick

Item {
    id: root
    required property var dex
    required property bool takesFocus
    readonly property var choices: dex.spriteChoices
    readonly property var choice: choices.length ? choices[Math.min(dex.focusIndex, choices.length - 1)] : ({})
    Rectangle { anchors.fill: parent; color: "#90103935" }
    MouseArea { anchors.fill: parent }
    MountedPanel {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 348; color: "#d7e5d4"
        Text { x: 26; y: 16; text: "Sprites & portraits · " + root.dex.detail.name; color: Theme.ink; font.pixelSize: 24; font.bold: true }
        Text { x: 27; y: 51; text: root.dex.detail.form + "  ·  " + (root.choices.length ? (root.dex.focusIndex + 1) + " / " + root.choices.length : "Not available"); color: Theme.muted; font.pixelSize: 14 }
        Rectangle {
            x: 24; y: 79; width: 262; height: 211; radius: 12; color: Theme.paper; border.color: "#9bb5a2"
            SpritePreview {
                id: picture; objectName: "dex-sprite-image"
                anchors.centerIn: parent; width: 192; height: 184
                asset: root.choice; playing: root.takesFocus
            }
            Text {
                anchors.centerIn: parent; width: 224
                visible: !picture.ready
                text: root.choices.length ? "Image unavailable or loading" : "No confirmed image"
                wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; color: Theme.muted; font.pixelSize: 16
            }
        }
        Text { x: 312; y: 83; width: parent.width - 340; text: root.choice.label || root.dex.spriteStatus; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 21; wrapMode: Text.WordWrap }
        Text { x: 312; y: 122; width: parent.width - 340; text: root.choice.credit || "This exact form keeps its own identity. Another form or a recolor is never substituted."; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 14; wrapMode: Text.WordWrap }
        Text { x: 312; y: 189; width: parent.width - 340; text: root.choice.license || "Reference facts and your journal remain available."; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 12; wrapMode: Text.WordWrap }
        Text { x: 312; y: 228; width: parent.width - 340; text: root.choice.source || "Optional detail artwork · separate from illustrations"; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 11; wrapMode: Text.WrapAnywhere; maximumLineCount: 3; elide: Text.ElideRight }
        CapButton {
            objectName: "dex-sprite-back"
            x: 312; y: 290; width: 322; height: 43; label: "A / B   Back to entry"
            selected: root.takesFocus; tint: Theme.yellow; onActivated: root.dex.activate(root.dex.focusIndex)
        }
        Text { x: 28; y: 312; text: "← / →  Sprite & emotions"; visible: root.choices.length > 1; color: Theme.muted; font.pixelSize: 14 }
    }
}
