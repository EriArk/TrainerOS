# Social, navigation and Home acceptance — #98–112

Reviewed with issue bodies and owner comments on 2026-10-01. The owner approved
the dependency order below. [ROADMAP](ROADMAP.md) remains the single execution
queue; this register preserves acceptance, not a second schedule. Earlier
R1–R18/R7a/R18a/R18b, exact-game, device, image/update, trust and artwork gates
remain. Planned integrations are not installed capabilities.

## Placement and lifecycle supersessions

- [#111](https://github.com/EriArk/TrainerOS/issues/111) owns five primaries:
  **Home / Worlds / Companions / Trainer / Social**. Trainer has full peer faces
  Profile / Journey / Hall / RA; Social has Friends / Chats. This supersedes the
  separate Journey primary in #85 and the former in-Trainer messenger placement
  in #101. Profile editing, live progress, historical archive and independent
  external RA truth keep their existing controllers and ownership.
- L1/R1 retains primary edges; L2/R2 wraps secondary faces. First Trainer entry
  opens Profile; revisits restore its face; explicit history links select that
  face. Migrate old named/numeric page slots, nested archive state, notifications,
  Settings returns and launch checkpoints; an old slot 4 must never become Social.
  Unsupported route versions have a safe Home fallback. Retain per-owner state,
  per-face focus/selection, existing modal/write priority and Worlds grid re-entry.
- [#112](https://github.com/EriArk/TrainerOS/issues/112) **plans** adaptive physical
  Home/Guide quick access in both shell and supported games. Opening it does not
  jump to Home, ask about saving, end history or pause an online game. It captures
  the actual owner/live session/origin, not a subsequently selected library game.
  Home again/B/Continue return to the same origin/process. Start retains #86's
  system controls, Settings, Power and Trainer switching; no duplicated controls.
- Updated [#49](https://github.com/EriArk/TrainerOS/issues/49) owns only explicit
  Exit game: fresh clean gameplay capture, exact-title policy, manual/unknown
  confirmation, then graceful close. Verified autosave can close without the
  manual-save question. Cancellation preserves the same live process. Clean
  capture occurs after Exit intent, before its question; an overlay thumbnail
  is not exit evidence. Preserve capture errors, neutral input, stale-session
  rejection, spontaneous exit/watchdog recovery and ordinary saves; no persistent
  savestate resume. These Home/exit changes are a separate increment after #111.
- Home appearance keeps shader, ratio, exact supported widescreen hack and bezel
  independent, with small previews, per-game override/inheritance/reset, actual
  live-versus-next-launch state and failure/cancel behavior. Use proven runtime
  APIs; no automatic restart, raw-config browser, imaginary PC-game settings or
  unverified hacks. Social quick access later reuses the same providers/state.

## Messenger and provider

| Issue | Preserved acceptance and dependencies |
| --- | --- |
| [#98](https://github.com/EriArk/TrainerOS/issues/98) | Small native controller-first social experience; no community workspace, server tree, feed, marketplace, streams, universal inventory/currency or mandatory always-online use. Conversation, voice, connection, native activity and runtime gameplay are independent. Deliver increments; unrelated work is not implicitly deleted/reordered. |
| [#99](https://github.com/EriArk/TrainerOS/issues/99) | First bounded integration gate: verify supported **user-client** auth/MFA/revocation/logout and friend/DM/group/history/realtime/read-state/presence/media/voice/invitation capability matrix against official current deployment. Bot/identity OAuth is not messaging authorization. ARM64 native feasibility, upstream terms/license, exact versions and sanitized two-account proof or precise blocker; no token scraping or hypothetical future voice protocol. |
| [#100](https://github.com/EriArk/TrainerOS/issues/100) | Native provider after #99. One external binding per local Trainer; canonical instance + immutable user ID, lossless IDs, generation-bound callbacks/drafts/uploads/cache. Protected credential store policy, endpoint/TLS/redirect rules, bounded history, paging/backoff/resync and uncertain-send deduplication. Logout clears account-owned resources/projections without deleting pending save journals. Presence is opt-in; local Trainer/provenance stays distinct. No QML protocol, unlimited sync or render-thread I/O. |
| [#101](https://github.com/EriArk/TrainerOS/issues/101) | Rightmost Social: Friends / Chats, simple list/conversation navigation, requests/accept/decline/remove/block, permitted reporting; private/group text and emoji, membership, message edit/delete, paginated history, owner-scoped drafts/scroll. Muting/DND/private previews, non-stealing notifications, lazy virtualized media and Unicode/RTL checks. Existing Home quick access shares services; no second messenger or mandatory voice dashboard. |
| [#102](https://github.com/EriArk/TrainerOS/issues/102) | Supported pictures: choose/preview/send, safe bounded decode, orientation/metadata stripping on copies, lazy cache. Explicit recorded voice: record/stop/preview/send/discard, native provider format/codec/waveform proof, limits and recovery. Recording and live calls share device ownership, not lifecycle; no accidental broadcast/capture/autoplay. Cancel/logout/input/device loss clean up temporary media; upload success alone is not message success. Ordinary Fluxer-client round trips required. |
| [#103](https://github.com/EriArk/TrainerOS/issues/103) | Simple DM/group calls, one local session, explicit Join/Leave/Mute/output and compact participants/call state. Current deployed native voice contract only; grants/audience identity and same-account multi-device handling. Group removal is not proof of call ejection: test actual membership, stop local capture/rejoin safely where audience cannot be enforced. Preserve mute/devices, launch/return and explicit consent on owner/logout/suspend transitions; no assumed E2EE or unnecessary call features. Chat can ship independently. |

## Activities and delivery

| Issue | Preserved acceptance and dependencies |
| --- | --- |
| [#104](https://github.com/EriArk/TrainerOS/issues/104) | Reuse existing Link connection/activity consent; authenticated versioned invitation dispatches to **native system activity** or **runtime multiplayer** independently. Bind actual author/instance/user, local owner, endpoint, session, audience, expiry and minimum descriptor; reject replay/duplicates/wrong-owner/device/history. Coarse capabilities before acceptance; exact final offer approval remains separate. Safe ordinary-client fallback; no hidden provider opcodes, ROM/save/path/command metadata, automatic launch/download or microphone. Declining retains valid connection; teardown preserves unresolved journals. |
| [#109](https://github.com/EriArk/TrainerOS/issues/109) | Small adapter-driven system-activity contract composed from working Emerald Link, not a rewrite/framework. Versioned namespaced rules/schema/build pairs, participants, effects, readiness/revisions, bounded payload/recovery and semantic offer/result. Adapters have no sockets/auth/host powers; host retains transaction/write authority. Explicit conversion for cross-game exchange; no universal Pokémon schema/currency. Non-Pokémon synthetic activity proves extensibility, not real-game support. Works with game closed, independently of voice/netplay/virtual LAN. |
| [#105](https://github.com/EriArk/TrainerOS/issues/105) | Evaluate **separately** low-rate native events and gameplay packets. Research permitted supported message/attachment envelopes, actual Fluxer/media data grants, or signalling/direct/authorized relay. Test normal-client UX, notifications, retention/confidentiality, author binding, deletion/edit/history/replay and operation IDs beyond provider dedup windows. No frame/keepalive spam, hidden fields, homemade crypto or assumed public capacity. Measure actual workload/resource/latency; server route is relay, ACK is not save commit. Research does not authorize operating a production relay/fork. |
| [#110](https://github.com/EriArk/TrainerOS/issues/110) | First online system vertical: #99/#100 + #104/#109 + suitable #105 delivery (or necessary #106). Existing exact Emerald trade/gift first, then bounded already-supported sales/battle side effects. Same native UI/transaction engine, no running emulator; local IDs/filenames need not match. Two real devices on separate internet connections, bilateral receipts and normal-game readback. Delayed/reordered/duplicate events, source changes, logout/restart, one-sided commit and later recovery retain durable journals and restrictions. No unilateral rollback, automatic offline trading or claimed native-link parity. |
| [#106](https://github.com/EriArk/TrainerOS/issues/106) | **Conditional** native encrypted/authenticated pair transport when first consumer needs it. One mature selected route after ADR/ARM64/license/operations proof, not four stacks. Bind invitation/device/activity; bounded reliable events/datagrams/backpressure and replay checks. Different NATs/CGNAT/IPv6/restricted UDP/authorized relay and recovery evidence; real relay operator/quotas/expiry/cost/privacy. No broad VPN/router/firewall changes, custom cryptography or public relay promise. Suitable #105 events can unblock #110 before full P2P. |
| [#107](https://github.com/EriArk/TrainerOS/issues/107) | Separate exact-game/runtime online multiplayer proof: supported upstream mode, compatible versions/content/options, safe host/join, actual play and clean return on two different internet connections. Reuse runtime networking where it already works; use #106 only where needed. Define host/guest persistent save authority, temporary config recovery and ordinary-save lifecycle. Native TrainerOS battle/trade is not emulator netplay evidence. Virtual LAN only for a proven actual LAN requirement; no automatic ROM distribution, broad forwarding or every-game tuning. |
| [#108](https://github.com/EriArk/TrainerOS/issues/108) | Applied throughout, with independent messaging/media/voice/native-activity/runtime gates. Ownership/privacy/hostile input/replay/duplicate devices, reconnect/rate limits/uncertain sends, capture/audio lifecycle and durable transaction recovery. Numeric Flip/Odin baseline/budgets for idle/start/frames/typing/voice/gameplay/cache/long sessions; lazy hidden work. Ordinary Fluxer interoperability and real internet route proof. Sanitized allowlisted diagnostics, migration/rollback/disablement. Isolated automated networking; local/offline use survives unavailable features. |

Bluetooth remains the nearby default; LAN remains independent. Never automatically
fall back to Wi-Fi Direct scanning. The Odin freeze cause, physical Wi-Fi-off
proof, longer reliability and stronger #93/#94 trust remain open. A social account
is not proof of save provenance, and a paused/covered game is not safe to edit.

## Delivery record

The #111 increment changes shell routing/presentation only. It does not claim a
Fluxer login, live friends/chat, Home activity overlay, appearance application,
internet settlement, voice or emulator multiplayer. Native and device verification
is recorded in [navigation migration](NAVIGATION_111.md).
