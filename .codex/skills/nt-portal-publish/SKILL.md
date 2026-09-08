---
name: nt-portal-publish
description: "Use when preparing a game in this repository for publication on a web game portal: Yandex Games, Poki, Playgama, CrazyGames or itch. Covers building the portal target, proving the SDK, walking that portal's requirement list, and producing the store asset set for the console draft. Triggers include requests to publish, submit, upload, or prepare assets for any of those portals, and any moderation-rejection follow-up."
---

# NT Portal Publish

A rejection costs more than the review itself. Yandex answers in three to five
business days, but every refused resubmission doubles the wait before the next
one, from a day up to sixteen, and the counter resets only once every four
weeks. CrazyGames grades a build over a launch window of one to three weeks.
Everything here exists so that no requirement is answered from memory.

This skill owns the ORDER of the work, the rules that do not change between
portals, and how a console is driven from the agent's browser. It owns no
portal REQUIREMENTS. Those live where the code that enforces them already reads
them, and a second copy in a skill body drifts: this skill once quoted Yandex
requirement numbers the portal had renumbered, and a row citing a number nobody
can find reads as a row nobody checked. Upload mechanics are the exception on
purpose — they are how the work is done, not what the portal demands, and every
hour lost to them was lost because they were written down in a game's private
notes instead of here.

## Where a portal's facts live

Four files per portal, four owners. Read them; do not restate them here.

| What | Where |
| --- | --- |
| Requirements, cited to the portal's own docs | `features/platform-sdk/references/portals/<portal>.md` |
| Artifact contract: files, layout, forbidden and required markers | `features/platform-sdk/publish-targets/<portal>.json` |
| Store asset sizes, counts and text limits | `ai_studio/store_kit/<portal>_spec.json` |
| This game's verdict on each requirement | `games/<id>/design/knowledge/<portal>_release_checklist.md` |

A packet is studio knowledge and never names a private game. A checklist is the
opposite: one game's verdicts, one row per requirement, each row carrying the
file and line that proves it or the reason no code settles it.

## Order of work

Build and SDK first, store assets second. A beautiful draft attached to a build
that never signals it is ready is rejected on a technical requirement, and the
clock starts again.

### 1. Build the target

```
node tools/game.mjs test
node tools/game.mjs package --target <portal>
```

`package` runs the asset audit, the artifact contract and a browser smoke over
the packaged archive. It refuses a build whose feature sources are dirty, so
commit first.

### 2. Check everything mechanical, in one command

```
node tools/game.mjs portal-check --target <portal>
```

Three rows, one verdict: the artifact against the publish manifest, the store
folder against the portal's spec, and the portal's own SDK proof where one
exists. A row is absent rather than silently passing when the portal offers no
local proof.

Only Yandex ships a dev server a game can be driven inside, and the probe
starts it, drives the artifact in headless Chrome and reports what the SDK
itself saw. It prints the calling stack frame beside every call, because the
portal's own `sdk.js` registers some of the listeners and a verdict that cannot
tell them apart would pass a game that handles nothing. It refuses to run when
the port is already held: a proxy left behind by an earlier run serves an older
artifact, and the verdict would be about a build nobody asked about.

### 3. Walk the requirement list and write the verdicts down

Read the portal's packet, and record one row per requirement in the game's
checklist. A row without evidence is a row nobody checked. `UNPROVEN` means the
code was read and nothing in it settles the question; it is not a synonym for
failure, and saying which is which is the point of the file.

### 4. Produce the store asset set

Everything goes to `games/<id>/release/store/<portal>/` and is validated by the
`store` row of `portal-check`. `ai_studio/store_kit/README.md` says who captures
what and what a capture bot must prove.

**Screenshots and video come from the game**, never from a mockup, through the
game's own capture bot in `games/<id>/devapi/`. **Icon and cover may not be
screenshots**; they come from the game's own art through the asset pipeline and
carry licence, provenance and hashes like every other asset here.

**Every material is filmed once per language the draft declares.** A draft keeps
its own set per language and mirrors one into the others until the box beside
the field is cleared, so the default outcome is a card in one language showing
another language's interface. The language is chosen when the string table
loads, not when a setting is flipped, so a bot switches it through the picker a
player uses and proves the language off the running game before filing a frame
under it.

