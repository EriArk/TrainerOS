# Game experience adapters

Each game family has one directory here. `pokemon/` owns its presentation and
controller orchestration, including the game persona; `generic/` supplies ordinary
game details, observed sessions and verified achievements. `BuiltinExperiences.cpp`
is the explicit application composition list.

The contract is [ExperienceModule](../core/experience/ExperienceModule.h).
It supplies a versioned descriptor, a compiled-resource presenter, Home projection,
local input/actions, focus/modal guards and opaque navigation state. The host owns
the five navigation slots, universal identity/profile, history, accounts, Start,
Options, actual runtime/session ownership and authorized service boundaries.

A new adapter adds its directory, a factory registration and CMake sources/resources.
It must not modify ShellController, Main.qml or ExperienceHost. Application-level
service composition can wire existing exact-build providers into its controllers;
the module cannot turn a visual descriptor into permission to modify saves.

`tests/fixtures/adapters/CounterExperience.h` exercises an independent module through
the actual shell without registering Pokemon. It is not a playable RPG/racing
adapter. Existing launch/netplay adapters under `integrations/adventure/` and exact
save services remain separate reusable mechanisms; this directory is their game
experience composition, not a replacement emulator or untrusted plugin loader.

Legacy database fields and navigation keys retain their identities during migration.
External installation, signing/trust and varied real-game semantic proof remain
under #92/#90. See [the accepted contract](../../docs/EXPANSION_174_178.md).

## Compatibility and adapter-owned assets

Every module implements `manifest()` using
[ExperienceManifest](../core/experience/ExperienceManifest.h). Its alternatives
are OR rules; each rule's platform and named evidence fields are AND constraints.
ScreenScraper is optional. Namespaced identities can come from any catalogue,
an explicit local catalogue/game identity, observed ROM codes/hashes, or a
game-specific validated save reader. Missing evidence fails that alternative only.
Title, path, artwork filenames and collection membership are not match rules.

The host considers all enabled compatible modules. Two matching nonlegacy
adapters cause generic fallback instead of choosing registration order. Explicit
stored legacy domain is a weaker migration fallback; it preserves existing
libraries (including their historical filename classification) but never proves
ROM/build/save compatibility. The same resolver serves Home, slots, Properties
and live Game Options. Catalogue matching cannot grant save capabilities.

[ExperienceProviders](../core/experience/ExperienceProviders.h) keeps shared
Social/Settings/archive code independent of concrete adapter controllers and art
readers. Runtime transport events work even without a native game-activity provider.
Module write/session guards are aggregated for Trainer switching and process
handoff, including inactive modules with unfinished protected operations.

Batocera discovery publishes cached identities separately from visible artwork.
ScreenScraper writes its source/game ID into the existing `game` XML attributes;
rescanning reads them. Adapter identity probes run on the discovery worker, with
no filesystem/network work in host navigation. The built-in Pokemon probe reads
only a bounded raw GBA header; archives and other unimplemented probes remain
unsupported by that route. Another alternative can still match them.

Each adapter declares its own art/sprite family profile, version and implemented
extraction sources. Installed user data is namespaced under
`<state>/adapters/<id>/packs/<family>/`, outside replaceable source/binaries.
Existing private roots are an explicit migration fallback. No copyrighted art is
bundled by this change. A future compatible active pack suppresses ROM/save
extraction for its family; an empty extraction list means no extractor exists.
The matcher can consume validated save/hash evidence, but it does not create a
universal save recognizer or download arbitrary catalogue mappings.
