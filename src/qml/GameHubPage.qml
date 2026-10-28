import QtQuick

Item {
    id: root
    required property var shell
    objectName: "gamehub"
    function hints(h) {
        if(shell.adapterActive) {
            if(shell.adapterMenuOpen) return [h("\u2191\u2193","Screens"),h("A","Open"),h("B","Game spaces")]
            return adapterPresenter.hints(h) || [h("A","Open"),h("Select","Actions"),h("B","Screens")]
        }
        return [h("A","Choose game"),h("B","Home")]
    }
    readonly property bool available: visible && !shell.serviceOpen && !shell.menuOpen
        && !shell.drawerOpen && !shell.quickAppsOpen && !shell.keyboard.open && !shell.notice.length
    Column {
        anchors.centerIn: parent; width: Math.min(parent.width - 64, 580); spacing: 18
        visible: root.shell.hubFace === "spaces"
        Text { width: parent.width; text: "Game spaces"; font.family: Theme.displayFamily; font.pixelSize: 30; color: Theme.ink }
        Text { width: parent.width; text: root.shell.homeGame.title || "Choose your next game"; font.pixelSize: 20; color: Theme.ink; wrapMode: Text.WordWrap }
        Text { width: parent.width; text: root.shell.adapterMenu.length ? "Open this game's space using the tabs above." : "Dedicated game spaces appear here when a compatible adapter is available."; font.pixelSize: 14; color: Theme.muted; wrapMode: Text.WordWrap }
        Item {
            width: 220; height: 40
            Text { anchors.verticalCenter: parent.verticalCenter; text: "Choose a game  \u203a"; font.pixelSize: 16; color: Theme.ink }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 2; color: Theme.focusGlow }
            MouseArea { anchors.fill: parent; onClicked: root.shell.goToSection("games") }
        }
    }
    Item {
        anchors.fill: parent; visible: root.shell.adapterActive
        ListView {
            id: menu
            objectName: "adapter-screen-menu"
            x: 0; y: 0; width: Math.min(210, parent.width * .24)
            height: parent.height - (root.shell.chooseAdventureAvailable ? Theme.adventureCutoutHeight : 0)
            visible: root.shell.adapterMenuOpen
            model: root.shell.adapterMenu; spacing: 7; clip: true
            currentIndex: root.shell.adapterMenuFocus
            onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
            delegate: CapButton {
                required property var modelData
                required property int index
                objectName: "adapter-screen-" + modelData.id
                width: menu.width - 4; height: 37; textSize: 14; contentInset: 42
                label: modelData.label; tint: index % 3 === 0 ? Theme.blue : index % 3 === 1 ? Theme.green : Theme.pink
                selected: root.available && menu.visible && index === menu.currentIndex
                deferredFocus: true
                onActivated: root.shell.selectAdapterScreen(modelData.id)
                MenuGlyph { x: 11; y: 8; width: 23; height: 23; glyph: modelData.icon; ink: Theme.ink }
            }
        }
        Item {
            id: content
            x: menu.visible ? menu.width + 12 : 0; y: menu.visible ? 0 : 34
            width: parent.width - x; height: parent.height - y
            // Complex legacy workspaces expand when entered. Navigation remains
            // available through the visible screen selector and B at local root.
            enabled: root.available && !menu.visible
            clip: true
            ExperienceHost { id: adapterPresenter; anchors.fill: parent; shell: root.shell }
            ExperienceHost { anchors.fill: parent; shell: root.shell; overlay: true }
        }
        CapButton {
            objectName: "adapter-screens-button"
            visible: !menu.visible; x: 0; y: 0; width: 192; height: 27; textSize: 12
            label: "‹  Screens"; tint: Theme.blue; claimsFocus: false
            onActivated: root.shell.showAdapterMenu()
        }
    }
}
