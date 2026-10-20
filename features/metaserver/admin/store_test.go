package admin

import (
	"bytes"
	"context"
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"net/url"
	"os"
	"testing"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"
)

func freshAdminStore(t *testing.T) *adminStore {
	t.Helper()
	adminURL := "postgres://metaserver:metaserver@127.0.0.1:5432/metaserver"
	if value := os.Getenv("META_TEST_DATABASE_URL"); value != "" {
		adminURL = value
	}
	ctx := t.Context()
	admin, err := pgx.Connect(ctx, adminURL)
	if err != nil {
		t.Fatal(err)
	}
	var suffix [4]byte
	if _, err := rand.Read(suffix[:]); err != nil {
		t.Fatal(err)
	}
	name := "meta_admin_test_" + hex.EncodeToString(suffix[:])
	if _, err := admin.Exec(ctx, fmt.Sprintf(`CREATE DATABASE %s`, name)); err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() {
		admin.Exec(context.Background(), fmt.Sprintf(`DROP DATABASE %s WITH (FORCE)`, name))
		admin.Close(context.Background())
	})
	parsed, _ := url.Parse(adminURL)
	parsed.Path = "/" + name
	pool, err := pgxpool.New(ctx, parsed.String())
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(pool.Close)
	// The migration runner owns this table; the schema grants on it.
	if _, err := pool.Exec(ctx, `CREATE TABLE schema_migrations (version integer PRIMARY KEY,
		checksum text NOT NULL, applied_at timestamptz NOT NULL DEFAULT now())`); err != nil {
		t.Fatal(err)
	}
	for _, file := range []string{"../schema.sql", "../migration_2.sql"} {
		script, err := os.ReadFile(file)
		if err != nil {
			t.Fatal(err)
		}
		if _, err := pool.Exec(ctx, string(script)); err != nil {
			t.Fatalf("apply %s: %v", file, err)
		}
	}
	dummy, err := hashPassword("dummy password")
	if err != nil {
		t.Fatal(err)
	}
	return &adminStore{pool: pool, dummyHash: dummy}
}

func TestLoginCreatesRevocableSession(t *testing.T) {
	store := freshAdminStore(t)
	ctx := t.Context()
	password := "owner password for testing"
	hash, _ := hashPassword(password)
	if _, err := store.pool.Exec(ctx, `INSERT INTO ops.admin_users (username, password_hash, role)
		VALUES ('owner', $1, 'owner')`, hash); err != nil {
		t.Fatal(err)
	}
	now := time.Unix(1_780_000_000, 0).UTC()
	if _, _, err := store.login(ctx, "owner", "wrong", now); err != errInvalidCredentials {
		t.Fatalf("wrong password: %v", err)
	}
	cookie, user, err := store.login(ctx, "owner", password, now)
	if err != nil || user.Username != "owner" || user.Role != "owner" {
		t.Fatalf("login: %+v %v", user, err)
	}
	active, err := store.session(ctx, cookie, now.Add(time.Minute))
	if err != nil || active.ID != user.ID {
		t.Fatalf("session: %+v %v", active, err)
	}
	if err := store.logout(ctx, cookie, now.Add(2*time.Minute)); err != nil {
		t.Fatal(err)
	}
	if _, err := store.session(ctx, cookie, now.Add(3*time.Minute)); err != errNoSession {
		t.Fatalf("revoked session: %v", err)
	}
	var allowed, denied int
	if err := store.pool.QueryRow(ctx, `SELECT count(*) FILTER (WHERE result='allowed'),
		count(*) FILTER (WHERE result='denied') FROM ops.audit_actions WHERE action='login'`).Scan(&allowed, &denied); err != nil || allowed != 1 || denied != 1 {
		t.Fatalf("audit: allowed=%d denied=%d err=%v", allowed, denied, err)
	}
}

func TestPlayerLookupUsesSafeProjectionAndCursor(t *testing.T) {
	store := freshAdminStore(t)
	ctx := t.Context()
	if _, err := store.pool.Exec(ctx, `INSERT INTO players (login) VALUES ('ann'), ('anna'), ('bob')`); err != nil {
		t.Fatal(err)
	}
	first, err := store.players(ctx, "an", 0, 1)
	if err != nil || len(first) != 1 || first[0].Login == nil || *first[0].Login != "ann" {
		t.Fatalf("first page: %+v %v", first, err)
	}
	second, err := store.players(ctx, "an", first[0].ID, 1)
	if err != nil || len(second) != 1 || second[0].Login == nil || *second[0].Login != "anna" {
		t.Fatalf("next page: %+v %v", second, err)
	}
	player, err := store.player(ctx, first[0].ID)
	if err != nil || player.ID != first[0].ID {
		t.Fatalf("player: %+v %v", player, err)
	}
	encoded, _ := json.Marshal(player)
	if bytes.Contains(encoded, []byte("password")) || bytes.Contains(encoded, []byte("doc")) {
		t.Fatalf("sensitive data in player projection: %s", encoded)
	}
}

