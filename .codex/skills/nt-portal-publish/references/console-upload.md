# Uploading into a console

Use only for an authorized draft upload. These are observed console and browser
bridge behaviors, not universal API contracts. Check the current tool's upload
capabilities and applicable console fields; use only the workaround needed for
the observed failure. Video slots and submission remain lead-owned.

## The 10 MB bridge cap, and the file it silently ruins

The legacy `file_upload` bridge capped each call at 10 MB total and could
silently truncate files. If the current host has this cap, use a supported
whole-file transport or the reconstruction below. Do not split files when the
current upload tool accepts them intact; verify uploaded size/integrity.

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

## When the widget stays silent

A file input that swallows a file without a request, an error or a message is
usually running a client-side validator that never settles. Yandex's video
widget measures the clip's height through a `<video>` element on a blob URL; the
agent's Chrome decodes no mp4 at all — even a clip Yandex itself has already
transcoded fails with `MEDIA_ERR_SRC_NOT_SUPPORTED` — so the promise never
resolves and the handler returns having done nothing. When diagnostics establish
this renderer failure, do not re-encode a valid clip to chase it. Hand the
validated clip and slot information to the lead.

## Driving the console API

Inspect the console's current requests during an authorized edit, or its payload
builders, before using an undocumented call. Do not change draft fields merely
to discover the API. Use the browser/connector's supported authenticated flow;
these examples do not authorize extra writes or credential extraction.

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

## After the upload

Video is transcoded server-side and appears in the slot only when it is done.
A clip of about 7 MB carrying an audio track was ready in roughly four minutes;
the same footage at 13–20 MB and with no audio track at all had not appeared
hours later and is what an automatic rejection called invalid. Watch current
processing status rather than guessing from the form. Change encoding only
when current validation or diagnostics support it.

An automatic rejection is not a moderator: `GET /console/api/moderation-history/
<app-id>` answering `was_moderated: false` means no human ever saw the draft, so
no resubmission cooldown has been spent. The rejection line itself is a frozen
field — it keeps naming the old file id until the next submission, and no amount
of saving clears it.
