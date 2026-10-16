import QtQuick

Item {
    id: root
    required property var shell
    objectName: "gamehub"
    function hints(h) {
        if(shell.hubFace === "lobby") return shell.social.hints
        if(shell.adapterActive) {
            if(shell.adapterMenuOpen) return [h("↑↓","Screens"),h("A","Open"),h("B","Overview")]
            return adapterPresenter.hints(h) || [h("A","Open"),h("Select","Actions"),h("B","Screens")]
        }
        if(shell.hubFace === "ra") return [h("A","Open"),h("Select","Refresh"),h("B","Back")]
        return shell.overviewFace === "timeline" ? [h("←→","Timeline"),h("Select","Game actions"),h("B","Overview")]
            : [h("A","Open"),h("Select","Game actions"),h("B",shell.overviewFace === "summary" ? "Home" : "Overview")]
    }
    readonly property bool available: visible && !shell.serviceOpen && !shell.menuOpen
        && !shell.drawerOpen && !shell.quickAppsOpen && !shell.keyboard.open && !shell.notice.length
    GameHubOverview { anchors.fill: parent; anchors.margins: 10; shell: root.shell; visible: root.shell.hubFace === "overview" }
    HallOfFamePage { anchors.fill: parent; shell: root.shell; visible: root.shell.hubFace === "ra" }
    SocialPage { anchors.fill: parent; shell: root.shell; visible: root.shell.hubFace === "lobby" }
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
