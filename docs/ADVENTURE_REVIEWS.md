# Adventure Reviews — #113

Reviews live in Hold A → Properties → Reviews. Reading does not require a
completed save. The compact list/detail panel shares the controller keyboard,
spoiler reveal, author/date, edit/delete and report actions. It adds no primary
page and never intercepts normal A-to-launch.

## Identity and completion

The key is the SHA-256 of the actual Adventure content, using the adapter's
existing exact-build identity when available. Renaming/moving a file does not
change its reviews. Remakes, hacks and different builds do not silently merge.
Archive repacking can change the key; cross-container equivalence is not claimed.
Hashing and ordinary-save inspection run off the GUI thread.

The first write policy is `emerald-en/champion-v1`: the supported English Emerald
adapter must observe the Champion milestone in a readable ordinary save.
Playtime, badges alone and manual collection marks cannot unlock it. The save is
read again before publication. Other games can read reviews; writing waits for
their own explicit completion policy. The portable adapter export includes this
policy and its exact profile.

This proves a local completed save, not that this Fluxer account personally
completed the game. Imported completed saves qualify. It is a casual community
policy, not signed competitive provenance. Returning from an eligible Adventure
can show a gentle once-per-Trainer/build invitation; it never opens a forced form.

## Supported Fluxer representation

The implementation uses ordinary channel messages, message search, author-owned
PATCH/DELETE and the native message-report endpoint. There is no hidden metadata
facility or undocumented service account. A deployment-owned
`/var/opt/traineros/integrations/reviews.json` selects the shared channel:

```json
{"channel":"<review channel snowflake>"}
```

The signed-in user needs access to that channel. Test deployment uses the existing
two-account test community. Public community onboarding, ownership/moderation and
the distribution configuration are still release work; new users are not silently
joined to a community. Lack of configuration, membership or Fluxer access does
not affect launch or local game features.

Messages contain a visible `TrainerOS review v1` header and a small JSON body:
exact Adventure key, text (800 characters), spoiler flag and completion policy.
This is intentionally visible in other Fluxer clients. Fluxer supplies the author
and timestamps; client-supplied author identity is ignored. Bots/webhooks are not
reviews. Mentions are disabled on publication.

Search selects the exact Adventure key and the configured channel. A separate
author query plus a direct GET of a known own message prevents stale indexing
from replacing an existing editable review with a new post. The displayed result
is one latest review per account. Fluxer does not provide a permanent atomic
unique constraint on account/Adventure: simultaneous first posts from two
devices may produce duplicate underlying messages. Strict cross-client uniqueness
remains open, rather than being claimed from client-side deduplication.

Own edits and deletes target the known author's message, never a caller-supplied
foreign ID. Reports target a displayed foreign review through Fluxer's normal
spam/harassment/other categories. Moderation remains with Fluxer and channel staff;
no abusive-content test report was sent to real moderators.

## Failure and cache behavior

The optional cache is scoped by Fluxer account, review channel and exact Adventure.
Unavailable/stale copies are labelled. Fresh ownership checks are required before
mutations; ambiguous delivery disables immediate resend and asks for refresh.
Late requests are invalidated on account/Trainer or Adventure changes. Reading
can be dismissed while loading. A failed review operation cannot block launching
the game or close the ordinary Properties surface.

## Evidence, 2026-10-02

- Actual Flip controller flow published an Emerald review, edited the same
  message, toggled spoilers and deleted it. Odin loaded that review through the
  public Fluxer API and revealed its spoiler text in the installed UI.
- Test data was removed after the read/edit/delete proof. Private captures are
  `review-published.png`, `reviews-read.png`, `reviews-delete.png` under
  `work/research`; they are not shipped assets.
- Automated coverage checks exact identity, readable completion, ownership,
  read-without-completion and failure gates. Device/account provenance and
  global uniqueness are deliberately not claimed.

Primary references: [Fluxer messages](https://docs.fluxer.app/http-api/messages/),
[message search](https://docs.fluxer.app/http-api/search/),
[reports](https://docs.fluxer.app/http-api/reports/).
