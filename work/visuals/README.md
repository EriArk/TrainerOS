# Early interface studies

These HTML fragments and the organizer behavior script preserve the exploratory design work that preceded the native Qt/QML shell. They are visual references, not a second application or a frozen design specification.

- `material-study.html`: material, contrast and attached-drawer study.
- `flip2-landscape.html`: fixed landscape composition and color exploration.
- `flip2-organizer.html` and `organizer-behavior.js`: organizer tabs, recessed screens, controller text entry and page interactions.

The fragments were developed in a preview host and can depend on its surrounding styles/controls. They are not part of the CMake build. The current product implementation lives in `src/qml/` and its C++ controllers; `docs/DESIGN_LANGUAGE.md` contains the confirmed direction.

Generated PNG captures and machine-specific checking scripts remain local and ignored by Git. Research downloads are also excluded; source citations are preserved in `docs/WORLDS_AND_PROGRESS_FEASIBILITY.md`.
