# TrainerOS interactive website demo

A mouse-driven, self-contained tour of TrainerOS. The native shell's five primary
tabs, peer faces, game wheel, Choose Adventure, settings and in-game Home menu are
recreated in HTML. A Russian explanation panel lives above the English interface.
The embedded screen retains its landscape proportions.

## Open or embed

No application build, backend, account, CDN or runtime dependency is required.
Serve this directory with any static HTTP server, for example from the repository:

```sh
python -m http.server 8766 --bind 127.0.0.1 --directory web-demo
```

Open `http://127.0.0.1:8766/`. For a website, copy **this directory's contents** to
the chosen public path. Assets are relative; deployment under a subdirectory is
supported. Do not deploy the repository, private research files or ROM library.

```html
<iframe
  src="/demos/traineros/"
  title="Try the TrainerOS interface"
  loading="lazy"
  style="width:100%;height:1000px;border:0;border-radius:16px"
></iframe>
```

Use enough iframe height for the explanation panel as well as the 16:9 shell.
The desktop layout also fits the shell against the available viewport height.
Small screens retain the landscape composition, so desktop/landscape is the best
way to explore. Mouse is the target input; keyboard tab/focus, Enter and Escape
are supported for browser accessibility. This is not a controller implementation.

## What visitors can try

| Area | Interactive behavior |
| --- | --- |
| Home | Cycle four fictional series; independent selected adventures; launch; party preview |
| Worlds | Collection/system grids; cyclic game wheel; detail/artwork/player counts; immediate launch; Home selection; rename, delete and review examples |
| Guide | Combined list/detail, four original creatures, favorite, name search, colored stats |
| Party / Boxes | Individual details and moves between party and storage; at least one party member retained |
| Care Center | Animated healing updates demo HP; save-care explanation; trade example; illustrative turn-based practice |
| Playroom | Wandering companions, greetings and a group interaction example |
| Shops | Immediate stock preview, quantities, cross-shop basket, one purchase, wallet and bag updates |
| Trainer | Editable name; profile; journey; memories; illustrative achievements |
| Social | Immediately selected conversations, groups, community, demo directory, composer, emoji, sample attachment, scripted reply, call/mute and invitation examples |
| Start / Settings | Two-panel settings; persisted shell colors and motion; illustrative sound/storage/radio/communication controls |
| In-game Home | Continue, Nearby / Online friend, acceptance example, display/scanlines and explicit exit |

“Показать пример” runs short annotated scenes for library launch, guide, team
storage, healing, shopping, playroom, profile/journey, themes and invitations.
Trade has its own animated example at Center → Link Counter. Stop, navigation and
Escape cancel the remaining scheduled steps; no guided scene starts on its own.

Profile, theme, favorites, library edits, party/boxes, HP, purchases and messages
are stored under `traineros.website.demo.v1` in this browser's localStorage.
The reset action only replaces that key. If storage is unavailable, the demo still
runs in memory. There are no analytics, cookies, remote API calls, microphone
requests or uploads. Message/name text is rendered as text, not executable HTML.

## Boundaries

This is an **illustrative interface demo**, not the native application or an
emulator. All games, characters, progress, friends and messages are fictional.
Dates, years, supported-platform labels and player counts are demonstration data.
No account is authenticated, no actual multiplayer connection is made, and no
ROM, game save or operating-system setting is accessed. The care/trade/battle
scenes illustrate supported-game concepts; they do not certify universal adapter
support. Native MP-02 and other pending acceptance remain open.

The game view is an animated illustration, not recorded gameplay. Radio, audio,
backup and system-mode controls disclose their demonstration boundary in context.
The browser demo is silent. The ongoing-call indicator illustrates the native
background-call journey but does not access audio devices.

Artwork provenance is in [ASSETS.md](ASSETS.md), [environment prompts](assets/generation.json)
and [companion prompt](assets/companions-generation.json).
Existing public screenshots and the handheld installation are not modified.

## Validation

From the repository root, with Python Playwright and Chromium installed:

```sh
python -m pip install playwright==1.58.0
python -m playwright install chromium
python tools/test-web-demo.py --screenshots work/research/web-demo-screenshots
```

An existing Chromium binary can be supplied through `--browser PATH`. The check
starts a loopback-only server and a fresh temporary browser context. It covers
navigation/wheel wrapping, direct launch/exit, guide/storage/healing, basket
arithmetic, chat text escaping, invitations, background-call continuity, settings
persistence, guided scene completion/cancellation, reset and four viewport sizes.
It rejects browser errors and failed asset requests. These are **website browser
captures**, not handheld screenshots or native runtime acceptance.

Original code is GPL-3.0-or-later, as in the parent project; [LICENSE](LICENSE)
is included for self-contained distribution. Font notices stay with their files.
