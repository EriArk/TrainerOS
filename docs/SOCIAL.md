# Native Social - conversations and discovery

2026-10-01. Supersedes the unlinked #111 placeholder. The owner's refinement
uses four cyclic faces: Messages / Groups / Communities / Search. The five
primaries and Start/Home responsibilities remain unchanged.

## Implementation

FluxerSession owns supported public-instance browser handoff, REST and Gateway
on a separate Qt event-loop thread. UI receives only presentation snapshots.
SocialController owns list focus, the retained conversation and in-memory drafts;
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

## Sessions and recovery

The selected local Trainer scopes each session and every asynchronous result.
Owner changes close the old worker's network session and immediately clear the
private view/drafts. Credentials stay in the worker; QtKeychain has insecure
fallback disabled. Windows uses its native protected store. On Linux an already
running secret service/wallet is used; absence/failure explicitly means a
memory-only session. TrainerOS does not summon a desktop wallet unlock dialog.
Options reports whether the session is remembered. No token is stored in the
ordinary database, screenshots, logs, ROMs, save backups or command arguments.

Logout calls the supported revocation route. An offline/failed revocation stays
visible and retains the session for a retry rather than claiming success.
Successful logout clears private session data and deletes its vault entry.
Heartbeat ACKs, bounded backoff/jitter and Gateway Resume reconnect the client;
new sessions also refresh bounded REST views. Provider retry delays are honored.
Messages carry a provider-supported 32-character nonce; an uncertain HTTP result
is shown as uncertain and never automatically resent. Server echoes reconcile
pending messages. Late responses from a previous owner/session are ignored.
A history reply cannot overwrite messages changed by newer Gateway events.
No activity/title/ROM/save data is automatically published to Fluxer.

## Remaining #100/#101 acceptance

This is a working text slice, not completion of both issues. Preserve:

- durable account-scoped history/drafts, history pagination and detailed read-state
  synchronization; current history is bounded to 50 fetched / 100 displayed items;
- group creation/member management/leave, own-message edit/delete controls,
  richer friend lookup where supported, avatars and compact notification/privacy
  controls; remote edits/deletions are already reflected;
- hardware proof of remembered secure credentials where a system vault is not
  already available, and longer reconnect/rate-limit/large-account validation;
- live in-game Home messenger presentation without disturbing emulator input;
- #104/#105 supported compatible-TrainerOS recognition and accepted game offers,
  #109/#110 native online activities, #102 media, #103 calls and #107 runtime play.

Existing group/community access follows Fluxer permissions; this is not a guild
administration clone. No new account-name markers or invisible profile suffixes.

## References

- [Authentication](https://docs.fluxer.app/http-api/authentication/)
- [Relationships](https://docs.fluxer.app/http-api/users/relationships/)
- [Private channels](https://docs.fluxer.app/http-api/users/private-channels/)
- [Messages](https://docs.fluxer.app/http-api/messages/)
- [Guilds](https://docs.fluxer.app/http-api/guilds/) - Get guild returns visible channels.
- [Gateway](https://docs.fluxer.app/gateway/events/)
- [Community discovery](https://docs.fluxer.app/http-api/discovery/)
- [Invitations](https://docs.fluxer.app/http-api/invites/)
- [Earlier native feasibility proof](FLUXER_SPIKE.md)

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
- Flip currently has no available system credential vault: sign-in is session-only.
  Persistent handheld sign-in is the next account UX gate, not delivered evidence.
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
- Odin SSH still times out; this correction is not installed there. Persistent
  handheld credentials and the remaining acceptance above stay open.
