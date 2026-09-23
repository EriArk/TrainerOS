import QtQuick

Item {
    id: root
    required property var dex
    required property bool takesFocus
    readonly property var choices: dex.artChoices
    readonly property var choice: choices.length ? choices[Math.min(dex.focusIndex, choices.length - 1)] : ({})
    Rectangle { anchors.fill: parent; color: "#90103935" }
    MouseArea { anchors.fill: parent }
    MountedPanel {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 348; color: "#d7e5d4"
        Text { x: 26; y: 16; text: "Illustrations · " + root.dex.detail.name; color: Theme.ink; font.pixelSize: 24; font.bold: true }
        Text { x: 27; y: 50; text: root.dex.detail.form + "  ·  " + (root.choices.length ? (root.dex.focusIndex + 1) + " / " + root.choices.length : "No confirmed image"); color: Theme.muted; font.pixelSize: 14 }
        Rectangle {
            x: 24; y: 79; width: 262; height: 211; radius: 12; color: Theme.paper; border.color: "#9bb5a2"
            ClassicIllustration { x: 11; y: 5; width: 240; height: 200; art: root.choice; showLabel: true }
        }
        Text {
            x: 312; y: 86; width: parent.width - 340; height: 53
            text: root.choice.sourceName ? root.choice.sourceName.split("/").pop() : "This form keeps its own identity. An image from another form is not substituted."
            textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 18; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
        }
        Text { x: 312; y: 151; width: parent.width - 340; text: root.choice.credit || "Artwork is optional; all reference facts remain available."; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 14; wrapMode: Text.WordWrap }
        Text { x: 312; y: 188; width: parent.width - 340; text: root.choice.source || root.dex.artCoverage; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 12; wrapMode: Text.WrapAnywhere; maximumLineCount: 2; elide: Text.ElideRight }
        Text { x: 312; y: 232; width: parent.width - 340; text: [root.choice.review || "", root.choice.quality || ""].filter(Boolean).join(" · "); color: Theme.muted; font.pixelSize: 13; wrapMode: Text.WordWrap }
        CapButton {
            objectName: "dex-art-choice"
            x: 312; y: 280; width: 322; height: 47
            label: root.choices.length ? root.choice.current ? "Keep this illustration" : "Use this illustration" : "Back to entry"
            selected: root.takesFocus; tint: Theme.yellow
            onActivated: root.dex.activate(root.dex.focusIndex)
        }
    }
}