func TestOccupancyHistoryUsesOnlyLiveRoomsAndReportsSampleCount(t *testing.T) {
	store := freshAdminStore(t)
	ctx := t.Context()
	now := time.Date(2026, 9, 29, 12, 4, 0, 0, time.UTC)
	minute := now.Add(-time.Minute)
	if _, err := store.pool.Exec(ctx, `INSERT INTO activity.occupancy_runs
		(minute_utc, sampled_at, rooms_seen, live_rooms) VALUES ($1, $1, 2, 1)`, minute); err != nil {
		t.Fatal(err)
	}
	if _, err := store.pool.Exec(ctx, `INSERT INTO activity.occupancy_minutes
		(minute_utc, room_id, incarnation, seated, away, capacity, alive, room_seen_at)
		VALUES ($1, 1, 1, 3, 1, 8, true, $1), ($1, 2, 1, 9, 2, 12, false, $1)`, minute); err != nil {
		t.Fatal(err)
	}
	hours, err := store.occupancyHistory(ctx, now)
	if err != nil || len(hours) != 1 || hours[0].Samples != 1 ||
		hours[0].PeakSeated != 3 || hours[0].PeakAway != 1 ||
		hours[0].PartialSamples != 1 || hours[0].SampleStatus != "partial" {
		t.Fatalf("occupancy history: %+v %v", hours, err)
	}
}

func TestKnownRoomsKeepsSilentRegistryRows(t *testing.T) {
	store := freshAdminStore(t)
	now := time.Date(2026, 9, 29, 12, 0, 0, 0, time.UTC)
	if _, err := store.pool.Exec(t.Context(), `INSERT INTO rooms
		(id, incarnation, host, port, free, accounts, seen_at, seated, away, capacity, period_ms)
		VALUES (42, 1, 'room.example', 443, 12, '{}', $1, 3, 1, 16, 1000)`, now.Add(-2*time.Hour)); err != nil {
		t.Fatal(err)
	}
	rooms, err := store.knownRooms(t.Context(), now)
	if err != nil || len(rooms) != 1 || rooms[0].ID != 42 || rooms[0].Alive {
		t.Fatalf("known rooms: %+v %v", rooms, err)
	}
	// Two hours silent and in no plan: a room switched off, counted, not listed.
	snapshot := metaSnapshot{Now: now}
	server := &adminServer{store: store}
	if err := server.includeKnownRooms(t.Context(), &snapshot); err != nil ||
		len(snapshot.Rooms) != 0 || snapshot.RoomsRetired != 1 {
		t.Fatalf("merged rooms: %+v %v", snapshot.Rooms, err)
	}
	// The machine plans rooms 41..43: room 42 is an alarm again, and the two
	// the registry never heard are listed as never reported.
	if _, err := store.pool.Exec(t.Context(), `INSERT INTO room_machines (host, rooms, room_id_base, reported_at)
		VALUES ('room.example', 3, 40, $1)`, now); err != nil {
		t.Fatal(err)
	}
	snapshot = metaSnapshot{Now: now}
	if err := server.includeKnownRooms(t.Context(), &snapshot); err != nil || len(snapshot.Rooms) != 3 ||
		!snapshot.Rooms[1].Planned || snapshot.Rooms[1].NeverReported || !snapshot.Rooms[0].NeverReported ||
		len(snapshot.RoomPlans) != 1 || snapshot.RoomPlans[0].Missing != 3 || snapshot.RoomsRetired != 0 {
		t.Fatalf("planned rooms: %+v %+v %v", snapshot.Rooms, snapshot.RoomPlans, err)
	}
}