### 5. Fill the draft and read the portal's own pre-check

Most consoles run an automated check on every archive and print the verdict
above the form. It is free, it is where moderation starts, and it answers in
minutes: upload, read it, fix, upload again.

How to get a file into the form is its own section: **Uploading into a console**,
below.

### 6. Hand over

The reply to the lead names: the archive path, the `portal-check` result, the
checklist rows that are NOT closed and what each would take, and the store
folder. When videos are part of the delivery, name the files and the slot each
belongs to. Uploading the draft, the age rating, the categories and the platform
flags are the lead's; the console is theirs.

## Uploading into a console

A console form is not a file system. Everything below is observed behaviour of
the live consoles and of the agent's browser bridge, re-verified on 2026-09-08
against the Yandex console; when a console changes, correct this section in the
same session that discovers it.

### The 10 MB bridge cap, and the file it silently ruins

One `file_upload` call carries at most 10 MB in total, across every file in that
call. Nothing raises it. The danger is not the refusal — there is none: a form
accepts a truncated file, the console stores it, and the failure surfaces days
later as an automatic rejection naming a file id (`VIDEO.INVALID`) that the field
no longer even contains.

So a file over the cap goes in whole or not at all:

1. `split -b 6000000 -d <file> part_` locally, and take the source's SHA-256.
2. Send each part in its own `file_upload` call into an `<input type=file>`
   injected on the page (several parts per call while their sum stays under the
   cap).
3. Read each part with `arrayBuffer()`, concatenate **in part order** into one
   `Blob`, and build a `File` from it.
4. Compare the reassembled SHA-256 against the source before handing it to the
   site's upload call. A mismatch means a part is missing or out of order.

Proven on a 14 263 090-byte archive: byte-identical hash after reassembly.

Two things this does not lift: a **directory input** (CrazyGames uploads a build
as a folder) and a widget that needs the browser to decode the file. Both stay
the lead's.

A local HTTP server serving the file to the page is not an alternative on a
console with a strict CSP — `fetch("http://127.0.0.1:…")` from the Yandex console
page fails outright.

### When the widget stays silent

A file input that swallows a file without a request, an error or a message is
usually running a client-side validator that never settles. Yandex's video
widget measures the clip's height through a `<video>` element on a blob URL; the
agent's Chrome decodes no mp4 at all — even a clip Yandex itself has already
transcoded fails with `MEDIA_ERR_SRC_NOT_SUPPORTED` — so the promise never
resolves and the handler returns having done nothing. Do not re-encode the clip
to chase this: the file is fine, the renderer is not. Use the console's own API.

### Driving the console API

Read the console's own calls before inventing any: hook `window.fetch`, make one
harmless change in the form, press its Save, and read the URL, the headers and
the body shape it sent. The bundle is the second source — it is minified but the
payload builders are readable, and they settle field names the API only answers
with `Invalid value`.

Every write needs the console's CSRF header. On Yandex it is `x-csrf-token`; a
token an SPA has held for hours goes stale and the answer is a bare 403, while
`<meta name="csrf-token">` holds a fresh one.

**Yandex** — uploads are multipart to `POST /console/api/files/<kind>`, always
with `file` and `app-id`:

| Kind | Extra fields | How the draft references it |
| --- | --- | --- |
| `sources` (the archive) | `?size-limit=104857600&size-limit-type=custom` | `{"sources": <id>}` — a bare id; an object answers `Invalid type` |
| `screenshots` | — | `{"screenshots":{"desktop":[{"ru":<id>,"en":<id>}],"mobile":[…]}}` — bare ids |
| `videos` | `orientation` (`horizontal`/`vertical`) and one `tag` field per tag: `promo` for the gameplay slots, `ad` for the promotional ones | `{"videos":{"common":[{"ru":{"file_id":<id>,"options":{"orientation":"horizontal"}},"en":{…}}],"desktop":[],"mobile":[]}}` |

