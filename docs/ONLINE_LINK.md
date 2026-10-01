# Online invitations and bounded Link delivery

## Scope, 2026-10-01

This increment composes #104/#109/#105 with the existing Emerald Link controller.
In a friend's DM, Options / Together checks the **running client**, then offers
only matching activities. One compact Accept/Decline invitation opens the existing
portrait workspace. Invitation consent is separate from both final offer
confirmations. There is no ROM launch, download, voice activation or new setup page.

The current host advertises exact English Emerald exchange, sale and gift only
when its existing verified adapter observation is ready and saves are writable.
Descriptors independently identify `systemActivity`, namespaced action/version,
exact build, individual schema, rules, two participants and bilateral save effects.
No `runtimeMultiplayer` or online battle capability is advertised. Other games
remain unsupported by this consumer; the envelope itself has no species schema.

The native Link UI, verifier, prepare/commit/finish journal and backend remain the
only save operation path. No second trading implementation or online save writer
was added. Provider delivery success is never a save-commit acknowledgement.
Bluetooth/LAN remain independent; this route does not start Wi-Fi Direct.

## Carrier decision and limits

The bounded candidate carrier is documented ordinary Fluxer DM messages, received
through `MESSAGE_CREATE`. Sources checked 2026-10-01:
[message API](https://docs.fluxer.app/http-api/messages/),
[Gateway events](https://docs.fluxer.app/gateway/events/),
[rate limits](https://docs.fluxer.app/topics/rate-limits/) and
[terms](https://fluxer.app/terms).

This is a client-side convention in visible message content, not a Fluxer custom
opcode, hidden field, native data-channel grant or claim of provider endorsement.
Messages have understandable fallback text in ordinary clients. TrainerOS renders
compact status lines and collapses adjacent protocol updates. No periodic chat
heartbeat or contact-wide scanning runs. A capability probe requires explicit
Together selection in one existing friend DM; accepted recipients can reply.
Protocol sends set the documented suppress-notifications flag and disable
mentions; they do not create TrainerOS message toasts. Provider unread state is
still real and is acknowledged through the existing conversation route.

The client caps content at 2,000 characters, one request in flight, a minimum
one-second gap after successful sends and a 32-message queue. Native frames are
at most 12 KB, base64-fragmented into at most 16 parts. Receive reordering is
bounded to eight frames. Provider rate limits, DM privacy, block and relationship
rules still apply. Failure/429/uncertain delivery ends this transport rather than
blindly repeating a transaction. Explicit cancellation drops unsent data.

This low-rate route is not an emulator frame transport or a capacity promise.
Activity data is ordinary **provider-stored DM content**, protected in transit by
the provider's HTTPS/WSS, not end-to-end encrypted. Selected semantic offers and
receipts are shared after consent; whole saves, ROMs, credentials, filesystem
paths, host owner IDs and local Adventure registration IDs are not sent. Remote
errors are generic; actionable local errors stay local. Deleting a DM message
does not undo a save transaction or delete its durable local recovery journal.

## Identity, lifecycle and recovery

Only live ordinary one-to-one friend messages reach the engine. History, edits,
webhooks, groups and communities never execute envelopes. Actual provider author
and channel are checked, never trusted from content. The fixed provider identity,
account and persistent per-local-owner endpoint derive a stable Link participant
ID. A fresh login/process boot UUID, explicit target, random request/session IDs,
90-second envelope expiry, bounded replay history and sequenced fragments prevent
old/duplicate/wrong-device messages from opening or executing a new session.
Multiple devices on one account are addressed separately. Conflicting fragments
end the connection. Names remain untouched; account/community labels are not
compatibility or binary-authenticity proof.

Protected surfaces/runtime activity make new invitations unavailable. Logout,
owner change, Gateway loss and queue failure disconnect the online transport;
ordinary chat can recover independently. Inactivity expires after ten minutes;
an incomplete frame expires after 90 seconds. Reconnection is explicit and uses
the same persistent peer identity to recover an unresolved exchange. Existing
local journal/backup/read-only/runtime/source-revision safeguards remain in force.
An idle, disconnected pending exchange permits primary-page browsing so either
peer can reach Social and explicitly reconnect. Connected settlement, runtime
launch, owner/Adventure changes and save operations keep their existing gates.
Neither a declined activity nor page changes tear down an otherwise accepted
connection. Unresolved saves are never unilaterally rolled back by this carrier.

This retains the existing casual-session trust boundary. Authenticated Fluxer
accounts do not prove save provenance or genuine binaries. #93/#94 trust gates,
broader #109 game contracts, online battle and #107 runtime netplay remain open.

## Acceptance boundary

Automated coverage exercises consent, author/channel/audience/endpoint binding,
stale history and edits, duplicate/reordered/partial frames, expiry, simultaneous
probes, incompatible descriptors, read-only availability, delivery failure and
the existing native workspace/recovery path. It does not replace physical proof.

#110 remains open until two
real handhelds on separate internet connections complete a protected transaction,
normal-game readback and interrupted-settlement recovery. A synthetic peer or
successful invitation must never be reported as that full acceptance.

## Delivery evidence, 2026-10-01

- Final Windows build and ARM64 production build succeeded. Seven focused CTest
  targets passed in 26.34 seconds: online_link, social, link_peer, core,
  interactions, qml_smoke and exit_qml_smoke. Adapter knowledge/export check passed
  for all three exact builds; pure save adapters and their reusable copies did
  not change in this increment.
- Flip's installed/live SHA-256 is
  `f29b8a8896fc647ecf8bb31b6ec356dd02f6634de9cd5065bb7c4a92b2fd0ba5`.
  Delivery preserved three Trainers, 830 Adventures, boot settings and nearby
  helpers, with no active emulator or pending Link transaction.
- Public Fluxer used the owner's two designated test accounts. Flip ran the
  installed shell; an isolated Windows native test peer used the same provider
  and OnlineLink implementation, authorized by official browser handoff. This
  peer advertised a synthetic matching capability and had **no save backend**.
- Actual controller input on Flip exercised capability selection, outgoing
  invitation/remote decline, incoming invitation outside Social, A acceptance,
  direct entry to saved Party portraits, one semantic offer, cancellation,
  subsequent activity invitation/decline, page switching and direct return to
  the existing session through Together. The final build repeated invitation
  acceptance and the return path. Normal companion/chat state remained usable.
- The live selected-member offer was 1,214 compact JSON bytes, delivered in two
  bounded fragments. Control frames observed were 29/50/82 bytes. There were no
  periodic protocol keepalives or blind resends. This proves a small real workload
  through the public service, not a latency/resource benchmark or production
  capacity guarantee. Longer workload, normal-client notification/retention and
  interrupted-commit measurements remain in #105/#108.
- No final exchange confirmation or save commit was performed in this proof.
  Automated tests cover wrong peer/recovery restrictions; actual one-sided
  commit/restart recovery and normal-game readback still need the two handhelds.
- Actual compositor captures are private `work/research/online-incoming.png`,
  `online-final-actions.png`, `online-final-workspace.png` and
  `online-final-return.png`. No internal preview is presented as device evidence.
- Odin SSH timed out. Its deployment and separate-internet two-handheld proof
  remain deferred. Account names containing Odin are not evidence of a running
  Odin handheld. The production artifact is retained for that next delivery.

## Two-handheld follow-up, 2026-10-01

- Odin was positively identified after reboot, updated and started in TrainerOS.
  Its original Steam boot preference, existing library and saves were preserved.
  Official browser handoff connected its designated public Fluxer test account.
- Actual Flip and Odin controller input exercised explicit Together selection,
  an incoming invitation, acceptance into the existing portrait workspace, and
  both saved-member offers (Swellow and Alakazam). The review displayed both
  proposals. Cancellation and explicit session disconnect left no pending journal.
- This did **not** confirm a save exchange. Flip fell from 15% to 12% while
  discharging; final save writes and fault injection were deferred pending power.
  Fresh private backups were verified. Both current save hashes still matched
  their pre-test values. Both devices used the same home internet connection;
  separate-internet proof remains open.
- Source review found that a pending journal locked both peers out of Social
  after disconnection. Primary-page navigation now permits reaching chat in that
  idle recovery state. Focused tests verify the exception and retained runtime,
  Home menu, Adventure selector and connected-settlement gates. This is automated
  regression evidence, not completed physical interrupted-commit recovery.
- Windows and ARM64 production builds passed. All seven focused CTest targets
  passed in 26.55 seconds: link_peer, online_link, social, core, interactions,
  qml_smoke and exit_qml_smoke.
- Both installed **and live** executables now match SHA-256
  `7760f20ce6ec62df6fe00b0a5b4a8803440817fa0797d88c899025c08e5a138e`.
  Flip retained three Trainers/830 Adventures; Odin retained one Trainer/25
  Adventures. Database checks, boot preferences, nearby helpers and InputPlumber
  were verified. Both native logins survived the final app restart. The final
  build repeated a real invitation and decline between the two handhelds.
- Actual compositor captures remain private under `work/research/`:
  `pair-flip-review.png` (two offers, before confirmation),
  `pair-final-incoming.png` (final-build Odin invitation), and
  `pair-flip-final-chat.png` / `pair-odin-final-chat.png` (remembered login).
  No synthetic save or host-rendered image is presented as handheld proof.
- Next remains final bilateral settlement, normal Emerald readback and controlled
  interruption/reconnection on stable power, then separate-internet validation.
  #110, the #105 traffic matrix and remaining #109 contracts stay open.
