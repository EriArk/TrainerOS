"""Architecture gate: new game modules must not grow concrete shell dependencies."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]

class AdapterBoundaryTests(unittest.TestCase):
    def test_generic_host_has_no_concrete_game_controllers_or_view_registration(self):
        for relative in ("src/core/navigation/ShellController.h", "src/core/navigation/ShellController.cpp",
                         "src/qml/Main.qml", "src/qml/ExperienceHost.qml"):
            source = (ROOT / relative).read_text(encoding="utf-8-sig")
            for forbidden in (r'#include\s+"adapters/', r'PokemonExperience', r'PokedexController',
                              r'PartyPresentation', r'"pokemon-(?:home|guide|party|persona|journey|hall)"',
                              r'shell\.(?:party|pokedex|center|pokemonFace)\b'):
                self.assertIsNone(re.search(forbidden, source), (relative, forbidden))

    def test_universal_profile_has_no_game_editor_or_catalogue(self):
        for suffix in ("h", "cpp"):
            source = (ROOT / f"src/features/trainer/TrainerController.{suffix}").read_text(encoding="utf-8")
            self.assertNotRegex(source, r'SpeciesPicker|Pokedex|favoritePokemonId|PokemonPersona')

    def test_shared_navigation_does_not_choose_a_game_family(self):
        source = (ROOT / "src/core/experience/ExperienceNavigation.cpp").read_text(encoding="utf-8")
        self.assertNotRegex(source, r'pokemon|\.domain|\.title')

    def test_common_services_depend_on_provider_contracts(self):
        for directory in ("src/features/settings", "src/features/social", "src/features/halloffame"):
            for path in (ROOT / directory).glob("*.*"):
                if path.suffix in (".cpp", ".h"):
                    self.assertNotRegex(path.read_text(encoding="utf-8-sig"), r'#include\s+"adapters/')

if __name__ == "__main__":
    unittest.main()
