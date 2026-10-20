# Install metaserver

A game uses the library from its own Go module; nothing is copied.

1. Create `games/<game>/metaserver/go.mod`:

   ```text
   module <game>/metaserver

   go 1.27

   require studio/metaserver v0.0.0

   replace studio/metaserver => ../../../../features/metaserver
   ```

   The relative path is from `games/private/<game>/metaserver`; run
   `go mod tidy` to fill `go.sum` and the indirect requirements.
2. `metaserver/main.go` calls `metaserver.Main` with the game's routes (the
   table in `README.md#public-surface`), mounting `Enter` on
   `metaserver.PathEnter`.
3. `metaserver/cmd/admin/main.go` calls `admin.Main(admin.Brand{Name, Mark})`.
4. List the feature in the game's `dependencies.json`
   (`{"id": "metaserver", "source": "features/metaserver", "version": ...}`).
   `live_games.mjs` then vets and builds the game's `metaserver/`.
5. The game's room tests read `features/metaserver/testdata/*.json`, the
   contract both sides pin.

The image build needs the Studio checkout as its context (the `replace`
reaches outside the game). Deploy, database name and owner, role passwords and
the ticket key stay the game's.

Local stand: `docker compose -f features/metaserver/compose.yaml up -d --wait`,
then `go run . migrate` and `go run .` in the game's `metaserver/` with a
`ticket.key` (`openssl rand -hex 32`). A stand initialised with another owner
needs `docker compose down -v` first; that drops its data.

Uninstall: delete the game's `metaserver/`, its deploy pieces and the
`dependencies.json` entry.
