# Native Social - conversations and discovery

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
faces; L1/R1 keeps its primary meaning. X writes a draft in a conversation, Y
sends it, right reads/scrolls messages, left returns to the people list. Back
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

## Remaining #100/#101 acceptance

This is a working text slice, not completion of both issues. Preserve:

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
