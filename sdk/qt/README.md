# Qt avatar cards

Optional GPL-3.0-or-later SDK consumer code. Add `AvatarCards.h` to an AUTOMOC
target linked to Qt Core/Network, and bundle the three QML files together.
The card UI itself uses Qt Quick; the short-lived portrait renderer additionally
needs matching Qt Quick 3D/AssetUtils plugins. Without those plugins or an avatar
capability the card retains an anonymous silhouette and gameplay continues.

Construct `AvatarCards(appId, endpoint)`, expose it as a QObject to QML and call
`start()` after the first application frame. It uses a separate read-only avatar
connection, so local play does not impersonate a second Trainer or need Lobby.
Bind `sendRecord` to the game's existing reliable channel. On `network.ready`
call `session(session.peer)`; on close call `clearPeer()`. Validate the channel,
sequence and **peer against the admitted session** before routing records with
namespace `traineros.avatar.v1` to `receive()`. Keep these records outside the
game rules decoder and return normal receive credit. No extra transport exists.

`local` and `peer` are descriptors suitable for `PlayerCard.descriptor`.
Set the label/seat/accent from game state; never use the local descriptor for a
guest/bot. Empty recipes withdraw identity. Replacement, close and context
revocation fence outstanding resolutions. Latest peer recipes coalesce behind
one resolver and a one-second interval. Invalid files/digests use the fallback.
Only the small recipe crosses the channel, never GLB files, paths or tokens.

`AvatarPortrait` captures a 256px transparent bust and releases its renderer.
The grab result stays alive with the 2D image. Saved appearance invalidates the
image; there is no continuous 3D card rendering. Qt uses the descriptor's neutral
preview GLB after validating the companion game GLB; both are host-resolved files.
For Godot use `sdk/godot/avatar_portrait.gd` and generation-fence asynchronous
completion when identity changes. The helper returns a Texture2D and releases
its SubViewport. Headless callers receive null.

Checks: `cmake -S sdk/qt/tests -B build/avatar-cards-checks`, build, then CTest.
The socket fixture covers replacement/withdrawal/revocation, not mesh rendering.
`avatar-portrait-preview MODEL-preview.glb OUTPUT.png` separately exercises the
actual QML importer, capture and renderer release. Device acceptance is separate.