func TestPlansSeparateAlarmsFromSwitchedOffRooms(t *testing.T) {
	plans := []roomPlanView{{Host: "b", Rooms: 2, RoomIDBase: 1000}}
	rooms := []metaRoom{
		{ID: 1001, Host: "b", Alive: true},
		{ID: 1002, Host: "b", AgeS: 90_000},
		{ID: 1050, Host: "b", AgeS: 90_000},
		{ID: 1051, Host: "b", Alive: true},
		{ID: 7, Host: "old", AgeS: 600},
		{ID: 8, Host: "old", AgeS: 7200},
	}
	kept, counted, retired := applyPlans(rooms, plans)
	ids := []int64{}
	for _, r := range kept {
		ids = append(ids, r.ID)
	}
	// 1002 is planned and silent: kept as the alarm it is; 1050 is outside the plan
	// and silent: retired; 1051 answers outside it: kept; room 8 of no plan, past the hour: retired.
	if retired != 2 || len(ids) != 4 || ids[0] != 7 || ids[1] != 1001 || ids[2] != 1002 || ids[3] != 1051 ||
		counted[0].Live != 1 || counted[0].Missing != 1 || kept[3].Planned || !kept[2].Planned {
		t.Fatalf("kept %v retired %d plans %+v", ids, retired, counted)
	}
}

