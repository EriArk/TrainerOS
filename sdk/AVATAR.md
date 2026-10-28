# Shared avatar v1: local model and peer recipes

New integration? Start with the [step-by-step guide and runnable Godot demo](AVATAR_QUICKSTART.md).
This page is the protocol/reference companion.

The current source pack is **revision 9**: the corrected face bridge, 14 eye
styles and 22 separate garments, including golf, two starter looks and two
racing looks. All four hairstyles retain reversible cap/visor fits; racing
helmets hide hair until removed. See [collection integration](../docs/AVATAR_COLLECTION_DELIVERY_20261028.md),
[wardrobe integration](../docs/AVATAR_WARDROBE_20261028.md) and
[facial API](AVATAR_EXPRESSIONS.md). Historical revision descriptions below
remain evidence of earlier deliveries.

TrainerOS owns appearance and the installed asset pack. A game distributes only
its SDK adapter, never a copy of the character. The interface is engine-neutral:
ordinary glTF 2.0 / GLB, metres, +Y up, +Z forward, floor at Y=0.

## Minimal path

After the ordinary Platform API handshake, check `capabilities["avatar.local"] == 1`.
Call `avatar.subscribe` (or one-shot `avatar.get`). Result:

```json
{"avatar":{"schema":1,"profile":"local-profile-id","pack":"trainer-avatar-1",
"revision":"content-key","appearance":{"schema":3,"pack":"trainer-avatar-1","hair":"swept","eyes":"round","hairColor":"#62402c","eyeColor":"#98602d","skinColor":"#f0c4a5","shirtColor":"#ec9e8d","shortsColor":"#577c87","socksColor":"#f4f0df","shoesColor":"#f4f0df","headwearColor":"#234f43","top":"tank","bottom":"base-shorts","socks":"none","shoes":"sandals","headwear":"none"},
"glb":"/absolute/console-cache/content-key.glb","preview":"/absolute/console-cache/content-key-preview.glb",
"sha256":"file-sha256","rig":"trainer-humanoid-54-v1","units":"metres","forward":"+Z","eyeHeight":0.8,"actions":{"wave":"Wave","sit":"Sit","squat":"Squat","step":"Step","raise_arms":"Arms_Up"}}}
```

Appearance schema 2 has the original nine fields plus `top`, `bottom`, `socks`,
`shoes` and `headwear`. Schema-1 stored looks remain accepted and normalize to
`tank`, `base-shorts`, `none`, `sandals`, `none`. Descriptor/recipe envelopes remain
schema 1. IDs and colours are validated, and peer pack hashes must still match.
No purchase is required for these garments; paid saved-look slots remain separate.
The sundress and both racing suits occupy `top` and cover the stored `bottom`.
Removing a whole outfit restores that bottom; equipping a separate bottom replaces
the whole outfit with the default tank. Racing suits include their matching gloves.
These rules belong to the host resolver: games consume the resulting GLB and need
no clothing-specific branches. Appearance schema 3 is unchanged.
Current appearance schema 3 additionally carries `socksColor`, `shoesColor`,
and `headwearColor` as `#RRGGBB`. Schema 1/2 inputs remain accepted; normalization
adds ivory socks/golf shoes, the original blue-grey sandals and forest headwear.
The wardrobe saves equipment and
colour changes immediately in the active look, using the same `avatar.changed`
delivery. A focused catalogue item is distinct from an equipped item.
Garment selection, optional primary fabric colour and headwear fitting belong
to the console resolver. It preserves one 54-joint rig, garment corrective
tracks and facial metadata, omitting unused meshes/buffers within the existing
64 MiB bound. Do not rely on mesh/accessor array indices across resolved looks.

Load `glb` directly at runtime; it contains the chosen face surface, colors,
garments, skin and corrective animation. Do not copy it into a game package or
save its machine-specific path as player identity. The selected preset lives in
the Trainer profile's SQLite record, not in the game or browser storage.

`avatar.changed` carries the same `avatar` object after a saved change. Draft
editor changes are private to the editor. Apply a new visual at a safe boundary
in your game's action; release the old visual. `avatar_not_ready` means generation
is pending; **a subscribe returning this error remains subscribed**. Missing pack
means capability absent. `no_profile` requires choosing a Trainer first.

On `context.revoked`, `host.stopping`, EOF or connection failure, clear the old
identity/visual. A different profile requires a deliberate fresh handshake.
There is no `avatar.set` for games. Never show the local player as the peer.
The host owns content-addressed local-profile cache paths; these have no automatic
eviction. Peer recipes use the bounded temporary lifetime described below.

