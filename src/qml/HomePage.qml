import QtQuick

Item {
    id: root
    required property var shell
    readonly property var currentAdventure: shell.home
    readonly property bool hasParty: shell.party.activities.hasParty
    readonly property bool partyPlaying: visible && !shell.drawerOpen && !shell.menuOpen && !shell.serviceOpen && !shell.keyboard.open && !shell.notice.length
    clip: true
    Image {
        id: exitBackdrop
        objectName: "home-exit-picture"
        anchors.fill: parent
        source: root.currentAdventure.exitPreview
        asynchronous: false; cache: false
        fillMode: Image.PreserveAspectCrop
        opacity: 0.13
        sourceSize.width: 960; sourceSize.height: 540
    }
    Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 44
        visible: exitBackdrop.status === Image.Ready
        gradient: Gradient {
            GradientStop { position: 0; color: Theme.paper }
            GradientStop { position: 1; color: Qt.rgba(Theme.paper.r, Theme.paper.g, Theme.paper.b, 0) }
        }
    }
    Text {
        anchors { right: parent.right; bottom: parent.bottom; rightMargin: 20; bottomMargin: 9 }
        text: root.currentAdventure.exitPreviewLabel
        visible: exitBackdrop.status === Image.Ready && !root.hasParty
        color: Theme.muted; font.pixelSize: 10
    }
    Column {
        x: 32; y: root.hasParty ? 14 : 27; spacing: root.hasParty ? 5 : 9
        Text { text: (root.currentAdventure.hasTrainer ? "WELCOME BACK, " : "WELCOME, ") + root.currentAdventure.trainer; color: Theme.muted; font.pixelSize: 13; font.bold: true; font.letterSpacing: 2; width: 620; elide: Text.ElideRight; textFormat: Text.PlainText }
        Text { text: root.currentAdventure.world; width: 600; elide: Text.ElideRight; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: root.hasParty ? 54 : text.length > 14 ? 48 : 70; font.weight: Font.DemiBold }
        Text { text: root.currentAdventure.adventure; width: 600; elide: Text.ElideRight; textFormat: Text.PlainText; color: "#347561"; font.pixelSize: 24 }
        Text { visible: !root.hasParty; text: root.currentAdventure.adventureId.length ? "Your selected Adventure. Your next discovery." : "Your next adventure is waiting."; color: Theme.muted; font.pixelSize: 16 }
    }
    Text { x: 34; y: root.hasParty ? 160 : 219; width: 580; elide: Text.ElideRight; textFormat: Text.PlainText; text: root.currentAdventure.progressNote; color: Theme.muted; font.pixelSize: 12 }
    Row {
        x: 33; y: root.hasParty ? 185 : 250; spacing: 12; width: root.width - 244 - 66
        BadgeTray {
            objectName: "home-badge-tray"
            badgePrefix: "home-badge-"
            width: parent.width - 256; height: 92
            slots: root.currentAdventure.badgeSlots
            count: root.currentAdventure.badges
        }
        Repeater {
            model: [{number: root.currentAdventure.caught, label: "CAUGHT"}, {number: root.currentAdventure.recordedTime, label: "RECORDED TIME"}]
            delegate: Rectangle {
                required property var modelData
                width: 116; height: 92; radius: 13; color: "#edf2e7"; border.color: "#c9d8c7"
                Text { x: 13; y: 12; width: parent.width - 26; height: 38; verticalAlignment: Text.AlignVCenter; text: modelData.number; color: Theme.ink; font.pixelSize: text.length > 4 ? 22 : 34; font.bold: true; elide: Text.ElideRight }
                Text { x: 13; y: 65; text: modelData.label; color: Theme.muted; font.pixelSize: 10; font.letterSpacing: .4 }
            }
        }
    }
    Text { x: 34; y: root.hasParty ? 290 : 350; width: 610; elide: Text.ElideRight; textFormat: Text.PlainText; text: root.currentAdventure.milestone; color: Theme.muted; font.pixelSize: 15 }
    Item {
        anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
        width: 244
        Text { x: 23; y: 28; text: "READY WHEN YOU ARE"; color: Theme.muted; font.pixelSize: 12; font.letterSpacing: 1; font.bold: true }
        AdventureButton { x: 12; y: 57; width: 220; height: 220; shell: root.shell }
        Text { x: 22; y: root.hasParty ? 278 : 287; width: 200; text: root.currentAdventure.action; wrapMode: Text.WordWrap; color: Theme.ink; font.pixelSize: root.hasParty ? 22 : 26; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter }
    }
    HomeParty {
        objectName: "home-party"
        x: Theme.adventureCutoutWidth + 6; anchors.bottom: parent.bottom
        width: parent.width - x - 15; height: 88
        activity: root.shell.party.activities
        visible: root.hasParty; playing: root.partyPlaying
    }

}
