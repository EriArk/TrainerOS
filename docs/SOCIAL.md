# Native Social - conversations and discovery

The 2026-10-02 [media, calls and active-game notifications increment](SOCIAL_MEDIA_VOICE.md)
adds native attachments, the LiveKit call worker and shared Home call controls.
[#113 Reviews](ADVENTURE_REVIEWS.md) uses the same account provider from Adventure
Properties. Those documents separate delivered behavior from remaining live and
distribution gates; earlier acceptance below remains binding.

The [online Link increment](ONLINE_LINK.md) adds active-client checks and compact
friend-DM invitations. Accepted exact Emerald activities reuse the native Link
workspace; final bilateral save approval remains separate. The linked evidence
distinguishes public invitation/offer delivery from two-handheld settlement.

2026-10-01. Supersedes the unlinked #111 placeholder. The owner's refinement
uses four cyclic faces: Messages / Groups / Communities / Search. The five
primaries and Start/Home responsibilities remain unchanged.

The subsequent [native Community slice](SOCIAL_COMMUNITIES.md) adds creation,
owner-pinned versioned discovery identity, All/TrainerOS filtering, public tag
search and ordinary invite links. Its marker is independent of game/client
capabilities. Shared activity invitations remain the next #104/#109 increment.

## Implementation

FluxerSession owns supported public-instance browser handoff, REST and Gateway
on a separate Qt event-loop thread. UI receives only presentation snapshots.
SocialController owns list focus, the retained conversation and account-scoped drafts;
ShellController routes physical actions and the existing controller keyboard.
No browser cookies, bot credentials, password scraping or provider webview.

The landscape layout keeps people/conversations on the left and the selected
conversation on the right. Text uses plain-text rendering. L2/R2 wraps the four
faces; L1/R1 keeps its primary meaning. A writes in the selected conversation
(X remains a compose shortcut); the keyboard's Send key or Select sends directly.
B keeps an unsent draft. Its optional emoji layout shares the same keyboard;
X changes case and Y cycles symbols/emoji/letters. Y in a friend DM opens Together
directly, with a matching attached composer action. Right reads/scrolls messages,
left returns to the people list. Back
returns focus to the list without clearing the conversation. Options holds the
less frequent account/friend actions. Home Friends opens the contacts view in
Messages; Home Chats opens Messages. Existing in-game
Home Continue/Exit is preserved; live in-game chat overlay is a separate gate.

At the oldest visible message, Up fetches the preceding page in the same view;
Y returns to the latest messages while reading older history. Selection stays on
the same message after prepending. Pages contain up to 50 messages, with a moving
100-message display window rather than retaining an entire account in memory.
Gateway events do not pull the reader out of older history. Uncertain sends remain
tracked even when their display row falls outside that window.

The first slice supports approved browser sign-in, friend requests by exact
Fluxer tag, accept/decline/cancel/remove/block/unblock, opening DMs, existing
group conversations, joined-community text channels, recent plain-text history,
sending and realtime incoming messages/edits/deletions. A community expands its
visible channels inside the left list, not into another full-screen page.
Messages restores the selected conversation per local Trainer and Fluxer account;
without a remembered selection it opens the most recent available conversation,
then the first accepted friend when no conversation exists. Groups and community
channels keep separate choices. Highlighting a row loads its conversation after
a short selection debounce; it never accepts a friendship automatically.

Search occupies the full content width, with People / Communities / Invite link
categories and a two-column result grid. Communities use the actual public
discovery endpoint with query and 24-result paging. People use Fluxer's exact
username#1234 friend request, not an invented partial-name user directory or a
fake resolved profile. Private groups/community invites are looked up through
the supported invite endpoint. Joining or sending a request requires explicit A;
browsing never changes membership. Existing friends are not search results.
Late query/action responses cannot replace a newer search's state.

## Groups and own messages

Groups uses a compact friend picker: A toggles people and Y creates the group.
An empty Groups face exposes New group directly on X; existing groups keep it in
Options alongside rename, add friend, owner-only removal and leave. Any current
group recipient may rename it. The provider owns privacy/admission and capacity;
there is no separate TrainerOS friendship policy. Member lists refresh after changes.
The controller-scrollable popover stays inside the current conversation.

Right enters message reading; A on one's own ordinary text message opens Edit /
Delete. Editing uses the existing controller keyboard and sends only content,
preserving attachments/embeds. A cancelled edit does not send anything; its draft
stays in memory for the same message/account. Delete, leave and member removal
replace the same popover with a Cancel-first confirmation. Uncertain mutations
are never retried automatically. Pending or unsupported messages offer no edit.

Public Fluxer now advertises ALTCHA v2. An explicit CAPTCHA_REQUIRED rejection
can complete the documented PBKDF2/SHA-256 proof in short worker-loop slices and
retry with X-Captcha-Token. Challenge parameters/signature are preserved; tokens
never reach QML, persistence or logs. INVALID_CAPTCHA permits one fresh challenge
retry; network uncertainty never does. Profile changes cancel the solver. Work
has a 90-second deadline and bounded input/cost. Unsupported challenges remain
actionable errors, not invented successful groups. Sources:
[Fluxer challenge contract](https://docs.fluxer.app/topics/captcha/) and
[ALTCHA v2 reference](https://github.com/altcha-org/altcha-lib-go/blob/main/v2/altcha.go).

## Read state and notifications

READY read_states and MESSAGE_ACK synchronize the active account's read watermark.
Loading history in the background no longer clears unread. Only a visible,
unobstructed conversation at the bottom of its rendered log requests the supported
batched read-state ACK; runtime, keyboard, service and protected modal gates take
priority. A newer Gateway ACK wins over an older REST response, including manual
mark-unread. This is current-account synchronization, not a peer read receipt.
See [read states](https://docs.fluxer.app/http-api/read-states/).

The Social tab shows the number of unread conversations, not a fabricated exact
message count. New messages can display one compact 4.5-second toast without
focus/input capture. Open chats, blocked authors, own/replayed messages, muted
conversations and local Do Not Disturb suppress toasts; covering the surface
discards a toast rather than replaying private content later. In-game delivery
retains the badge for return; this increment does not claim external-game overlays.
Options contains Mute conversation, Do not disturb and Private notifications.
These local presentation preferences are scoped to Trainer plus Fluxer account;
they do not pretend to change Fluxer presence. Private notifications default on,
showing generic text; opting out enables sender/message previews. Muting never
marks a conversation read.

## Sessions and recovery

The selected local Trainer scopes each session and every asynchronous result.
Owner changes close the old worker's network session and immediately clear the
private view/drafts. Credentials stay in the worker; QtKeychain has insecure
fallback disabled. Windows uses its native protected store. On Linux an already
running secret service/wallet is preferred. Armada gaming sessions without one
use asynchronous `systemd-creds --user` host-key encryption. The encrypted file
is scoped to the local Trainer/public provider; systemd binds decryption to the
host and OS user. It is atomically written with owner-only permissions. Input
travels through stdin, never shell arguments; the fixed executable runs as the
ordinary session user, with a timeout. No desktop wallet unlock dialog is started.
This is host-key protection, not a TPM/physical-theft claim. An unavailable or
failed protected store leaves an explicitly session-only login, never plaintext.
Startup verifies the restored token, retries temporary connection failures and
removes rejected credentials. Owner changes cancel outstanding credential jobs.
Options reports whether the session is remembered. No token is stored in the
ordinary database, screenshots, logs, ROMs, save backups or command arguments.

Logout calls the supported revocation route. An offline/failed revocation stays
visible and retains the session for a retry rather than claiming success.
Successful logout clears private session data and deletes its protected entry
and account drafts. Drafts use a separate atomic owner-readable local JSON cache,
keyed by provider/local Trainer/verified Fluxer account, with at most 128 drafts
of 2000 characters. They survive restarts and Trainer switching; another account
cannot inherit them. They are not encrypted message-history storage. No history
archive is persisted in this increment.
Heartbeat ACKs, bounded backoff/jitter and Gateway Resume reconnect the client;
new sessions also refresh bounded REST views. Provider retry delays are honored.
Messages carry a provider-supported 32-character nonce; an uncertain HTTP result
is shown as uncertain and never automatically resent. Server echoes reconcile
pending messages. Late responses from a previous owner/session are ignored.
A history reply cannot overwrite messages changed by newer Gateway events.
No activity/title/ROM/save data is automatically published to Fluxer.

## Historical #100/#101 acceptance register

The following list records the earlier text slice, not current delivery status.
Emoji entry and native community creation/filtering were delivered in the later
sections; current history/retry work and completed paired delivery are recorded
under **Everyday communication — block 2**. Preserve the acceptance:

- durable account-scoped history/scroll, emoji picker and wider Unicode/RTL proof;
  drafts, bounded history pagination and current-account read-state sync are delivered;
- richer friend lookup where supported and avatars; group/member controls retain
  their actual live acceptance and provider limitations recorded below;
- native Community creation and durable supported TrainerOS-aware discovery/filter,
  per updated #98/#101; research with #104/#109 before picking a representation;
- Odin proof of remembered credentials, and longer reconnect/rate-limit/
  large-account validation; Flip proof is recorded below;
- live in-game Home messenger presentation without disturbing emulator input;
- #104/#105 supported compatible-TrainerOS recognition and accepted game offers,
  #109/#110 native online activities, #102 media, #103 calls and #107 runtime play.

Existing group/community access follows Fluxer permissions; this is not a guild
administration clone. No new account-name markers or invisible profile suffixes.

## References

- [Authentication](https://docs.fluxer.app/http-api/authentication/)
- [Relationships](https://docs.fluxer.app/http-api/users/relationships/)
- [Private channels](https://docs.fluxer.app/http-api/users/private-channels/)
- [Channels and group membership](https://docs.fluxer.app/http-api/channels/)
- [Messages](https://docs.fluxer.app/http-api/messages/)
- [Guilds](https://docs.fluxer.app/http-api/guilds/) - Get guild returns visible channels.
- [Gateway](https://docs.fluxer.app/gateway/events/)
- [Community discovery](https://docs.fluxer.app/http-api/discovery/)
- [Invitations](https://docs.fluxer.app/http-api/invites/)
- [Earlier native feasibility proof](FLUXER_SPIKE.md)
- [systemd credential command contract](https://github.com/systemd/systemd/blob/main/man/systemd-creds.xml)
- [systemd credential protection](https://github.com/systemd/systemd/blob/main/docs/CREDENTIALS.md)

## First text increment verification (historical)

- Windows: native build and focused `social`, `core`, `interactions`, `qml_smoke`
  and `exit_qml_smoke` all passed on the final source (5/5, 27.59 seconds).
  Synthetic tests cover distinct Search/Add actions, stale owner results, cancelled
  handoff, provider backoff, uncertain-send deduplication, history/delete races,
  logout, bounded history and owner-isolated drafts.
- ARM64: production build on Flip; earlier social/core/interaction tests passed in
  the same build container. Final installed binary SHA-256:
  `66a76900e82e7fdc2a5c88374b1836f5186275a5265085292621b0f2a4b9b5c5`.
- Actual Flip: official browser handoff into the designated Flip test account,
  real friend/DM list, controller keyboard and outgoing `HI`, independently seen
  in the designated Odin account in the official Fluxer web client. Native logout
  completed and removed the connected view; reauthorization on the final binary
  restored real history. L2 from Messages wrapped to Search/friends. This is not two-handheld proof.
- Actual-device captures are private `work/research/flip-social-*.png`, not host
  smoke renders. Deployment verified the live executable plus unchanged 3 Trainer
  profiles / 830 library records, boot configuration and nearby helpers.
- Odin: initial binary was copied but the old process remained stuck in a GPU
  `dma_fence_default_wait` kernel wait before it could launch this increment.
  Session restart did not recover it; after a normal remote reboot request SSH
  became unreachable. Final polished build is not installed there; live acceptance
  remains open. This is not evidence that the new Social client froze Odin.
- At that delivery Flip had no available system credential vault: sign-in was
  session-only. The later persistence increment below supersedes this limitation.
- Receiving/edits/deletions, group/community and recovery paths have implementation
  and synthetic/earlier-spike coverage, but no current two-handheld live claim.

## Automatic conversation and discovery correction - 2026-10-01

- Final Windows native build passed; focused `social`, `core`, `interactions`,
  `qml_smoke` and `exit_qml_smoke` passed 5/5 in 25.35 seconds.
- Production ARM64 build installed and live executable verified on Flip:
  `b8706982a64afdfbe15e6ed4b0299a9e17a52c4885be3ebc3a5a0bddfe3fb6ad`.
  The same 3 local Trainers / 830 library records, boot configuration and nearby
  helpers were preserved; no emulator or pending Link settlement was active.
- Real official handoff restored the designated Flip account. Entering Messages
  opened the existing Odin-test conversation/history without A. B and a primary
  tab round trip retained it. Reauthorization after a binary restart also opened
  the remembered conversation directly.
- Search has no friend sidebar. The actual public directory loaded 187 results
  at verification time; entering GAME with the physical-controller keyboard returned
  18 matching communities. Empty browsing omits the optional query parameter: the
  deployed API rejected an explicit empty `query=` despite the current schema.
- Actual device captures: private `work/research/chat-final.png` and
  `work/research/search-final.png`. No random public community was joined and no
  message/request was sent to an unrelated account during this correction.
- Synthetic coverage includes latest/remembered conversation selection, Back,
  account isolation, stale search and membership callbacks, encoded query/paging,
  omitted empty query and lookup-versus-explicit-invite-join behavior.
- At that delivery Odin SSH still timed out; this correction was not installed
  there. Persistence was subsequently addressed in the following increment.

## Remembered login, drafts and history - 2026-10-01

- Final Windows native build passed. Focused `social`, `core`, `interactions`,
  `qml_smoke` and `exit_qml_smoke` passed 5/5 in 27.55 seconds.
- Production ARM64 build installed and live executable verified on Flip:
  `77b569c4ae6a62493d77924b7a396e784c634954ab6202dbe4ce911230a2e3d3`.
  Same 3 Trainers / 830 Adventures, boot configuration and nearby helpers preserved.
- Actual official-browser handoff created a 292-byte encrypted credential with mode
  0600. A controller-entered unsent draft was stored separately with mode 0600.
  Terminating/restarting TrainerOS restored the verified account, selected DM and
  draft without another handoff. Explicit logout revoked the session and removed
  both credential and draft files; reauthorization restored server history without
  the old draft. No plaintext real token was extracted during verification.
- The real test DM's beginning-of-history request completed without duplicate rows
  or a stuck Loading state. It has only one message: multi-page ordering/window
  bounds and concurrent-event behavior use synthetic tests, not fabricated live
  traffic. No messages or requests to unrelated accounts were sent.
- Synthetic coverage adds account-isolated draft restart/logout, earlier-history
  selection, moving 100-message windows, Latest focus and pending-send reconciliation
  after paging, alongside existing stale-owner/delete/backoff coverage.
- Actual-device captures are private `work/research/persistence-final-restored.png`,
  `persistence-options.png`, `persistence-history.png` and `persistence-final-search.png`.
  The final build also passed another login/draft restart and L2/R2 Search round
  trip; the public directory returned 186 results at that check.
- The same ARM64 artifact is retained locally for Odin. Its delivery and paired
  live tests wait for the owner's reboot; this is not two-handheld acceptance.

## Group and message controls - 2026-10-01

- Final Windows native build completed; focused social/core/interactions/QML/exit
  checks passed 5/5 in 27.07 seconds. New tests cover own-message authorization,
  content-only edits, deletion racing an edit response, serialized mutations,
  explicit group multi-select/create, owner-only removal, recipient rename,
  leave without message deletion, provider rejection and stale channel refresh.
  Channel refreshes are coalesced so Gateway bursts do not fan out HTTP requests.
- Production ARM64 installed and live executable verified on Flip:
  `a22568ee7d6031664459d1b851e1c9a31e7f5b4372dd882f48f8f270e87116fd`.
  Preserved 3 Trainers / 830 Adventures, boot preference and nearby helpers;
  no active emulator or pending Link settlement during delivery.
- Actual controller proof: existing test DM reopened after restart; editing HI
  to HIA appeared as edited in the designated Odin account's official web client.
  Cancelling the keyboard kept the in-memory edit draft without sending it.
  Cancel-first deletion was dismissed once, then explicitly confirmed: the
  message disappeared in both native and official clients. A fresh HI from the
  pre-existing composer draft was then sent to retain a useful test conversation.
- Group picker/selection/rendering works on Flip, but the public instance rejected
  native group creation with CAPTCHA_REQUIRED. No group was invented or challenge
  bypassed. Native supported challenge completion is an OPEN integration gate;
  group creation/addition cannot be called end-to-end ready on this instance.
  Rename/member removal/leave currently have contract/synthetic coverage only.
- Actual Flip screenshots: private `work/research/management-group-picker.png`,
  `management-final-message.png`, `management-delete-confirm.png` and
  `management-final-chat.png`. These are compositor captures, not host mockups.
- Odin SSH timed out. The exact ARM64 artifact is retained for its next delivery;
  official-browser observation of the Odin test account is not two-handheld proof.

## ALTCHA/read-state follow-up proof - 2026-10-01

This supersedes the creation challenge blocker above. The live discovery document
advertises ALTCHA; an older cached documentation result still described different
providers, so the current public documentation and reference solver were checked.

- Controller-only Groups / X / friend A / Y successfully created a real group on
  Flip. The approved Odin test account saw the two-member group in the official
  Fluxer web client. No browser challenge interaction was needed for creation.
  Addition/removal/rename/leave retain contract tests, not new physical proof.
- Real incoming emoji from the designated peer produced a private toast and
  one-conversation badge while another conversation/Home was displayed. The
  selected page/focus stayed intact. Opening Messages displayed the actual emoji;
  successful read acknowledgement cleared the badge. Restart preserved read state.
- Options was exercised with physical-controller events; Do Not Disturb survived
  a production restart. It was restored to Off afterwards; private previews remain
  On. A final incoming message on Home verified the corrected unobscured badge.
- Visual review caught the badge under the chassis rim; QML smoke caught a
  presentation-notification binding cycle. The badge was moved below the rim and
  presentation signals were separated from Shell navigation before final delivery.
- Final Windows/ARM64 builds succeeded; social (31 cases), core, interactions,
  qml_smoke and exit_qml_smoke passed, 5/5 CTest targets in 25.15 seconds.
- Final installed Flip SHA-256:
  `09625e820391c6603fc537a0d119fff7f32b0932a5277c64cf6bc12dfe858c13`.
  Deployment retained three Trainers, 830 Adventures, boot configuration and
  nearby helpers, with no active runtime or pending settlement. Private artifact:
  `work/research/social-read-production`; compositor captures:
  `read-final-toast.png`, `read-final-options.png`, `read-group-result.png`.
- Odin remained unavailable by SSH. Its deployment and two-handheld acceptance
  remain deferred; the official test-account web client is only peer evidence.


## Communication completion order

Owner clarification, 2026-10-01: finish the accepted Fluxer feature set and make
communication familiar and direct. This is part of the existing roadmap lane,
not a new feature tree and not permission to call partial implementations complete.

| Order | User result | Remaining acceptance |
| --- | --- | --- |
| 2 — delivered | Reliable everyday conversations | Friends, DMs, groups, communities, supported search, retained history/scroll, failed-send retry and reconnection; direct writing, emoji entry, drafts, actual provider permissions and both-device controller proof. See the block 2 evidence below. |
| 3 — in progress | Notifications and calls | Background-call recovery and Home controls, dismissible notifications and passive activity invitations delivered below. Finish the remaining microphone/interoperability gates in SOCIAL_MEDIA_VOICE before closing this whole block. |
| 1 | Actual emulator multiplayer | #107 exact runtime and route proof with in-game invitations; #106 only if its transport needs it. Preserve #112 appearance-control acceptance. |
| 4 | Native online Link | #104/#105/#109/#110 transport, privacy, workload, explicit invitation and separate-internet gates; preserve protected save operations. |
| 5 | Reviews and final communication audit | Finish #113 inside game Properties and review the complete journey without extra navigation layers. |
| Throughout and at completion | Familiar controller-first communication | One conversation workspace; obvious direct actions; no duplicate selection/apply/send pages; consistent Back, footer hints, retained drafts/focus and account isolation. End with an actual Flip/Odin walkthrough and screenshots. |

All #98-112 and R1-R18 acceptance stays in ROADMAP. Avatar/media presentation,
joined-community navigation, group actions, notifications and discovery need one
cohesive final visual/input pass; this small increment does not claim that audit
or the whole messenger is complete. Separate-internet proof must not be invented
from the current same-network devices.

Observed in the paired walkthrough: availability/invitation/exchange protocol
messages still occupy the ordinary timeline, and unread badges need reconciliation
with the actually displayed/read messages. Address these in the next conversation/
notification pass; keep the underlying invitation/session protocol intact.

### Notifications completion - owner addition, 2026-10-02

Deliver a coherent notification experience as part of the communication lane:
messages/mentions, friend requests and activity invitations. Existing transient
toasts, unread badges and Do Not Disturb are the baseline, not completion.

- Give missed notifications one compact, controller-accessible list and open the
  relevant conversation/request/activity directly from an entry, without another
  selection page. Respect actual Fluxer permissions and read/mention state.
- Keep unread counts, read/dismiss state and reconnect behaviour consistent;
  deduplicate live events and respect the active Trainer/account.
- Add understandable sound/private-preview/Do Not Disturb controls; receiving a
  notification must not steal focus, interrupt gameplay or accept an invitation.
- Verify live arrival, missed events, direct navigation and dismissal on Flip
  and Odin, including an active game. Keep the shell's existing Home/Start routes.

This is recorded acceptance for a later increment, not installed functionality.

## Direct composition and invitations, 2026-10-01

- A on the already displayed conversation opens its message keyboard. Send on
  that keyboard (or Select) submits once; no second Apply-then-Send step. X remains
  a compatible compose shortcut. Back retains the draft without transmitting it.
- The keyboard includes 26 standard Unicode emoji in its optional message/edit
  layout, reached through Y with the next layout named in the footer. Numeric
  keys remain on the right. Grapheme deletion keeps multi-codepoint emoji intact.
  Other name/password/search keyboards retain their ordinary layouts.
- Y and the attached Together action enter compatible activities directly from a
  friend DM. Options no longer duplicates it. Probing does not send an invitation;
  choosing an available activity does, and acceptance remains explicit. In older
  history, the existing Y Latest shortcut takes priority and the visible Together
  button remains available. Ordinary contacts keep ordinary chat.
- Message commands carry their intended channel. A provider-side channel change
  cannot redirect the text to a different person; the draft is retained instead.
  Blank messages and cancelled input are not sent. Ordinary message permissions
  and encoding still use the [supported message API](https://docs.fluxer.app/http-api/messages/).

This changes host presentation and input, not the Emerald adapter, transport
protocol or save transaction. Remaining completion work is listed above.

### Paired delivery evidence, 2026-10-02

- Final Windows and ARM64 builds succeeded. Social, core, interactions, qml_smoke
  and exit_qml_smoke passed: 5/5 CTest targets in 28.71 seconds. Coverage includes
  direct send, invitation consent, stale recipient protection and emoji drafts.
- Real controller events on Flip opened composition with A, cycled to the emoji
  keyboard with Y and entered a standard emoji. B returned without sending; A
  reopened the intact draft. Select sent once, and the same message appeared on
  the native Odin client. Odin sent a thumbs-up reply through the same keyboard.
- Y from the DM opened the available activities directly. Choosing Exchange sent
  the real invitation to Odin; B declined and returned to the existing chat. No
  offer/settlement was started and neither save was modified.
- Final production ARM64 binary installed and live SHA verified on both devices:
  `f2f95e2e989c5dd572da4092a09c357081ae4c079203f1ea36d9b2bbb7d3abc3`.
  Flip retains 3 Trainers / 830 Adventures; Odin 1 / 25. Boot choices and nearby
  helpers are unchanged; SQLite checks pass and no Link transaction is pending.
  Both exact Emerald save hashes still match the pre-increment values.
- Actual compositor screenshots are private `work/research/direct-final-keyboard.png`
  (Flip) and `direct-final-chat.png` (Odin); invitation/draft evidence is in
  `direct-incoming-odin.png` and `direct-draft-reopened.png`. These are installed
  handheld captures, not desktop mockups. The final revision also preserves long
  Unicode drafts without silent truncation when the provider rejects their length.
- Both devices were on the same ordinary LAN. Public Fluxer message/invitation
  delivery is proven here; this does not satisfy the separate-internet #110 gate.

## History and notification destinations, 2026-10-02

- Link protocol messages remain available to the transport, history paging and
  read acknowledgement, but no longer appear as conversation bubbles. Invitation
  consent and the activity workspace retain their own surfaces. Ordinary messages
  and provider system events remain visible.
- A separate server-message tail permits acknowledging a displayed conversation
  whose final rows are hidden protocol packets. Fresh protocol traffic and own
  posts do not create an unread badge on an otherwise read conversation, and do
  not clear earlier unread human messages. Manual Mark unread remains effective.
  Provider read state remains authoritative; implementation follows the official
  [read-state contract](https://github.com/fluxerapp/fluxer/blob/main/fluxer_docs/src/content/docs/http-api/read-states.mdx).
- Prepending history retains the selected message, including a one-message
  window. Latest scrolls to the bottom. This is not persistent scroll restoration
  across changing conversations or restarting the shell.
- Tapping a message toast, or opening physical Home / Chats while its destination
  is still unread, opens that conversation directly in its correct face. The
  destination survives toast expiry and opening Home, but is cleared on Trainer
  change. No invitation is accepted and no game is launched by this navigation.

### Installed evidence and remaining limits

- Windows and ARM64 builds succeeded. Social, core, interactions and qml_smoke
  passed. The exit smoke initially raced its fixture PID-file creation (three
  related assertions); its isolated rerun passed. No product fix was inferred
  from that test timing failure.
- On the actual handhelds, the DM shows ordinary emoji messages without protocol
  noise. Flip loaded older messages and returned through Latest; Together still
  resolved Odin's actual available activities after the filtered capability
  exchange. No invitation settlement or save mutation was needed in this check.
- Odin sent a group message while Flip was on Home. Flip showed unread, then
  physical Home / Chats opened that group directly; after presentation its unread
  badge cleared. The test message was sent with the controller keyboard.
- Both devices run production SHA
  `024e03ba602f7ffc312757cf901486d68eb8cfd40f2653d058ca32b3ec0674c0`.
  Flip retains 3 Trainers / 830 Adventures and Odin 1 / 25. Boot settings and
  helpers are preserved, SQLite checks pass, no Link transaction is pending,
  and both Emerald save hashes match the pre-increment values.
- Actual compositor captures: private `work/research/history-final-flip.png`,
  `history-final-odin.png`, `history-earlier-flip.png` and
  `history-notification-group-flip.png`. These are installed device screenshots.
- The destination is the latest eligible message toast, not a durable notification
  inbox. Missed-event lists, notification sounds, active-game delivery and the
  broader notification/reconnect acceptance above remain pending. READY rebuilds
  unread from provider truth; old unseen protocol traffic can remain unread until
  that conversation is presented. Protocol-only history pages remain pageable
  through the existing Earlier action. Separate-internet #110 proof stays open.

## Notification list and simpler composer, 2026-10-02

Owner correction: the composer contains only the message draft. The attached
Together button is removed; Y / Play together in the footer retains direct
activity selection and explicit recipient consent. Y / Latest still takes
priority when reading earlier history.

Physical Home now includes Notifications, with a compact list in the same overlay.
Unread DMs, groups and community channels come from the current Fluxer read state;
incoming friend requests come from its relationship state. Muted chats are omitted.
There is one row per conversation, not one row per message, and no private message
preview is retained in another local store. Signing in again reconstructs the list
from provider state. Switching Trainer clears the source snapshot immediately.

A opens the selected conversation in the correct Social face. A on a friend
request selects that person's request in Friends; acceptance remains a separate
explicit action. Merely opening Notifications does not acknowledge messages.
B returns to quick access; Home closes the overlay at its original page; Start
retains the system menu. Reading a conversation clears its row through the normal
provider acknowledgement.

This is an unread/request inbox, not a historical archive of dismissed alerts.
Notification sound controls, mention-specific presentation, expired activity
history and active-game delivery remain open. Existing activity invitations keep
their live Accept/Decline surface. The separate-internet route remains unverified.

### Notification-list device evidence

- Windows and ARM64 builds passed. Social, core, interactions, qml_smoke and
  exit_qml_smoke passed (5/5). Coverage includes empty-list modal navigation,
  provider unread/request filtering, owner reset and no implicit acceptance.
- Both handhelds run SHA
  `ae547c3501adb43918c9c197795acd9934058b12ec22d7c26b597dbad31136e9`.
  Final live binary hashes, SQLite integrity, profile/library counts, boot settings
  and nearby helper preservation were checked independently. Both exact Emerald
  saves retain their pre-increment hashes; no pending transaction exists.
- Controller-composed group messages were delivered in both directions while the
  receiving handheld displayed Notifications. Each list gained its unread row;
  A opened the correct group and ordinary presentation cleared unread. Keeping
  the overlay open over that same group did not acknowledge its new message.
- Empty and populated overlays were visually inspected. Height follows one to
  three visible rows; longer lists scroll. Actual final Flip captures are private
  `work/research/notifications-final-flip.png` and
  `work/research/notifications-composer-final.png`. The latter shows the composer
  without the removed button. Live friend-request creation was not repeated;
  source filtering, direct selection without acceptance and Trainer isolation
  are covered by the social test. This does not claim the full reconnect matrix.


## Everyday communication — block 2, 2026-10-02

The owner-defined order is 2 → 3 → 1 → 4 → 5, with one whole block per delivery.
This block covers friends, DMs, groups, communities, supported discovery, history,
manual failed-send retry and reconnection. Notifications/calls, runtime multiplayer,
protected online Link and reviews retain their separate blocks and gates.

### Implementation

- Conversations retain a bounded window and selected message across conversation,
  secondary-face and primary-page changes. Returning does not blank the chat while
  REST loads. Earlier history refreshes around the retained message using Fluxer's
  supported `around` cursor; the explicit Latest action returns to the live tail.
- The worker saves text history for at most 24 recently visited conversations,
  100 displayed messages each, 4 MiB total. Entries older than 30 days are discarded
  on load. Files use atomic replacement and owner-only filesystem permissions.
  This is a local text cache, not encrypted history or a permanent archive.
  No attachment bearer URLs, embeds, voice credentials or Link packets are saved.
- The cache belongs to provider + local Trainer + immutable Fluxer account.
  Offline cold restoration additionally requires the same token recovered from
  protected credential storage that last verified the account (only its digest
  binds the cache). A different login cannot open that cache. Rejected credentials
  and explicit logout remove the current cache; Clear local history is available
  in Social Options. Draft storage remains independently account-scoped.
- Gateway edits/deletions also update inactive cached conversations. Current REST
  history replaces stale remote text; access denial, channel removal and leaving
  a group remove corresponding history. Teardown clears any cache repopulated by
  intermediate voice/Link signals before the next Trainer receives a snapshot.
- A failed message offers Retry sending directly in its message menu. An in-flight
  retry cannot be submitted twice. Gateway acknowledgement wins over a late HTTP
  failure. Switching conversations while sending keeps the outcome attached to
  the original conversation; nothing is silently resent on reconnect/restart.
- Fluxer documents nonce deduplication for five minutes. A user-requested retry of
  unknown delivery reuses the original nonce only within a conservative four-minute
  window from the initial send. Afterwards Check delivery fetches history; there
  is no blind resend. Known rejected sends can be retried. Slowmode Retry-After is
  respected along with normal 429 guidance.
- Groups expose Members alongside existing create/name/add/remove/leave actions.
  They remain small popovers over the conversation; ordinary writing still takes
  one A and one keyboard Send. Avatar images load lazily from the public instance's
  documented media origin, with an initials fallback. Blocked authors are excluded
  from the message projection. Conversation previews follow existing privacy choice.

Contracts rechecked against the official [message API](https://docs.fluxer.app/http-api/messages/),
[channels](https://docs.fluxer.app/http-api/channels/),
[instance discovery](https://docs.fluxer.app/http-api/instance/) and
[avatar routes](https://docs.fluxer.app/media-proxy/routes/).

### Delivery gate

**Block 2 is delivered on Flip and Odin.** The owner's Odin reboot allowed the
final paired controller walkthrough recorded below. Both devices run the same
production artifact. Block 3 is next; its notification/media/call and in-game
Home acceptance is not implied by this delivery.

- Current Windows and ARM64 Social tests both pass: 60 cases on each, including
  the final member/cache cleanup. The ARM production build has testing disabled.
- Flip runs final production SHA-256
  `73a6402a3488837e8afe752bedaa807303a984a9b2816c96c8938da972ae877b`.
  The live executable hash, SQLite integrity, 3 Trainers / 830 Adventures,
  boot preference, InputPlumber, nearby helpers and absence of pending Link
  settlement were checked. Emerald save SHA remains
  `cf39ceece96e0b8804864a23fedb81bfdf6781ed3b560fff039ec21bf61666cf`.
- Flip controller proof: direct DM entry, one keyboard Send, reading position
  across DM/group face changes, Members including self/owner, joined community
  channel preview and Unicode text (Cyrillic, Hebrew, accented Latin, emoji).
  The multilingual test message used only the two designated test accounts.
  Full-width public discovery returned 186 actual community results; no public
  community was joined or contacted for this check.
  The group owner also removed the designated Odin test member through the
  controller confirmation and added that same friend back through the picker.
  The original group identity/history and mutual friendship were preserved.
- A temporary isolated nft table rejected user HTTPS for 40 seconds, leaving
  SSH intact, then removed itself. The native message stayed visible with unknown
  delivery; after recovery its explicit Retry action delivered exactly one new
  server message using the original nonce. No automatic resend occurred.
- A second bounded outage plus shell restart demonstrated cold offline history
  on the actual Flip screen, followed by automatic authentication/reconnection.
  Both temporary firewall tables were removed; no persistent network policy changed.
- Installed compositor captures (private): `everyday-retry-failed.png`,
  `everyday-retry-success.png`, `everyday-history-anchor.png`,
  `everyday-cold-offline.png`, `everyday-unicode-final.png`,
  `everyday-group-final.png`, `everyday-community-final.png` in `work/research/`.
- Before the owner's reboot, Odin accepted the first updated binary, but its old TrainerOS process remained
  a zombie with graphics-related kernel tasks in uninterruptible sleep
  (`gpu-worker`, SMMU fault handler, ring worker, `kwin`). No fresh compositor
  capture was available. The authorized normal reboot was requested; subsequent
  SSH probes timed out/refused. That attempt was **not** a verified Odin delivery
  or proof that the freeze is fixed. The successful recovery delivery follows.
  Do not attribute the kernel failure to a specific app change without evidence.

### Odin recovery and final paired walkthrough

- After the owner rebooted Odin, its model and idle runtime state were verified
  before replacing TrainerOS. It initially ran Steam. The installed session
  switcher started TrainerOS, then its temporary autologin override was removed
  to restore the exact previous boot preference. No game was stopped.
- Odin runs the same production SHA above, with 1 Trainer / 25 Adventures.
  Its login restored automatically from protected credentials. SQLite integrity,
  InputPlumber, nearby helpers, voice helper, boot preference and no pending Link
  settlement were verified after the walkthrough on both devices. Odin Emerald
  SHA remains `3fca83edc8bb3d69f5627a6ecec820069ca7fac8699676ecade2fd748ea47c7a`;
  Flip retains the save hash and counts recorded above.
- Controller-composed DMs and group messages arrived in both directions through
  the designated public Fluxer test accounts. A opens the displayed conversation's
  composer directly; Select sends once. Odin's draft survived B, a face change
  and reopening the composer. An emoji selected from its keyboard arrived on Flip.
- Odin's member popover shows self and the actual group owner. Highlighting the
  joined CLUB/general channel previews its history directly. Full-width public
  discovery displayed 186 results without joining or contacting public groups.
  DM selection and its retained message survived B, Groups/Messages cycling and
  leaving/returning to Social with L1/R1. Both handhelds remained responsive.
- Actual installed captures are private `work/research/everyday-odin-received.png`,
  `everyday-flip-received.png`, `everyday-odin-draft.png`,
  `everyday-flip-emoji-received.png`, `everyday-odin-members.png`,
  `everyday-odin-group-received.png`, `everyday-odin-general.png`,
  `everyday-odin-search.png` and `everyday-odin-primary-restored.png`.
- This completes the paired everyday-messaging gate. Controlled offline/retry
  evidence is from Flip above; rate limits, account isolation and bounded history
  use deterministic tests rather than deliberately rate-limiting the public
  service. The live accounts remain small test accounts, not a high-volume soak.
  The earlier Odin kernel/graphics hang is not diagnosed or claimed fixed by
  a successful reboot and walkthrough. Its platform recovery investigation stays
  separate from this functioning messaging delivery.

## Block 3: background calls and passive notifications — 2026-10-02

This block is **in progress**, not complete. The remaining real-microphone,
reverse web-client media and separate-internet gates are recorded in
[media and voice](SOCIAL_MEDIA_VOICE.md#background-call-recovery--2026-10-02).
Do not advance to block 1 on the strength of this implementation alone.

**Owner continuation:** hands-on checks wait until the owner returns home.
Continue preparing this block; do not interpret that deferral as passing its
audio/headset or interoperability acceptance.

- Calls belong to the authenticated session, not the displayed page. Tab changes,
  Home menus and ordinary Adventure launch/return retain the same voice worker.
  A chat Gateway reconnect no longer tears down independent LiveKit audio.
  Fresh chat sessions recheck channel access before reattaching the call without
  ringing again. Group removal/logout still end the appropriate call.
- Home exposes microphone, call-output and Leave controls wherever the user is.
  Back and repeated Home retain the call and origin. Recording a voice message
  cannot compete with a live call for the microphone.
- The call strip and Home call panel identify the actual call conversation,
  including while another chat is displayed. They share the existing session
  and do not introduce another call page.
- Incoming calls have direct Answer/Decline actions in Home. Opening Home while
  a call is ringing selects Answer; receiving a ring never opens a menu by itself.
  A on an answerable inbox entry answers in place and returns to the underlying
  page, without first opening its conversation/Options. The footer says Answer
  and Decline for those actions. An existing call is never replaced implicitly;
  stale, disconnected and no-longer-ringing answers do nothing. Missed-call
  entries still open the conversation normally.
- An observed incoming ring that ends unanswered becomes a missed-call inbox
  entry. Opening it displays the conversation; it never calls back automatically.
  Explicit decline, answering on another client, unavailable-call events and a
  Gateway interruption do not fabricate missed calls. Dismissal remains separate
  from read acknowledgement. Up to 32 latest per-conversation missed calls use
  the account-bound, 30-day history cache. Calls entirely missed while offline
  cannot be inferred from current participant lists and are not reconstructed.
- Notifications can be dismissed without marking messages read. A new message
  makes that conversation visible again. Dismissals and missed activity
  invitations are isolated by Trainer and Fluxer account.
- Incoming activity invitations produce a passive notice and an inbox entry;
  they do not replace the page, editor or game. Opening a pending entry reveals
  the existing Accept/Decline consent. Expired entries lead to the conversation,
  never implicitly accept or begin a saved-game operation.
- Friend requests have a direct notification destination; community mentions
  use provider read state. Ordinary unread DMs are not mislabeled as mentions.
  Sound, DND, muted-conversation and private-preview settings remain effective.

Actual paired captures include `calls-invite-passive.png`,
`calls-invite-inbox.png`, `calls-invite-consent.png`,
`calls-home-voice-controls.png` and `calls-odin-voice-controls.png` in private
`work/research/`. The invitation reached Odin on Home and opened consent only
after selection; declining did not change either save. Audio and interoperability
evidence, including its limits, is in the linked document.

The October 2 remote continuation verified both installed shells and preserved
data, but public Fluxer declared a connectivity outage at 12:30 UTC. Both native
clients and the ordinary web client could not reconnect; no incoming-call runtime
success is claimed. [Outage evidence and the exact next checks](SOCIAL_MEDIA_VOICE.md#remote-continuation-blocked-by-public-service-outage--2026-10-02)
retain block 3 as open, including deferred human microphone/headset acceptance.