The draft itself is a merge: `PATCH /console/api/application-draft/<app-id>`
writes only the fields the body carries. Video entries are the fussy ones — the
field is `file_id` even though the draft reads the same slot back as `file`, an
entry without `options` answers `Invalid type`, and an entry that also carries
`tags` answers `Invalid value`. One entry per orientation holds one file per
language, so a horizontal file for one language paired with a vertical one for
the other is not a slot the console can draw.

Read the result back from `GET /console/api/application/<app-id>`: `data.draft`
carries `fill_info` (percent and `missed_fields` per language) and `status`. That
is the only honest completion check — the form's own percentages lag a write.

**CrazyGames** — cover images refuse a file set straight into their input; the
portal logs `UploadType is not properly set`, because the type is set by its own
Upload button. Stub `HTMLInputElement.prototype.click` to a no-op, click the
portal's Upload button (no native dialog opens), set the file, confirm the crop
dialog, then restore the prototype. Its video inputs take a file directly.

### After the upload

Video is transcoded server-side and appears in the slot only when it is done.
A clip of about 7 MB carrying an audio track was ready in roughly four minutes;
the same footage at 13–20 MB and with no audio track at all had not appeared
hours later and is what an automatic rejection called invalid. Keep clips small
and give them a track, even a silent one, and watch `embed_url` fill in rather
than guessing from the form.

An automatic rejection is not a moderator: `GET /console/api/moderation-history/
<app-id>` answering `was_moderated: false` means no human ever saw the draft, so
no resubmission cooldown has been spent. The rejection line itself is a frozen
field — it keeps naming the old file id until the next submission, and no amount
of saving clears it.

## Rules that do not vary

- **The form wins.** When a console form disagrees with a spec file, the form is
  right and the spec is corrected in the same commit that finds it.
- **Never invent a number.** Some portals publish their sizes and some publish
  nothing. Where a portal publishes nothing, the spec says so under
  `undocumented` and the number is read off the form, not guessed. Where it
  does publish, the spec cites the page.
- **Never press submit.** Filling the draft is work; sending it is the lead's
  word, once per submission.
- **Requirement numbers move.** Read them off the live list on the day and fix
  any number in a packet that has drifted.
- **A rewarded offer looks like a video before the tap.** The button is blue
  (Poki's ad rule, kept on every portal so one build serves all) and carries
  an ad mark (video/play icon or an AD label); every portal's moderation reads
  an unmarked rewarded button as a broken ad. After the video the button is gone and the reward is
  shown being paid, not silently added. The rule is in
  `features/platform-sdk/references/contract.md`.

## Per-portal notes

Only what changes the procedure. Everything else is in the packets.

- **Yandex Games** — the one portal with a local proof, so the SDK row is real
  here and nowhere else. Its sizes are documented rather than form-only, and at
  most two categories are allowed. Ads need a registered advertising account;
  whether that blocks submission or only earnings is a console question the
  packet marks open. Requirement 1.3 is judged by minimizing the window and
  switching tabs, with a two-second grace, so a blur handler alone does not
  answer it. Its video widget is silent for an agent and its draft is filled
  through the console API; both are in **Uploading into a console** above.
- **CrazyGames** — the build is graded before it is published: a Basic Launch of
  7 to 21 days with monetization off, so rewarded offers report unsupported for
  that whole window by design. Its checks live in the portal's own Preview tool,
  which needs the account. Its three covers must share one visual style.
- **Playgama** — a single-language English draft form, with the game's own
  languages declared as a separate tag list. Native placement, leaderboard and
  product ids come from the console and cannot be guessed; the bridge config
  ships with placements named but unfilled, and the Bridge falls back to the
  platform's own unit.
- **Poki** — a staged funnel with hard playtime and retention gates rather than
  a one-shot moderation queue. Read the packet before promising a date.
- **itch** — no portal SDK and no store spec; the artifact contract is the whole
  check.

## What this skill does not do

- It does not press "submit". That is the lead's word.
- It does not invent sizes. Numbers come from the form or from a spec file.
- It does not make screenshots out of promotional art, and does not make an icon
  out of a screenshot. Both are refusals.
- It does not put video into a draft. It films the clips, checks them and hands
  them over; the slots are filled by hand.
- It does not hold portal facts. Those belong to the four files above.
