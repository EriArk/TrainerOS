import QtQuick

Item {
    id: root
    required property var entry
    property bool selected: false
    property bool compact: false
    signal activated()
    Rectangle { x: 1; y: 3; width: parent.width-2; height: parent.height-3; radius: 12; color: Theme.chassis; opacity: .17 }
    Rectangle {
        anchors.fill: parent; anchors.bottomMargin: 3; radius: 12; color: Theme.paper
        border.width: root.selected ? 2 : 1; border.color: root.selected ? Theme.chassis : Qt.tint(Theme.paper,Theme.surfaceColor("#40516c60"))
        Rectangle {
            id: banner; x: 5; y: 5; width: parent.width-10; height: root.compact?parent.height*.28:parent.height*.47; radius: 8; clip: true
            gradient: Gradient { GradientStop { position: 0; color: Theme.chassis } GradientStop { position: 1; color: Theme.blue } }
            Rectangle { width: parent.width*.65; height: width; radius: width/2; x: parent.width*.65; y: -width*.4; color: Theme.paper; opacity: .15 }
            Rectangle { width: parent.width*.55; height: width; rotation: 35; x: -width*.35; y: parent.height*.5; color: Theme.yellow; opacity: .25 }
            Image { anchors.fill: parent; source: root.entry.banner || ""; fillMode: Image.PreserveAspectCrop; asynchronous: true }
        }
        AccountAvatar { x: 14; y: banner.y+banner.height-(root.compact?8:19); width: root.compact?27:39; source: root.entry.avatar || ""; name: root.entry.name || "" }
        Text { x: root.compact?49:62; y: banner.y+banner.height+6; width: parent.width-x-13; text: root.entry.name || "Community"; font.family: Theme.displayFamily; font.pixelSize: root.compact?14:18; color: Theme.ink; elide: Text.ElideRight; textFormat: Text.PlainText }
        Text { x: 14; y: banner.y+banner.height+(root.compact?29:35); width: parent.width-28; height: Math.max(12,parent.height-y-(root.compact?23:30)); text: root.entry.description || "A place to play and meet people."; font.pixelSize: root.compact?10:12; color: Theme.muted; wrapMode: Text.WordWrap; maximumLineCount: root.compact?1:2; elide: Text.ElideRight; textFormat: Text.PlainText }
        Text { x: 14; anchors.bottom: parent.bottom; anchors.bottomMargin: root.compact?7:11; text: (root.entry.members >= 0 ? root.entry.members + " members" : "Invitation") + (root.entry.language ? " · " + root.entry.language : ""); font.pixelSize: root.compact?9:11; color: Theme.muted; textFormat: Text.PlainText }
        Text { anchors { right: parent.right; bottom: parent.bottom; margins: 11 } text: root.entry.action === "Joined" ? "Joined" : "›"; color: Theme.ink; font.pixelSize: 14 }
    }
    MouseArea { anchors.fill: parent; onClicked: root.activated() }
}