## Peer recipe, without transferring models

Local descriptors also contain `recipe`: `{schema:1, packRevision:<SHA256 of the
installed manifest>, appearance:<the canonical appearance object>}`. Send only
this object through the game's existing admitted reliable channel, wrapped as
`{"namespace":"traineros.avatar.v1","recipe":{...}}`. An empty recipe withdraws
the appearance. Current recipes are under 1 KiB. Do not send local profile IDs,
GLB paths, textures, credentials or the full local descriptor.

On the receiving console, `avatar.recipe:1` enables
`avatar.resolve({recipe:...}) -> {avatar:<local render descriptor>}`. It validates
the exact schema, preset IDs, colours and installed pack hash, then constructs
the GLB on a worker thread. It never downloads a model or changes the local
profile. The resulting descriptor intentionally has **no `profile` claim**.
`invalid_avatar_recipe`, `avatar_pack_mismatch` and `avatar_busy` are explicit
failures; use an anonymous marker for an unsupported peer appearance.

There is one construction per connection, at most four globally. Each connection
retains only its latest successful temporary result; consume/load that GLB before
requesting another. Disconnect/profile revocation releases the temporary files.
Already loaded Godot scenes retain their resources. Coalesce rapid updates and
wait for a safe action boundary before requesting/replacing another rig.

`godot/avatar_exchange.gd` handles recipe exchange, duplicate suppression,
one-second construction pacing, latest-update/session fencing and withdrawal.
Attach it to the game's existing record bridge (`session`, `channel`, `send`,
`session_ready`, `closed`, `sdk_record`). The bridge routes this namespace outside
game rule messages and verifies the `network.data.peer` against the admitted
session. Set `can_resolve` when the game needs to defer loading during an action.

