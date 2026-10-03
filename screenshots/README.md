# TrainerOS on the handhelds

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

These two actual handheld captures document development work, not an accepted
multiplayer feature. The final installed delivery leaves this experiment disabled.
A later run proved independent remote controller gameplay; physical checks and
the internet route remain open. These invitation captures alone do not prove them; see
[runtime evidence](../docs/EMULATOR_MULTIPLAYER.md).

| Screen | Device | Preview |
| --- | --- | --- |
| In-game invitation route | Flip 2 | [![Experimental invitation](20-experimental-game-invitation.png)](20-experimental-game-invitation.png) |
| Named invitation consent | Odin 2 | [![Experimental consent](21-experimental-game-consent.png)](21-experimental-game-consent.png) |

## Capture provenance

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
