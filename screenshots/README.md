# TrainerOS on the handhelds

## Experimental PSP online relay evidence

Screens 35-37 are actual Gamescope captures on 3 October from executable
`e41f211d…`, following Home -> Online friend invitation and paired start.
Both PPSSPP processes used the external relay; both consoles were still on the
same home internet connection. Screen 37 shows a new match after a 12-second
relay interruption during rematch startup. The experiment was disabled again
after normal exit. See [scope and limits](../docs/PSP_MULTIPLAYER.md#online-relay-and-brief-loss--2026-10-03).

| Flip 2 | Odin 2 | After recovery, Odin 2 |
| --- | --- | --- |
| [![Flip](35-lumines-relay-flip.png)](35-lumines-relay-flip.png) | [![Odin](36-lumines-relay-odin.png)](36-lumines-relay-odin.png) | [![Recovered](37-lumines-relay-recovered.png)](37-lumines-relay-recovered.png) |

## Experimental PSP LAN evidence

Screens 33-34 are actual Gamescope captures after the complete TrainerOS Home
invitation/start route on 3 October, executable `e41f211d…`. One short round
accepted independent controller inputs and ended normally; both Home exit routes
then returned to the shell. The experiment was disabled again after verification.
See [scope and limits](../docs/PSP_MULTIPLAYER.md#integrated-short-check--2026-10-03).

| Flip 2, invited session | Odin 2, invited session |
| --- | --- |
| [![Flip](33-lumines-invited-flip.png)](33-lumines-invited-flip.png) | [![Odin](34-lumines-invited-odin.png)](34-lumines-invited-odin.png) |

| Device | Actual two-player Lumines arena |
| --- | --- |
| Flip 2 | [![Flip LAN arena](31-lumines-lan-flip.png)](31-lumines-lan-flip.png) |
| Odin 2 | [![Odin LAN arena](32-lumines-lan-odin.png)](32-lumines-lan-odin.png) |

Screens 31–32 are sequential Gamescope captures from the isolated copied-save
PPSSPP 1.20.4 diagnostic on 3 October, with `ForceLagSync2=True` on both peers.
They show an actual LAN round after separate controller inputs, not DEMO.
They do not prove sustained play, online transport or the complete corrected
TrainerOS invitation/return flow. See [precise evidence](../docs/PSP_MULTIPLAYER.md).

Actual 1920×1080 Gamescope captures from the installed ARM64 application on
Retroid Pocket Flip 2 and AYN Odin 2, taken on 2–3 October 2026. These are original
PNG screenshots, without compositing, cropping or generated replacement screens.
The red shell is Flip; the turquoise shell is Odin. Game progress uses test saves
and Social uses the designated test accounts.

## Home and library

| Screen | Device | Preview |
| --- | --- | --- |
| Home | Odin 2 | [![Home](01-home.png)](01-home.png) |
| Choose Adventure | Odin 2 | [![Choose Adventure](02-choose-adventure.png)](02-choose-adventure.png) |
| Pokémon Worlds | Flip 2 | [![Worlds](03-worlds.png)](03-worlds.png) |
| Game wheel and details | Flip 2 | [![Game wheel](04-game-wheel.png)](04-game-wheel.png) |
| Multiverse | Flip 2 | [![Multiverse](05-multiverse.png)](05-multiverse.png) |

## Companions and Trainer

| Screen | Device | Preview |
| --- | --- | --- |
| Field Guide | Flip 2 | [![Field Guide](06-field-guide.png)](06-field-guide.png) |
| Party | Flip 2 | [![Party](07-party.png)](07-party.png) |
| Boxes | Flip 2 | [![Boxes](08-boxes.png)](08-boxes.png) |
| Care Center | Flip 2 | [![Care Center](09-care-center.png)](09-care-center.png) |
| Playroom | Flip 2 | [![Playroom](10-playroom.png)](10-playroom.png) |
| Shops | Flip 2 | [![Shops](11-shops.png)](11-shops.png) |
| Trainer profile | Odin 2 | [![Trainer](12-trainer.png)](12-trainer.png) |
| Journey | Odin 2 | [![Journey](13-journey.png)](13-journey.png) |

## Social and system

| Screen | Device | Preview |
| --- | --- | --- |
| Messages and call history | Flip 2 | [![Social](14-social.png)](14-social.png) |
| Start system menu | Odin 2 | [![System menu](15-system-menu.png)](15-system-menu.png) |
| Appearance settings | Odin 2 | [![Settings](16-settings.png)](16-settings.png) |
| Communication settings | Odin 2 | [![Communication settings](17-communication-settings.png)](17-communication-settings.png) |
| Community discovery | Flip 2 | [![Search](18-social-search.png)](18-social-search.png) |
| Picture received from the ordinary web client | Flip 2 | [![Received picture](19-social-received-picture.png)](19-social-received-picture.png) |

## Experimental multiplayer (disabled by default)

These actual handheld captures document development work, not an accepted
multiplayer feature. The final installed delivery leaves this experiment disabled.
A later run proved independent remote controller gameplay; physical checks and
independent online gameplay remain open. Public-relay authentication is now proven
separately. These invitation captures alone do not prove gameplay; see
[runtime evidence](../docs/EMULATOR_MULTIPLAYER.md).

| Screen | Device | Preview |
| --- | --- | --- |
| In-game invitation route | Flip 2 | [![Experimental invitation](20-experimental-game-invitation.png)](20-experimental-game-invitation.png) |
| Named invitation consent | Odin 2 | [![Experimental consent](21-experimental-game-consent.png)](21-experimental-game-consent.png) |
| Online friend invitation, updated caption | Odin 2 | [![Online invitation](22-experimental-online-invitation.png)](22-experimental-online-invitation.png) |
| Exact Mega Drive game invitation | Odin 2 | [![Streets of Rage 2 invitation](23-experimental-megadrive-invitation.png)](23-experimental-megadrive-invitation.png) |
| Standalone PSP local invitation | Odin 2 | [![Lumines invitation](24-experimental-psp-invitation.png)](24-experimental-psp-invitation.png) |
| Game party in the existing group conversation | Odin 2 | [![Company party](28-experimental-company-party.png)](28-experimental-company-party.png) |
| Joined without another organizer prompt | Odin 2 | [![Company joined](29-experimental-company-joined.png)](29-experimental-company-joined.png) |
| Organizer's selected-member preference | Flip 2 | [![Company access](30-experimental-company-access.png)](30-experimental-company-access.png) |

Screen 23 is an actual 3 October capture with the experimental multi-game build
(`b5ce54dbc73131dc5ba6af7df892a51f631abe767220e81b0ad4349c71abc663`).
The accepted invitation launched Streets of Rage 2 on both devices; P2 character
selection and level movement were observed separately. Distinct-network and full
paired-session acceptance remain open. Normal installed launches disable this
experiment; this screen is not a claim of general platform support.

Screen 24 is an actual Odin Gamescope capture on 3 October, build
`9b157a0ec83ad7843e6c79e1a04c04be33e8142faf32bb15a7ccd5f69649455d`,
with the experimental flag enabled for the check. LAN consent and paired PPSSPP
launch succeeded; the later native Lumines match transition failed. See
[PSP evidence](../docs/PSP_MULTIPLAYER.md). The final ordinary installation has
the flag disabled. This screenshot is consent evidence, not gameplay acceptance.

## Capture provenance

Screens 28-29 are actual 3 October Odin captures from development build
`c40564e7c263d5bab105b11b873a881b127f929d556e65b1b5e069aa8280ac7a`;
screen 30 is the earlier controller-configured preference on Flip in the same
company check. The final installed build additionally preserves older-history
navigation and distinguishes closed/waiting parties in their captions. These
captures prove group discovery, retained preference and admission, not a completed
PSP match or simultaneous physical four-player sessions. The experiment is off
in the final ordinary installation. See [evidence](../docs/GAME_PARTIES.md).

Screens 25-27 are actual Gamescope captures on 3 October: Odin's friend-game
activity, Flip's reverse join request and Flip's accepted 2/2 roster. Development
build `9c1afa4748de963798ecc1e4590c6ed8ac2e6a72a40367d0abaf54c6c7f6e484`
had the multiplayer experiment enabled for this check. The final ordinary
delivery disables it again. These show real public-Fluxer signalling and Home
interaction, not completed PSP gameplay or four-player acceptance. See
[game-party evidence](../docs/GAME_PARTIES.md).

Screens 01–07 and 11–13 show the installed `023b3c8` delivery. Screens 08–10
and 14–18 show the call-history update included in the commit adding this gallery;
its executable SHA-256 is
`5e1cde8d9900dc282097aa795899a738d5695c856d8f5d60acf6abe4a61aca28`.
Only Social call-history presentation changed between these deliveries.
Screen 19 was captured on 3 October on the same executable. It shows an actual
incoming picture sent by the designated web test account and opened with the
handheld controller. The picture itself is an earlier TrainerOS test screenshot.

Screenshots document current appearance, not completion of every feature.
See [communication evidence and remaining checks](../docs/SOCIAL_MEDIA_VOICE.md)
and [ROADMAP](../docs/ROADMAP.md). Game artwork and characters retain their
respective owners' rights; this gallery is not an artwork pack.

Screens 38-42 are actual Flip/Odin Gamescope captures from 3 October's native
Dolphin bridge delivery: two-player Melee gameplay over LAN, physical Home over
the game, an Online friend invitation and accepted 2/4 roster. Both consoles run
the same TrainerOS/bridge hashes recorded in [Dolphin evidence](../docs/emulators/dolphin.md).
Online traversal startup was checked behind one router; these are not evidence
of distinct-network connectivity or four physical players. No ROM is included.
