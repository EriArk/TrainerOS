# Shared avatar v1: local model and peer recipes

TrainerOS owns appearance and the installed asset pack. A game distributes only
its SDK adapter, never a copy of the character. The interface is engine-neutral:
ordinary glTF 2.0 / GLB, metres, +Y up, +Z forward, floor at Y=0.

## Minimal path

After the ordinary Platform API handshake, check `capabilities["avatar.local"] == 1`.
Call `avatar.subscribe` (or one-shot `avatar.get`). Result:

```json
{"avatar":{"schema":1,"profile":"local-profile-id","pack":"trainer-avatar-1",
"revision":"content-key","appearance":{"schema":1,"pack":"trainer-avatar-1","hair":"swept","eyes":"round","hairColor":"#62402c","eyeColor":"#98602d","skinColor":"#f0c4a5","shirtColor":"#ec9e8d","shortsColor":"#577c87"},
"glb":"/absolute/console-cache/content-key.glb","preview":"/absolute/console-cache/content-key-preview.glb",
"sha256":"file-sha256","rig":"trainer-humanoid-54-v1","units":"metres","forward":"+Z","eyeHeight":0.8,"actions":{"wave":"Wave","sit":"Sit","squat":"Squat","step":"Step","raise_arms":"Arms_Up"}}}
```

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
installed manifest>, appearance:<the nine-field appearance object>}`. Send only
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
[TrainerOS ID / Fluxer binding](../docs/TRAINEROS_ACCOUNT_ECONOMY_RU.md) remains a
separate target: TrainerOS ID must not become the Fluxer ID or a local profile ID.

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
`play_action` returns false. Facial expressions and cloth physics are not part of
this minimum. Pocket Golf source now binds local and remote presenters, converts
+Z to its -Z forward, uses the model eye height and waits until a shot finishes
before replacing appearance. No handheld delivery is implied.

## Local preparation

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
