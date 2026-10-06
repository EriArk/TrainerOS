# Contributing to TrainerOS

TrainerOS is a native C++20 / Qt Quick handheld shell for ArmadaOS. Start with
[development setup](docs/DEVELOPMENT.md), the [documentation index](docs/README.md)
and [current work](docs/CURRENT_TASKS.md).

## Discuss the scope

Check existing issues before opening one. For larger changes, discuss the player
experience and integration boundary first. Keep normal game launch direct,
controller navigation consistent, and existing libraries/saves intact. Avoid
adding setup screens or nested confirmations to ordinary play.

The license for TrainerOS-owned code has not yet been selected. Public source
availability is not a general redistribution grant. No blanket relicensing or
contributor agreement is implied here; preserve all existing third-party notices.
Please discuss substantial contributions with the maintainer while this is resolved.

## Submit a useful change

1. Work on a focused branch and explain the concrete user-visible result.
2. Follow the existing QML/service/adapter boundaries; keep platform commands and
   persistence out of feature presentation.
3. Use isolated development data. Never commit ROMs, BIOS/keys, personal saves,
   credentials, private acquisition links, downloaded art packs or support dumps.
4. Run checks relevant to the change and describe their environment. Documentation
   changes need link/content checks. Do not claim device behavior from host tests.
5. Include screenshots for visual changes; identify host renders versus actual
   handheld captures and avoid publishing private data or unlicensed asset packs.
6. Update affected docs/acceptance and open a PR explaining behavior, verification
   and remaining limitations. Keep generated builds and logs outside the patch.

GitHub Actions is deliberately disabled. Do not add automated workflows or make
CI status a prerequisite without an owner decision. See the
[delivery workflow](docs/DEVELOPMENT_WORKFLOW.md) for local checks.

## Report problems

Use a bug issue for reproducible behavior and a feature issue for a concrete
player need. Include the TrainerOS commit/build, device, OS and emulator/core
versions when relevant. Describe expected/actual behavior and minimal steps.
Redact tokens, account details, personal network addresses and file paths.
Do not upload ROMs, BIOS, saves or full private logs. Report vulnerabilities
through the [private security channel](SECURITY.md).
