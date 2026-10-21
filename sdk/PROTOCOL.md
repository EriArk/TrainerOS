# Platform API v1 preview — wire contract

Transport: local IPv4 TCP on `127.0.0.1`, ephemeral port. UTF-8 JSON objects,
one per newline, maximum 64 KiB including the newline. Only the local user can
read the endpoint secret. No HTTP/WebSocket server, CORS route or internet listener.
Qt is a host implementation detail; a client needs JSON + a byte-stream socket.

## Discovery and admission

Read `TRAINEROS_SDK_ENDPOINT` when explicitly set. Otherwise:

- Linux: `${XDG_DATA_HOME:-~/.local/share}/traineros/platform-v1/endpoint.json`.
- Windows development: `%LOCALAPPDATA%/traineros/platform-v1/endpoint.json`.

The host creates the dedicated directory/file with owner-only POSIX permissions
on Unix; Windows uses the user's local application-data directory and inherited
user ACL. Do not put the overridden endpoint in a public/shared directory. A lock
prevents two shells from replacing the same endpoint. A crashed host may leave a
stale file: connection failure means unavailable, not a running service. The next
host replaces it with a new `instance` and random token. Treat the token as secret.

```json
{"protocol":1,"host":"127.0.0.1","port":12345,"token":"<private>","instance":"<opaque>"}
```

First request within five seconds:

```json
{"id":"1","method":"hello","params":{"protocol":1,"token":"<private>","app":"org.example.counter"}}
```

```json
{"id":"1","result":{"protocol":1,"capabilities":{"context":1,"lifecycle":1,"ui.start.widget":1},"context":{"profile":"<local-id>","selected":{"id":"<library-id>"},"game":{"id":"","session":"","state":"idle","minimized":false},"app":{"id":""},"instance":"<opaque>","revision":"1"}}}
```

All request IDs are nonempty strings, maximum 64 characters. Clients correlate
the exact response ID. `app` and widget IDs match `[a-z][a-z0-9_.-]{0,63}`.
Only one active connection per app ID. App ID is self-declared namespacing, **not**
publisher authentication. The private user endpoint grants only this bounded
local interface; it is not a sandbox against other programs already running as
that OS user. Privileged future capabilities need their real host-service gates.

## Calls

| Method | Params | Result |
| --- | --- | --- |
| `context.get` | `{}` | `{"context": snapshot}` |
| `context.subscribe` | `{}` | Current snapshot; subsequent context/lifecycle events |
| `context.unsubscribe` | `{}` | `{}`; widget activation still belongs to its registration |
| `widget.put` | `id`, `title` (1–64 characters), `text` (0–240), current `revision` string | `{"id": widgetId}`; replaces that connection's same widget |
| `widget.remove` | `id` | `{}`; idempotently removes only that connection's widget |
| `goodbye` | `{}` | `{}`, then disconnect and cleanup |

Example widget request:

```json
{"id":"2","method":"widget.put","params":{"id":"counter","title":"My counter","text":"Press A or tap","revision":"1"}}
```

Widgets are declarative plain text and one action, with host-owned layout,
navigation, focus and touch. No QML, URL, command, executable, HTML, arbitrary
image or script is interpreted. First registration appears in Start → Edit → Add;
the user places it. The same app/widget ID can restore the user's layout after
reconnect. Hiding a widget only hides its contribution; it does not close the app.
No profile means no personal widget registration. At most eight widgets per
connection, sixteen connections; client and host buffers are bounded.

`widget.put` is contextual: a stale revision returns `stale_context`. Read the new
context and decide whether the operation still applies; do not blindly retry
against a different player/game. `widget.remove` remains safe after a game-context
change. Existing widgets remain while navigating games; the app receives context
changes and owns its content. A widget callback carries context **at activation**.

## Events and independent state

```json
{"event":"context.changed","context":{"...":"complete current snapshot"}}
{"event":"widget.activated","id":"counter","context":{"...":"snapshot at activation"}}
{"event":"lifecycle","name":"wake","context":{"...":"current snapshot"}}
{"event":"context.revoked","reason":"profile_changed"}
{"event":"host.stopping"}
```

- Context is derived from actual Trainer, Shell, AdventureLaunch and Apps
  controllers. `selected` is a library selection, not permission to act on `game`.
- `profile` is a local Trainer ID, not an online TrainerOS/Fluxer identity.
- `game.state` comes from the launch lifecycle: preparation/start is not playing.
  `game.session` is opaque and assigned on running; it is not an admission ticket,
  save lease or verified competitive result. `game.id` can remain as last launch
  metadata while idle: state/session determine whether a game is actually live.
- `app.id` describes the existing Apps controller's current app; it is not proof
  that the connecting client is that app. No PID, executable path, token, private
  conversations, full library or save data is exposed in this context.
- Profile changes revoke connections/remove widgets. No automatic client rebind
  to another player's data. Game changes update subscribed snapshots instead.
- Disconnect/crash removes all live widgets/subscriptions, including pending edit
  targets. Already delivered application actions cannot be rolled back by socket
  teardown. User layout remains separate from replaceable widget content.
- Host stop sends a best-effort event then disconnects; clients must also handle
  EOF/reset. A host process crash need not send a final event. Reconnect starts a
  new handshake and explicit registrations; no automatic replay of actions.
- `wake` reports the actual PowerControl signal; this preview does **not** expose
  a suspend veto, certify sleep with a running emulator, or implement resume.

## Errors and unsupported capabilities

```json
{"id":"2","error":{"code":"stale_context"}}
```

Codes: `unauthorized`, `unsupported_version`, `invalid_app`, `app_in_use`,
`no_profile`, `stale_context`, `invalid_widget`, `limit_reached`, `unsupported_method`.
Malformed/oversized frames or a stalled outgoing queue close the connection.
An auth/version failure returns a best-effort error then closes. Capability absence
and `unsupported_method` are honest unavailability; no success-shaped placeholder.

Never automatically replay a mutation after a timeout or ambiguous disconnect.
The supplied client closes and reports unknown delivery. Ordinary program UI/data
can remain usable without the host. This protocol does not require an online
account, Crates release, application-specific server, or new permission prompt
for every read/event. It also does not authorize future sensitive operations.
