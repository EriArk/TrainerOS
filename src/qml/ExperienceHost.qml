import QtQuick

Item {
    id: root
    required property var shell
    property bool overlay: false
    readonly property string viewKey: shell.page === 0 ? shell.homeView : shell.experienceView
    PokemonExperienceViews { id: pokemon; shell: root.shell }
    readonly property var sharedViews: ({
        "game-home": genericHome, "game-details": genericPage,
        "game-history": genericPage, "achievements": achievements
    })
    Component { id: genericHome; MultiverseHome { shell: root.shell } }
    Component { id: genericPage; GenericExperiencePage { shell: root.shell } }
    Component { id: achievements; HallOfFamePage { shell: root.shell } }
    Loader {
        anchors.fill: parent
        active: root.visible
        sourceComponent: root.overlay ? (root.shell.centerFace ? pokemon.overlay : null) : pokemon.views[root.viewKey] || root.sharedViews[root.viewKey] || null
    }
    SocialIconButton {
        anchors.right: parent.right; anchors.rightMargin: 12; y: 10
        visible: !root.overlay && root.shell.page === 0
        icon: "dots-three"; label: "Game actions"
        onClicked: root.shell.openContext("game",root.shell.currentAdventureId)
    }
}
