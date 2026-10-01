# Native communities and TrainerOS discovery

## Supported representation - 2026-10-01

The #98/#101 community marker is a discovery hint, not an authenticated client,
activity capability or trust assertion. No game, transport, save lineage, library,
balance or collection is advertised by it. #104/#109 still require fresh peer
capabilities, an explicit invitation and the existing activity-specific checks.

Fluxer's supported [guild API](https://docs.fluxer.app/http-api/guilds/) provides
creation templates and a server-owned `system_channel_id`, but no arbitrary
application metadata field. Its [message API](https://docs.fluxer.app/http-api/messages/)
provides durable ordinary messages and pins. TrainerOS therefore creates one
ordinary `general` text channel and pins an owner-authored welcome message there.
The welcome contains a fenced JSON manifest:

```json
{"kind":"org.traineros.community","version":1,"guild_id":"actual-guild-id"}
```

The name, description and channel name do not establish identity. Discovery reads
the guild from Fluxer, locates its current system text channel, then checks the
latest 50 pins. A match must be an ordinary non-webhook message authored by the
current guild owner, in that channel, with this exact namespace/version and guild
ID. Unknown versions, inaccessible/missing pins, a copied foreign manifest or
owner transfer do not produce a positive match. A malicious owner can declare a
community TrainerOS-aware; this is explicitly not proof of installed clients.

The record lives in Fluxer, not solely in the device database. Another member or
a reinstalled client can read it through normal permissions. Ordinary Fluxer sees
the welcome and its readable JSON; TrainerOS renders the welcome compactly. No
hidden fields/opcodes, Unicode markers, forced renames, bots or administrator API
are used. Nothing is sent to all members or friends as a compatibility probe.

## Creation, browsing and recovery

- Communities / Options / New community opens the existing controller keyboard.
  In an empty Communities view X opens it directly. Submitting a name creates the
  guild and opens its chat; no separate greeting, setup dashboard or Open step.
- The supported native CAPTCHA proof handles explicit provider challenges.
  Email verification, instance restrictions and permissions remain Fluxer's.
- Creation/posting are never blindly replayed after uncertain delivery. Partial
  success preserves the community; its owner can Finish TrainerOS setup from
  Options. A recent matching owner post is reused after a failed pin. There is
  no automatic community deletion or rollback of ordinary messages.
- Y toggles All / TrainerOS while browsing the community list with no draft.
  Writing/sending and reading-history shortcuts keep their established priority.
  Options also exposes the filter. A filter never joins or leaves a community.
- Options / Invite link creates a normal one-day Fluxer invite and displays it
  in a small popover. It does not automatically send messages to anyone.
- Checks run serially on the network worker while Communities is visited,
  bounded to the existing first 100 joined guilds and 50 pins per system channel.
  Re-entry/Refresh rechecks; relevant message/channel/guild events invalidate the
  positive hint. Account epochs and per-guild revisions reject stale replies.
  Hidden/deleted channels remain normal provider permission boundaries.
- To intentionally retire the marker, unpin/delete it using ordinary Fluxer.
  System-channel changes or owner transfer require the new owner to enable it
  there. Moving the manifest beyond the inspected pin window makes it unknown;
  no unbounded background history crawl is introduced.

## Public discovery is a different boundary

Fluxer's [discovery API](https://docs.fluxer.app/http-api/discovery/) supports
structured custom tags and a `tag` query. Search / TrainerOS requests the
`traineros-v1` tag from the provider, alongside the ordinary Communities search.
This is a public listing hint only: a non-member cannot fetch a private guild's
pins. Joined-community filtering uses the owner manifest instead of a name or
public description. No automatic join is used to inspect search results.

Public listing requires Fluxer's eligibility and approval. Native creation does
not publish a guild or bypass those requirements. Owners of approved public
communities may add `traineros-v1` through Fluxer's supported discovery settings;
listing administration stays outside this bounded handheld feature. Private
communities are reachable through normal invite links. There is no independent
TrainerOS directory/server or fabricated global search coverage.

## Remaining gates

This slice does not enable game invitations or multiplayer through membership.
Continue #104/#109 capability/invitation integration, #105 delivery proof and the
existing system-activity host. Keep #102 media, #103 calls, #107 runtime multiplayer,
all prior messaging acceptance and the complete roadmap.

## Delivery evidence - 2026-10-01

- Windows native and Flip ARM64 builds passed. Social (36 cases), core,
  interactions and QML smoke passed. Exit QML smoke initially read the fixture
  PID before its child wrote it; the isolated rerun passed without source changes.
  This timing sensitivity remains in that existing smoke fixture.
- On Flip, physical controller events created a private community from the name
  keyboard, opened its chat immediately, read back its owner pin and toggled the
  TrainerOS filter. An ordinary one-day invite was accepted by the second test
  account in the official Fluxer browser; its message arrived in the native chat.
  No unrelated member was contacted and no public listing was submitted.
- After the final shell restart, the installed process reloaded the community,
  positive marker and peer message. Pin/join events use compact service lines.
  Actual compositor captures: private `work/research/community-final-chat.png`,
  `community-final-search.png` and `community-final-tag.png`. The public directory
  query worked; its TrainerOS tag had no listings at verification time.
- Final Flip binary SHA-256:
  `68b5f4820cae0aa670a4c14b8bfd057fb8221cd78173d0268851076a7b7b610c`.
  Its live process matched; all 3 Trainer owners and 830 Adventure records,
  boot preference and nearby helpers were preserved. Deployment retained a
  database/binary backup; no emulator or pending Link settlement was active.
- Odin SSH timed out on both bounded probes. Its device deployment and physical
  two-handheld proof remain deferred; the second-account browser result is not
  Odin hardware evidence. The final ARM binary is retained privately for delivery.
- Failed-pin reuse, stale/revoked/foreign marker rejection, permission denial,
  unknown creation delivery and account-switch isolation are automated evidence,
  not claims of deliberately faulting the public service or real user content.
