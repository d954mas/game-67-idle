package admin

import (
	"errors"
	"testing"
	"time"
)

// A document in the shape the game's profile generator writes: header,
// document fields, then one section per system.
const profileFixture = `NTGS 1
save_version=1
save_id="00c0ffee5a5e0001"
rev=7

[economy 2]
coins=1099512627779
purchases=123757

[progress 1]
level=12
`

func TestProfileParsesSystemsAsSaved(t *testing.T) {
	format, systems, err := parseProfile([]byte(profileFixture))
	if err != nil || format != "NTGS 1" || len(systems) != 2 || systems[0].Name != "economy" || systems[0].Version != 2 ||
		len(systems[0].Fields) != 2 || systems[0].Fields[0] != (profileField{Key: "coins", Value: "1099512627779"}) {
		t.Fatalf("parse: %q %+v %v", format, systems, err)
	}
}

func TestProfileRefusesOtherFormats(t *testing.T) {
	for _, doc := range []string{"", "{\"coins\":1}", "NTGS 1\n[economy]\n", "NTGS 1\n[economy 2]\nno equals sign\n"} {
		if _, _, err := parseProfile([]byte(doc)); !errors.Is(err, errProfileFormat) {
			t.Fatalf("%q: %v", doc, err)
		}
	}
}

func TestPlayerProfileReadsTheSavedDocument(t *testing.T) {
	store := freshAdminStore(t)
	ctx := t.Context()
	var id int64
	if err := store.pool.QueryRow(ctx, `INSERT INTO players (login) VALUES ('ann') RETURNING id`).Scan(&id); err != nil {
		t.Fatal(err)
	}
	if profile, err := store.playerProfile(ctx, id); err != nil || profile != nil {
		t.Fatalf("no save yet: %+v %v", profile, err)
	}
	saved := time.Date(2026, 10, 1, 12, 0, 0, 0, time.UTC)
	if _, err := store.pool.Exec(ctx, `INSERT INTO profile_docs (account, owner_room, owner_incarnation, rev, doc, updated_at)
		VALUES ($1, 1, 1, 7, $2, $3)`, id, []byte(profileFixture), saved); err != nil {
		t.Fatal(err)
	}
	profile, err := store.playerProfile(ctx, id)
	if err != nil || profile == nil || profile.Rev != 7 || !profile.SavedAt.Equal(saved) || len(profile.Systems) != 2 {
		t.Fatalf("saved profile: %+v %v", profile, err)
	}
}
