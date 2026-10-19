# Pi Skill Chain

Fixed order for feature work in pi. Each step consumes the previous step's
output; skipping a step means the next one invents what it was not told.

1. `/grill-me` — interview until shared understanding. Produces no file; it
   settles the design tree. Never jump to step 3 from an unfinished grilling.
2. `research` — model-invoked when a step needs facts. Writes findings as
   Markdown, every claim cited to a primary source.
3. `/to-spec` — synthesize the conversation into a spec and create it as an
   **epic** under the owning project. No interview here; the only question is
   confirming the test seams.
4. `/to-tickets` — split that epic into tracer-bullet tickets in the
   **Taskboard**, blockers first, each declaring its `Blocked by` edges.
   Never `.scratch/` or any other parallel store — the Taskboard is the single
   source of truth for current work.
5. `/implement` — take a ticket from the frontier (blockers all closed), build
   it through `tdd`, then `code-review`, then commit.

Run pi from the studio root, not from a game folder: `.pi/settings.json` is
read from the current directory only and does not search parent folders, so a
session started inside `games/<id>` silently loses these skills and the project
permission rules.

Taskboard commands take `--game <game-id>` for game work; private game stores
are invisible without it.

`to-spec` and `to-tickets` here are local overrides in `.pi/skills/`, retargeted
from the upstream `.scratch/` and GitHub-issue layouts to the Taskboard CLI.
Both upstream copies are excluded from the package filter so they never collide.

Skills are declared in `.pi/settings.json`; pi appends this file to its system
prompt when started from the studio root. The clone under `.pi/git/` is
gitignored; `.pi/settings.json` is committed.
