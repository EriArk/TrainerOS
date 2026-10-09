# Adapter implementation rules

- Own a versioned compatibility/asset manifest. Declare alternative match rules
  (OR), each with its required constraints (AND). ScreenScraper is one optional
  provider; support other catalogue IDs, explicit local identities, ROM hashes/
  internal codes or validated save evidence as appropriate. Never require scraping
  to play. Reject ambiguous matches; no generic-shell game lists or title guesses.
- Own art/sprite pack family schemas and entity mapping. Keep installed user packs
  in the adapter namespace, outside replaceable code. Declare only implemented
  extraction sources; exact ROM/save validation remains mandatory. Active compatible
  packs suppress extraction for their asset family. Preserve legacy private roots
  through an explicit compatibility path, never silently copy or bundle their art.

- Create every game experience inside `src/adapters/<id>/` from its first commit.
  Keep its descriptor, presenter, game-specific controllers, actions and navigation
  there. Shared helpers may be extracted only when genuinely reused.
- Implement `core/experience/ExperienceModule.h`. Register trusted modules in the
  composition factory and their sources/resources in CMake. Do not add game names,
  concrete module includes, widget registrations or controller branches to the
  generic shell, `Main.qml`, or `ExperienceHost.qml`.
- Home and both game slots must use the same selected owner/game/module context.
  A collection is not an adapter identity. Titles never grant capabilities.
- Keep UI presentation distinct from exact-build semantic readers/writers and
  emulator launch/netplay adapters. Reuse their existing services and authorization;
  no direct save writes, automatic runtime launch, or implicit voice consent.
- Preserve stable IDs and opaque per-module navigation. Cancel stale text/actions
  on owner/game/revision/module changes. Unsupported/disabled/incompatible modules
  fall back while retaining the selected game and its history/saves.
- Include a real-host integration test for registration, actions, state switching,
  fallback and stale callbacks. A descriptor-only test is insufficient. UI changes
  also require rendered checks and the applicable device delivery.
- These are compiled built-ins. Downloaded executable QML/native plugins, installable
  packages and their trust/version compatibility remain the separate #92 work.
