# Compact Social and Fluxer profiles

8 October 2026. Owner-approved correction to the first UX-01 delivery. The
previous layout was rejected: oversized buttons and the permanent Together
panel left too little space for conversation. Historical functional evidence in
[SOCIAL_UX](SOCIAL_UX.md) remains evidence, not acceptance of that composition.

## Interface and provider boundary

The conversation has one compact header (avatar/name, call, ellipsis) and one
composer row (attachment, draft, emoji, microphone, send). The sidebar has quiet
conversation rows, scoped ellipsis actions, a create/manage button and the current
account. Consecutive messages by the same author within five minutes share a
heading. Earlier history loads at the top of a completed upward scroll, with an
explicit fallback and a small Latest control. A thin activity strip appears only
for a real party/join request/call. No permanent Together billboard.

Person/group/community menus retain their target identity independently of the
displayed conversation. Existing game invitations, joining, membership,
notification policy and group access use their existing controllers. A missing
running multiplayer game produces an explanation. Controller Select opens the
same scoped context; its Messages/Communities entry reaches collection actions.
X composes, Y retains the compact shared-activity menu, and Back dismisses it.
Select > Message tools also reaches pictures, voice recording, emoji and the
retained draft without touch; horizontal emoji navigation and Back remain explicit.
The ordinary runtime, game exit and microphone consent gates still apply.

Profiles are accessible from conversation names/avatars, message authors,
contact rows, group members and the own-account footer. Public name, avatar,
username, bio, pronouns and mutual counts come from Fluxer's native profile
endpoint. Message, friend request, removal and block use provider operations;
copy username is local. Own Edit profile reuses Settings > Communication's
existing name/avatar/bio editor. Profile requests are on demand, not polling;
account epochs and request identities reject stale replies. A failed or limited
response never falls back to old private fields. Arbitrary file upload, custom
profile databases, message search and an embedded web messenger are not added.

## Sources and reuse

- [Fluxer profile API](https://docs.fluxer.app/http-api/users/) and
  [current-user updates](https://docs.fluxer.app/http-api/users/current-user/).
- Fluxer source inspected at `8b312c610ee5784c550940defd2ad54ae2e2d2f2`:
  `UserProfileCommands.tsx`, `UserProfile.ts`, `UserContextMenu.tsx`,
  `TextareaButtons.tsx`, `UserProfileShared.tsx` and `UserRequestSchemas.ts`.
  QML reuses those provider contracts and interaction organization; React/AGPL
  application source is not vendored or claimed as a drop-in native component.
- Unmodified [Phosphor SVGs](../assets/icons/phosphor/README.md), the same icon
  family used by Fluxer. MIT license is preserved and embedded with the icons.
- [Progressive disclosure](https://www.nngroup.com/articles/progressive-disclosure/)
  and [Carbon overflow menus](https://carbondesignsystem.com/components/overflow-menu/usage/)
  informed contextual placement and removal of infrequent actions from the chat.

## Validation

ARM compilation and the Social, interaction, game-party and exit-presentation
tests pass. Added checks cover group/action identity when another conversation
is open, profile response ordering and privacy, request-by-ID POST versus
accept/block PUT, cross-face DM navigation, draft-only emoji insertion and
controller access to message tools. Previously recorded wider baseline failures
in SOCIAL_UX remain separate; Windows and fresh-machine acceptance are not claimed.

The installed native client on both handhelds read actual Fluxer profiles. Flip's
own editor changed its empty bio to `UX02`; Odin fetched that value through the
public profile route. Flip then cleared the bio and Odin confirmed the restored
empty value. A deliberate composer Send delivered one emoji to the other test
account. Earlier/Latest, profile/card dismissal and scoped group menus were
exercised. Odin's header Call started the existing muted group call, the compact
status strip appeared, Y exposed controls, and Leave call ended it. Both outputs
remained at 0%; this is not human speech/headset acceptance. Inviting without a
running supported game reports the prerequisite instead of silently doing nothing.

Visual review found reproducible glyph degradation on Flip's installed OpenGL
route after expanding menus. Qt 6.7+ curve text is requested explicitly in Social,
including dynamically created delegates, and as the Flip shell default. Qt 6.4–6.6
retains its original renderer. The final Flip review repeated profile/overflow/
emoji opening and returned to readable chat rows. This changes shell text only;
Odin keeps Vulkan and no graphics variable is exported into emulator children.
See [Qt text rendering](https://doc.qt.io/qt-6/qquickwindow.html#TextRenderType-enum).
The idle message viewport occupies approximately 72% of the right pane's height
at 1920×1080, compared with roughly 20% in the rejected composition.

The final ARM binary is installed and its running process verified on both
handhelds (Flip PID 1291872, Odin PID 375046 at delivery). SHA-256:
`e335e024eacf8255dca4efd1f94a72ca9a2c91ccd7ff927214fed39eae87193a`.
On this final build, controller-only Select > Message tools > Attach picture
opened the existing picture picker and Back returned to the conversation.
Final logs showed no Social QML errors; both system outputs were checked at 0%.
The installed build also retains the pre-existing MP-02 timer changes, which
remain outside this Social commit.

Reviewed native captures from the final installed build replace only this
bounded Social screenshot set:
[Flip conversation](../screenshots/social-2026-10-08/messages.png),
[scoped group menu](../screenshots/social-2026-10-08/group-details.png),
[Odin conversation](../screenshots/social-2026-10-08/messages-odin.png), and
[Fluxer profile](../screenshots/social-2026-10-08/profile.png).

This correction does not close
#135, physical ergonomics/audio, distinct-network/group-size, later runtime,
Hotseat, artwork or distribution acceptance. MP-02 is next after this delivery.

## Palette follow-up, 8 October 2026

The owner requested the shared colourful handheld character back in Social.
The compact layout now uses a peach sidebar, lavender conversation header,
warm cream message surface and the shell's blue/pink/yellow/green icon accents.
Avatars and golden focus highlights repeat the existing shell palette. Message
text stays on a plain readable surface. Control sizes, message viewport,
provider behaviour and controller routes are unchanged.

The ARM/QML build passed. The palette build is installed on both handhelds;
SHA-256: `4f8cd4d2783e807f4d811edbe5af66530c134777b9e27641b094cef058a7f7b4`.
Device screenshots above now show this follow-up. Existing functional tests
are reused for this colour-only change; no new runtime acceptance is claimed.
The installed chat, profile and overflow were visually reviewed; the runtime
logs contained no Social QML errors and both outputs remained at 0%.