func TestCapacitySeriesSumsMachinesAndKeepsGapsMissing(t *testing.T) {
	store := freshAdminStore(t)
	ctx := t.Context()
	now := time.Date(2026, 10, 1, 12, 0, 30, 0, time.UTC)
	full := time.Date(2026, 10, 1, 11, 0, 0, 0, time.UTC)
	exec := func(query string, args ...any) {
		t.Helper()
		if _, err := store.pool.Exec(ctx, query, args...); err != nil {
			t.Fatal(err)
		}
	}
	// A run with no machine at all: an idle fleet, not a missing minute.
	idle := full.Add(-20 * time.Minute)
	exec(`INSERT INTO activity.capacity_runs (minute_utc, sampled_at) VALUES ($1, $1)`, idle)
	for i := 0; i < 10; i++ {
		minute := full.Add(time.Duration(i) * time.Minute)
		exec(`INSERT INTO activity.capacity_runs (minute_utc, sampled_at) VALUES ($1, $1)`, minute)
		exec(`INSERT INTO activity.machine_minutes (minute_utc, host, rooms, live_rooms, seated, away, capacity, cpu_pct, rx_mbit_s, tx_mbit_s)
			VALUES ($1, 'a.test', 2, 2, 10, 0, 40, 30, 1, 4)`, minute)
		if i < 6 { // b.test goes silent for the bucket's last four minutes
			exec(`INSERT INTO activity.machine_minutes (minute_utc, host, rooms, live_rooms, seated, away, capacity, cpu_pct, rx_mbit_s, tx_mbit_s)
				VALUES ($1, 'b.test', 1, 1, 5, 0, 20, 85, 1, 2)`, minute)
		}
		exec(`INSERT INTO activity.api_minutes (minute_utc, endpoint, requests, client_errors, server_errors, rate_limited, avg_ms, max_ms)
			VALUES ($1, 'enter', 3, 0, 1, 0, 2, 9)`, minute)
		// Ten players at 4 ms a player-second for six minutes, then two at 10: weighted by players, 320/68.
		players, cpuMs := 10.0, 4.0
		if i >= 6 {
			players, cpuMs = 2, 10
		}
		exec(`INSERT INTO activity.fleet_minutes (minute_utc, windows, avg_seated, cpu_ms_per_player_s, step_p99_us,
			replicate_p99_us, late_p99_us, late_ticks, dropped_ticks, rx_msg_per_s, tx_writes_per_s, tx_mbit_s)
			VALUES ($1, 30, $2, $3, $4, 1024, 2048, 1, 0, 10, 20, 0.5)`, minute, players, cpuMs, 200+i)
	}
	span, err := parseCapacityRange("7d", "600")
	if err != nil {
		t.Fatal(err)
	}
	win := windowFor(span, now, nil)
	points, err := store.capacitySeries(ctx, span, win, "")
	if err != nil {
		t.Fatal(err)
	}
	if len(points) != int(span.length/span.bucket) {
		t.Fatalf("points %d", len(points))
	}
	byTime := map[time.Time]capacityPoint{}
	for _, p := range points {
		byTime[p.At] = p
	}
	// Six minutes of 15 seats and 57.5% mean CPU, four of 10 seats and 30%.
	p := byTime[full]
	if p.Status != "ok" || p.Minutes != 10 || p.SeatedMax == nil || *p.SeatedMax != 15 || *p.Seats != 60 ||
		*p.CPUAvg != 46.5 || *p.CPUMax != 85 || *p.TxMbitS != 5.2 || *p.TxPeak != 6 || *p.RxPeak != 2 ||
		p.CPUMsPerPlayerS == nil || *p.CPUMsPerPlayerS < 4.70 || *p.CPUMsPerPlayerS > 4.71 || *p.StepP99Us != 209 || *p.LateTicks != 10 || *p.Requests != 30 || *p.ServerErrors != 10 {
		t.Fatalf("full bucket %+v", p)
	}
	if gap := byTime[full.Add(-10*time.Minute)]; gap.Status != "missing" || gap.SeatedMax != nil || gap.Requests != nil {
		t.Fatalf("gap %+v", gap)
	}
	if quiet := byTime[idle]; quiet.Status != "partial" || quiet.Minutes != 1 || *quiet.SeatedMax != 0 ||
		*quiet.TxMbitS != 0 || quiet.CPUAvg != nil || *quiet.Requests != 0 {
		t.Fatalf("idle fleet %+v", quiet)
	}
	host, err := store.capacitySeries(ctx, span, win, "b.test")
	if err != nil {
		t.Fatal(err)
	}
	for _, p := range host {
		if p.At.Equal(full) && (p.Status != "partial" || p.Minutes != 6 || *p.SeatedMax != 5 || *p.CPUAvg != 85 || p.Requests != nil) {
			t.Fatalf("one machine, silent for part of the bucket %+v", p)
		}
		if p.At.Equal(idle) && p.Status != "missing" {
			t.Fatalf("one machine in a minute without it %+v", p)
		}
	}
	machines, err := store.capacityMachines(ctx, win, now)
	if err != nil || len(machines) != 2 || machines[0].State != "unknown" || machines[0].CPU != nil ||
		*machines[1].CPUP95 != 85 || machines[0].SeatedMax != 10 {
		t.Fatalf("stale machines %+v %v", machines, err)
	}
	at := full.Add(6*time.Minute + 30*time.Second)
	fresh, err := store.capacityMachines(ctx, windowFor(span, at, nil), at)
	if err != nil || fresh[0].Host != "b.test" || fresh[0].State != "warn" || fresh[1].State != "ok" {
		t.Fatalf("a warning machine leads %+v %v", fresh, err)
	}
	endpoints, err := store.capacityEndpoints(ctx, win)
	if err != nil || len(endpoints) != 1 || endpoints[0].Requests != 30 || endpoints[0].AvgMs != 2 || endpoints[0].MaxMs != 9 {
		t.Fatalf("endpoints %+v %v", endpoints, err)
	}
	first, newest, err := store.capacityBounds(ctx)
	if err != nil || !first.Equal(idle) || !newest.Equal(full.Add(9*time.Minute)) {
		t.Fatalf("bounds %v %v %v", first, newest, err)
	}
	// A sampler a moment behind moves the expected end back; a long silence does not.
	if w := windowFor(span, full.Add(11*time.Minute+time.Second), newest); !w.last.Equal(*newest) {
		t.Fatalf("lagging sampler %+v", w)
	}
	if w := windowFor(span, now, newest); !w.last.Equal(now.Truncate(time.Minute).Add(-time.Minute)) {
		t.Fatalf("silent sampler %+v", w)
	}
}

func TestCapacityStepsStayOnTheDayGridAndBounded(t *testing.T) {
	for _, c := range []struct {
		rng, step string
		bucket    time.Duration
		ok        bool
	}{
		{"24h", "", 5 * time.Minute, true}, {"7d", "", 30 * time.Minute, true}, {"30d", "", 3 * time.Hour, true},
		{"24h", "60", time.Minute, true}, {"30d", "21600", 6 * time.Hour, true},
		{"24h", "420", 0, false}, {"30d", "60", 0, false}, {"7d", "x", 0, false}, {"1y", "", 0, false},
	} {
		r, err := parseCapacityRange(c.rng, c.step)
		if (err == nil) != c.ok || (c.ok && r.bucket != c.bucket) {
			t.Fatalf("%s step %q: %+v %v", c.rng, c.step, r, err)
		}
		if c.ok && (r.length/r.bucket > capacityPointsMax || (24*time.Hour)%r.bucket != 0) {
			t.Fatalf("%s step %q leaves the day grid or the point bound", c.rng, c.step)
		}
	}
}
