# Local shared avatar v1

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
There is no `avatar.set` for games. Remote players/online identity and asset
transfer are outside this first local capability; never show the local player as
the peer. The host owns content-addressed cache paths. Cache cleanup must retain
files used by running consumers; no automatic eviction is implemented yet.

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
this minimum. No game, handheld package or remote-peer rollout is implied here.

## Local preparation

The checked-in pack lives in `assets/avatar/packs/trainer-avatar-1` and installs
once under `share/traineros/avatar/packs`. A development build also finds that
source pack. `TRAINEROS_AVATAR_PACK` explicitly overrides discovery.

Run `tools/avatar/fit_shirt.py` with Blender and the retained Tank Top `.blend`
and coordinate `.npz`, then
`tools/avatar/build_pack.py CONSTRUCTOR_PUBLIC_ASSETS OUTPUT --shirt FITTED_SHIRT.glb`
with Python/numpy/scipy/Pillow. Keep intermediate source meshes outside Git. The
compiler retains the complete body's original index accessor and applies
a conservative shirt coverage index only to the rendered outfit. It transfers
donor weights/correctives to the fitted shirt and binds hair/shoes to head/feet.

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
