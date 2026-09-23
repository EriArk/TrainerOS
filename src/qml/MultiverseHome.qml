import QtQuick

Item {
    id: root
    required property var shell
    readonly property var choice: shell.multiverse.selected
    Rectangle {
        anchors.fill: parent; color: "#202c43"
        clip: true
        Image { anchors.fill: parent; source: root.choice.preview || ""; fillMode: Image.PreserveAspectCrop; opacity: 0.22; asynchronous: true }
        Repeater {
            model: 4
            Rectangle {
                required property int index
                x: -135 - index * 38; y: 50 - index * 38
                width: 390 + index * 76; height: width; radius: width / 2
                color: "transparent"; border.color: "#3b4962"; border.width: 2
            }
        }
        Rectangle { x: 0; y: 0; width: parent.width; height: 4; color: "#101a2b" }
    }
    Column {
        x: 30; y: 26; spacing: 17; width: parent.width - 300
        Text { text: "BEYOND THE REGIONS"; color: "#b1bbd1"; font.pixelSize: 13; font.letterSpacing: 2; font.bold: true }
        Text { text: "Multiverse"; color: "#f4ecdc"; font.pixelSize: 38; font.bold: true }
        Rectangle { width: parent.width; height: 3; color: "#7886a6" }
        Text { width: parent.width; text: root.choice.title || "A new horizon"; color: "#ffe0a0"; font.pixelSize: 27; font.bold: true; elide: Text.ElideRight }
        Text { text: root.choice.system || "Your library across systems"; color: "#d5dded"; font.pixelSize: 19 }
        Text { width: parent.width; text: root.shell.multiverse.sample ? "Development preview\nFictional titles · no recorded play history" : root.choice.id ? root.choice.time || "Ready for a new story" : "Choose your next Adventure in Multiverse."; color: "#c2cce0"; font.pixelSize: 17; lineHeight: 1.4; wrapMode: Text.WordWrap }
    }
    Item {
        anchors.right: parent.right; anchors.rightMargin: 16; width: 244; height: parent.height
        Text { y: 28; width: parent.width; horizontalAlignment: Text.AlignHCenter; text: root.choice.id ? "YOUR NEXT ADVENTURE" : "FIND YOUR NEXT STORY"; color: "#d5dded"; font.pixelSize: 12; font.bold: true }
        AdventureButton { objectName: "multiverse-launch"; x: 12; y: 55; width: 220; height: 220; shell: root.shell; selected: root.visible && root.shell.chooseAdventureAvailable && !root.shell.drawerOpen }
        Text { y: 287; width: parent.width; horizontalAlignment: Text.AlignHCenter; text: root.choice.id ? root.shell.multiverse.sample ? "Development preview" : root.choice.action : "Explore Multiverse"; color: "#f4ecdc"; font.pixelSize: 20; font.bold: true }
    }
}
