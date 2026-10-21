# Reviewed Crates presentation data

`showcase.json` defines the manually ordered containers. `catalogue.json` defines
projects separately and maps each crate ID to an explicitly ordered array of
project IDs. A project can belong to several crates without duplicate identities.

The owner requested a small temporary open-source selection on 2026-10-18.
The bundled catalogue contains 2048, Hextris, Breakout, FeatherPad and Qalculate!
as labelled examples, with real upstream artwork and empty Crates release lists.
[Sources and notices](../../assets/crates/examples/README.md) distinguish upstream
screenshots from actual TrainerOS compatibility. Workshop now contains the owner's
Session Timer and Pocket Artillery, with actual desktop screenshots and explicit
experimental x86_64/SDK requirements. The signed server catalogue provides their
published release records; bundled presentation alone never grants installation.
This temporary utility selection does not replace Bazaar as the ordinary app store.
Fictional pagination/version fixtures remain only in `tests/fixtures/crates`.

Catalogue schema 1:

- `projects`: unique `id`, `title`, `kind` (`game`, `app`, `module`), `creator`,
  `summary`, `description`, `hero`, `screenshots`, `releases`, optional `license`
  and `sample` (presentation label, never an installation capability).
- `contents`: known crate IDs mapped to ordered unique project IDs.
- Each release: unique `version`, `channel` (`Stable`, `Beta`, `Experimental`),
  `architecture`, `compatibility` (`supported`, `untested`, `incompatible`),
  `compatibilityNote`, optional `sizeBytes`, `releasedOn`, `notes`.
- Current artwork is compiled `qrc:/crates/` or `qrc:/series/` content only.
  All text is rendered as plain text. Missing artwork has a neutral placeholder.

Parsing is bounded and atomic. Invalid identities, duplicate membership, dangling
references or malformed documents cannot partially replace the visible catalogue.
Unknown compatibility is untested, never supported. Installation state, ratings,
commands and external asset URLs are not accepted from this presentation document.

This is **not** the signed remote catalogue or an installation manifest. Server
trust, verified asset caching and installation are implemented separately by
[the Crates helpers](../../packaging/crates/README.md). Viewing release metadata grants
no filesystem or execution capability. The catalogue worker feeds this model
only after its own trust checks; never expose raw server data as an install command.
