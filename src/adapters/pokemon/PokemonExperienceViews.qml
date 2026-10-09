import QtQuick
import TrainerOS
import "ExperienceHints.js" as ExperienceHints

// Trusted built-in package. The host resolves view keys, never title strings.
Item {
    id: root
    required property var shell
    property bool overlay: false
    function hints(h) { return ExperienceHints.actions(shell,h) }
    readonly property string viewKey: shell.page === 0 ? shell.homeView : shell.experienceView
    Loader { anchors.fill: parent; sourceComponent: root.overlay ? (root.shell.experienceModel.centerFace ? overlayView : null) : root.views[root.viewKey] || null }
    readonly property var views: ({
        "pokemon-home": home,
        "pokemon-guide": guide,
        "pokemon-persona": persona,
        "pokemon-journey": progress,
        "pokemon-hall": progress,
        "achievements": progress
    })
    property Component home: HomePage { shell: root.shell }
    property Component guide: PokedexPage { shell: root.shell }
    property Component persona: TrainerPage { shell: root.shell }
    property Component progress: HallOfFamePage { shell: root.shell }
    property Component overlayView: PokemonExperienceOverlay { shell: root.shell }
}
