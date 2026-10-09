import QtQuick

// Trusted built-in package. The host resolves view keys, never title strings.
QtObject {
    id: root
    required property var shell
    readonly property var views: ({
        "pokemon-home": home,
        "pokemon-guide": guide,
        "pokemon-persona": persona,
        "pokemon-journey": progress,
        "pokemon-hall": progress
    })
    property Component home: HomePage { shell: root.shell }
    property Component guide: PokedexPage { shell: root.shell }
    property Component persona: TrainerPage { shell: root.shell }
    property Component progress: HallOfFamePage { shell: root.shell }
    property Component overlay: PokemonExperienceOverlay { shell: root.shell }
}
