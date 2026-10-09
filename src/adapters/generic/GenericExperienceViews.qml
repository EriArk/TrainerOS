import QtQuick
import TrainerOS
Item {
    id: root
    required property var shell
    property bool overlay: false
    readonly property string key: shell.page === 0 ? shell.homeView : shell.experienceView
    function hints(h) {
        if (key === "game-details") return [h("A","Game actions"),h("Select","Actions"),h("B","Home")]
        if (key === "game-history") return [h("↑↓","Sessions"),h("Select","Game actions"),h("B","Home")]
        if (key === "achievements") return [h("Select","Refresh"),h("X","Account"),h("A","Open"),h("B","Back")]
        return null
    }
    Component { id: home; MultiverseHome { shell: root.shell } }
    Component { id: page; GenericExperiencePage { shell: root.shell } }
    Component { id: achievements; HallOfFamePage { shell: root.shell } }
    Loader { anchors.fill: parent; active: !root.overlay; sourceComponent: root.key === "game-home" ? home : root.key === "achievements" ? achievements : page }
}