Identity comes from the admitted session member, **not the recipe**. This is not
a server-account lookup or proof of paid cosmetic ownership. The accepted
[TrainerOS identity API](PROTOCOL.md#backend-identity-preview) is separate: an avatar recipe does not
authenticate a Player UUID, a Fluxer account or a local profile.

## Godot / Pocket Golf

`godot/avatar_client.gd` is a read-only consumer without Lobby dependencies.
`godot/avatar_presenter.gd` validates the file hash and loads the external GLB
using Godot's runtime `GLTFDocument`. Copy/version these **code files** as the SDK
dependency. No art belongs under `res://`.

```gdscript
var client = TrainerAvatarClient.new()
client.app_id = "art.abysstail.pocket-golf.avatar"
add_child(client)
client.appearance_changed.connect(func(description):
    var visual = TrainerAvatarPresenter.from_descriptor(description)
    if visual != null: anchor.install_visual(visual, description.profile))
client.unavailable.connect(func(_reason): anchor.clear_identity())
client.connect_host()
```

The existing Pocket Golf `AvatarAnchor.install_visual` is the intended seam.
Use `eye_height` for a head camera and the presenter's +Z forward convention.
Visuals do not participate in ball collision or control shot timing. The first
pack has joint/pose clips, **not** an authored `golf_putt` animation; unsupported
`play_action` returns false. Pack revision 5 adds [facial expressions and mouths](AVATAR_EXPRESSIONS.md)
through the shared presenter; cloth physics remains outside this minimum.
Pocket Golf binds local and remote presenters, positions the
golfer side-on to the ball, uses model eye height and waits until a shot finishes
before replacing appearance. Pack revision 4 was installed on Odin/Flip on
27 October; game/runtime verification is recorded separately.

The Godot presenter adapts linear glTF vertex colors for Compatibility rendering.
It updates only the imported color attribute, preserving UVs, skin, corrective
buffers and the hashed source file. Mobile/Forward+ keep linear colors. Use the
helper as a versioned unit instead of adding game-specific material overrides.
See [Godot's vertex color behavior](https://docs.godotengine.org/en/stable/classes/class_basematerial3d.html#class-basematerial3d-property-vertex-color-is-srgb).

The 28 October presenter also evaluates body/garment corrective morphs on the
CPU in Compatibility, where GPU morph playback corrupted otherwise unchanged
head vertices. Original clips drive per-instance weights; skeleton skinning
stays on GPU. Dirty surfaces update before rendering and revocation disconnects
the updater. Other renderers retain GPU morphs. Run `tests/correctives.gd` with a
resolved descriptor to check all eight clips, unchanged head/skin and instance
isolation. Update the SDK code in consumers; an asset-pack update cannot replace
a presenter's code already embedded in a game package.

## Local preparation

### Facial rig, pack revision 5 (28 October 2026)

Revision 5 retains the approved appearance and adds eight mouth poses, nine
expressions and four transient reactions. The full GLB carries named morphs and
`extras.trainerFace` metadata. The resolver's optional `eyeBodyMeshes` mapping
keeps legacy painted eyes attached to their original face surface; the twelve
modular sets retain the prepared face. Install the updated resolver together
with this pack. Descriptors, recipes and saved appearance fields do not change.
Pack hashes still reject peers with a different installed revision.

See the [game API and example](AVATAR_EXPRESSIONS.md) and
[authoring / verification record](../docs/AVATAR_EXPRESSIONS_20261028.md).
The revision 4 delivery below is historical, not proof of revision 5 on devices.

### Approved art, pack revision 4 (27 October 2026)

The shared pack now includes the corrected standalone body (head scale 0.92 and
the retained shorts repair), separate thin-strap tank and four approved anime
hairstyles. The source body remains in
[`approved/base-body`](../assets/avatar/approved/base-body/README.md).

The `eyes` field accepts the retained `round` / `almond` choices and twelve new
IDs: `rogue-base`, `rogue-feminine`, `innocent-base`, `innocent-feminine`,
`sly-base`, `sly-feminine`, `calm-base`, `calm-feminine`, `sleepy-base`,
`sleepy-feminine`, `determined-base`, `determined-feminine`. The wardrobe exposes
them to touch and controller input. Saved legacy choices are preserved.

The resolver selects the matching prepared face and exactly one eye set, retaining
the same skeleton and animation clips. Iris, skin, hair/brows, shirt and shorts
follow the existing appearance colours. Facial blinking/gaze and cloth simulation
are not added. Consumers still load one resolved GLB; they do not assemble meshes
or copy art into games. Descriptor/recipe schema and identity rules are unchanged;
pack hashes fence peers with different installed revisions.

`tools/avatar/import_approved.py APPROVED_ROOT LEGACY_V3_PACK OUTPUT` reproduces
revision 4 from the approved source models and retained v3 face textures/masks.
The v3 inputs can be restored from commit `ebfdd07c` under
`assets/avatar/packs/trainer-avatar-1`. Only the resulting runtime pack installs
on a console; Blender sources and comparison renders remain authoring assets.

### Static player portraits

Games can capture the resolved model once into a 256px transparent portrait.
Use [Qt cards](qt/README.md) or `godot/avatar_portrait.gd`; the portrait retains
the saved appearance and requires no copied model assets or new host method.
Use the existing recipe exchange for a peer, the admitted session identity for
seat mapping, and an anonymous fallback for bots, local guests or unavailable
avatars. Clear on withdrawal/revocation and fence asynchronous completion.
The renderer must be released after capture rather than running behind each card.


The checked-in pack lives in `assets/avatar/packs/trainer-avatar-1` and installs
once under `share/traineros/avatar/packs`. A development build also finds that
source pack. `TRAINEROS_AVATAR_PACK` explicitly overrides discovery.

For a future authoring rebuild, run `tools/avatar/fit_shirt.py` with Blender and a Tank Top `.blend`
and coordinate `.npz`, then
`tools/avatar/build_pack.py CONSTRUCTOR_PUBLIC_ASSETS OUTPUT --shirt FITTED_SHIRT.glb`
with Python/numpy/scipy/Pillow. Keep intermediate source meshes outside Git. The
compiler retains the complete body's original index accessor and applies
a conservative shirt coverage index only to the rendered outfit. It transfers
donor weights/correctives to the fitted shirt and binds hair/shoes to head/feet.
The owner deleted `TrainerAvatar-local` on 24 October, including those external
authoring inputs and old review captures. The checked-in runtime pack is intact;
using it and changing runtime appearance does not require those deleted inputs.

`tools/avatar/install_pack.py EXTERNAL_OUTPUT` installs once into the user's
shared `traineros/avatar/packs/trainer-avatar-1` data directory. Development can
instead set `TRAINEROS_AVATAR_PACK`; children need only the SDK endpoint.
Qt profile preview requires matching Qt Quick 3D, Quick Timeline and AssetUtils
plugins, including the Assimp import plugin's dependencies. The
shared resolved GLB has no Python, Three.js or Godot runtime dependency.

The owner authorized including the prepared pack in the repository. The
[asset notices](../assets/avatar/README.md) preserve third-party attribution and
the remaining body/Anime Hair 14/20 redistribution review. This is not a final
public binary asset release.
