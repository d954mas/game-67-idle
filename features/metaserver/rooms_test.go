package metaserver

import (
	"bytes"
	"context"
	"encoding/hex"
	"errors"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"testing"
	"time"
)

func roomCall(t *testing.T, h http.Handler, path, bearer string, body []byte) *httptest.ResponseRecorder {
	t.Helper()
	req := httptest.NewRequest("POST", path, bytes.NewReader(body))
	req.Header.Set("Authorization", "Bearer "+bearer)
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, req)
	return rec
}

func status(port uint16, free int, accounts ...int64) []byte {
	out := []byte(`{"v":3,"port":` + strconv.Itoa(int(port)) + `,"free":` + strconv.Itoa(free) + `,"accounts":[`)
	for i, account := range accounts {
		if i > 0 {
			out = append(out, ',')
		}
		out = strconv.AppendInt(out, account, 10)
	}
	out = append(out, "],"...)
	return append(out, `"seated":`+strconv.Itoa(len(accounts))+`,"away":0,"passes":30,"pass_avg_us":120,"pass_max_us":900,"round_trip_ms":3,"capacity":16,"period_ms":1000,"host":"room.test"}`...)
}

// An account that just left a room goes back to it while a PvP lock that
// room remembers could still run, over the usual company rule; past the
// window, or with that room silent, the usual rule decides again.
func TestARecentRoomTakesItsAccountBack(t *testing.T) {
	st, _ := freshStore(t)
	h := newTestAPI(t, st).handler(testRoutes)
	ctx := context.Background()
	if _, err := st.pool.Exec(ctx, `UPDATE rooms SET seen_at = now() - interval '10 seconds' WHERE id = 9`); err != nil {
		t.Fatal(err)
	}
	// Room 2 held account 42, which then left; room 3 has company. Only
	// the departure is written, not every seated account.
	roomCall(t, h, "/rooms/status", roomBearer(testKey, 2, 5), status(7002, 14, 42, 44))
	roomCall(t, h, "/rooms/status", roomBearer(testKey, 2, 5), status(7002, 15, 44))
	var rows int
	if err := st.pool.QueryRow(ctx, `SELECT count(*) FROM last_rooms`).Scan(&rows); err != nil || rows != 1 {
		t.Fatalf("departures recorded: %d %v", rows, err)
	}
	roomCall(t, h, "/rooms/status", roomBearer(testKey, 2, 5), status(7002, 16))
	roomCall(t, h, "/rooms/status", roomBearer(testKey, 3, 5), status(7003, 15, 7))
	if room, err := st.pick(ctx, 43); err != nil || room.id != 3 {
		t.Fatalf("a stranger goes where the company is: %+v %v", room, err)
	}
	if room, err := st.pick(ctx, 42); err != nil || room.id != 2 {
		t.Fatalf("back to the room it left: %+v %v", room, err)
	}
	// Past the window the company rule is back (the booked seat comes
	// back with room 2's next status).
	roomCall(t, h, "/rooms/status", roomBearer(testKey, 2, 5), status(7002, 16))
	if _, err := st.pool.Exec(ctx, `UPDATE last_rooms SET left_at = now() - interval '80 seconds' WHERE account = 42`); err != nil {
		t.Fatal(err)
	}
	if room, err := st.pick(ctx, 42); err != nil || room.id != 3 {
		t.Fatalf("window over: %+v %v", room, err)
	}
	// A silent last room is never offered, window or not.
	roomCall(t, h, "/rooms/status", roomBearer(testKey, 2, 5), status(7002, 15, 42))
	roomCall(t, h, "/rooms/status", roomBearer(testKey, 2, 5), status(7002, 16))
	if _, err := st.pool.Exec(ctx, `UPDATE rooms SET seen_at = now() - interval '10 seconds' WHERE id = 2`); err != nil {
		t.Fatal(err)
	}
	if room, err := st.pick(ctx, 42); err != nil || room.id != 3 {
		t.Fatalf("silent last room offered: %+v %v", room, err)
	}
	// A status of an older life is refused and records no departure.
	roomCall(t, h, "/rooms/status", roomBearer(testKey, 3, 5), status(7003, 14, 7, 50))
	if rec := roomCall(t, h, "/rooms/status", roomBearer(testKey, 3, 4), status(7003, 16)); rec.Code != http.StatusConflict {
		t.Fatalf("older life: %d", rec.Code)
	}
	var stray int
	if err := st.pool.QueryRow(ctx, `SELECT count(*) FROM last_rooms WHERE account IN (7, 50)`).Scan(&stray); err != nil || stray != 0 {
		t.Fatalf("a refused status wrote departures: %d %v", stray, err)
	}
}

// Entry keeps players together without cramming: a room takes players
// until more than the comfort share of its seats is taken, then an empty
// room opens; among rooms with company the emptiest goes first, and
// only when there is no empty room does a crowded one take more.
func TestEntryKeepsCompanyUnderTheCeiling(t *testing.T) {
	st, _ := freshStore(t)
	st.comfort = 70
	h := newTestAPI(t, st).handler(testRoutes)
	ctx := context.Background()
	if _, err := st.pool.Exec(ctx, `UPDATE rooms SET seen_at = now() - interval '1 minute' WHERE id = 9`); err != nil {
		t.Fatal(err)
	}
	roomCall(t, h, "/rooms/status", roomBearer(testKey, 2, 5), status(7002, 16))
	roomCall(t, h, "/rooms/status", roomBearer(testKey, 3, 5), status(7003, 16))
	roomCall(t, h, "/rooms/status", roomBearer(testKey, 4, 5), status(7004, 4))
	var got []uint32
	for player := int64(100); player < 114; player++ {
		room, err := st.pick(ctx, player)
		if err != nil {
			t.Fatal(err)
		}
		got = append(got, room.id)
	}
	// Twelve into room 2 (the 12th makes it 75 %), then room 3 opens and
	// keeps taking: it is the emptiest room with company.
	want := []uint32{2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3}
	for i := range want {
		if got[i] != want[i] {
			t.Fatalf("picks: %v", got)
		}
	}
	// Room 2 back under the ceiling and emptier than room 3: it goes first.
	roomCall(t, h, "/rooms/status", roomBearer(testKey, 2, 5), status(7002, 10))
	roomCall(t, h, "/rooms/status", roomBearer(testKey, 3, 5), status(7003, 8))
	if room, err := st.pick(ctx, 200); err != nil || room.id != 2 {
		t.Fatalf("emptiest with company: %+v %v", room, err)
	}
	// Everything crowded and nothing empty: the emptiest crowded room.
	roomCall(t, h, "/rooms/status", roomBearer(testKey, 2, 5), status(7002, 2))
	roomCall(t, h, "/rooms/status", roomBearer(testKey, 3, 5), status(7003, 3))
	if room, err := st.pick(ctx, 201); err != nil || room.id != 4 {
		t.Fatalf("emptiest crowded: %+v %v", room, err)
	}
}

// A life of a room has one address. A status of the same life from
// another host or port (a ticket caught on the wire, replayed from
// elsewhere) is refused like an older life; the next life may move.
func TestALifeKeepsItsAddress(t *testing.T) {
	st, _ := freshStore(t)
	a := newTestAPI(t, st)
	h := a.handler(testRoutes)
	bearer := roomBearer(a.rooms.key, 5, 10)
	if rec := roomCall(t, h, "/rooms/status", bearer, status(7001, 3)); rec.Code != http.StatusNoContent {
		t.Fatalf("first status: %d %s", rec.Code, rec.Body.String())
	}
	moved := bytes.Replace(status(7001, 3), []byte(`"host":"room.test"`), []byte(`"host":"elsewhere.test"`), 1)
	if rec := roomCall(t, h, "/rooms/status", bearer, moved); rec.Code != http.StatusConflict {
		t.Fatalf("same life from another host: %d", rec.Code)
	}
	if rec := roomCall(t, h, "/rooms/status", bearer, status(7002, 3)); rec.Code != http.StatusConflict {
		t.Fatalf("same life on another port: %d", rec.Code)
	}
	if rec := roomCall(t, h, "/rooms/status", bearer, status(7001, 2)); rec.Code != http.StatusNoContent {
		t.Fatalf("same life at its own address: %d", rec.Code)
	}
	if rec := roomCall(t, h, "/rooms/status", roomBearer(a.rooms.key, 5, 11), moved); rec.Code != http.StatusNoContent {
		t.Fatalf("the next life elsewhere: %d", rec.Code)
	}
	rooms, err := st.listRooms(context.Background())
	if err != nil {
		t.Fatal(err)
	}
	for _, room := range rooms {
		if room.ID == 5 && room.Host == "elsewhere.test" && room.Incarnation == 11 {
			return
		}
	}
	t.Fatalf("registry: %+v", rooms)
}

// A flood of bad tickets on the public room routes is a counter and a
// sampled line, never a line per call and never a decode of a long header.
func TestBadTicketsAreCountedNotLogged(t *testing.T) {
	st, _ := freshStore(t)
	a := newTestAPI(t, st)
	h := a.handler(testRoutes)
	for i := 0; i < 50; i++ {
		if rec := roomCall(t, h, "/rooms/status", "deadbeef", status(7001, 3)); rec.Code != http.StatusUnauthorized {
			t.Fatalf("bad ticket %d: %d", i, rec.Code)
		}
	}
	long := strings.Repeat("ab", 8000)
	if rec := roomCall(t, h, "/rooms/status", long, status(7001, 3)); rec.Code != http.StatusUnauthorized {
		t.Fatalf("long bearer: %d", rec.Code)
	}
	if got := a.rooms.ticketsRefused.Load(); got != 51 {
		t.Fatalf("tickets_refused = %d", got)
	}
	// One line for the first, none for the rest within the period.
	if a.rooms.refusals.since != 50 {
		t.Fatalf("lines sampled: %d pending", a.rooms.refusals.since)
	}
}

// Rooms report themselves and the report is the registry: entry goes to
// the room holding the account, else to the most free seats; a room that
// stopped reporting is skipped; an older life of a room id is fenced,
// across a restart of this service too; a bad ticket is refused.
func TestRoomsReportAndAreRouted(t *testing.T) {
	st, _ := freshStore(t)
	h := newTestAPI(t, st).handler(testRoutes)
	ctx := context.Background()
	// The API fixture's own room is out of the picture here.
	if _, err := st.pool.Exec(ctx, `UPDATE rooms SET seen_at = now() - interval '10 seconds' WHERE id = 9`); err != nil {
		t.Fatal(err)
	}

	if rec := roomCall(t, h, "/rooms/status", roomBearer(testKey, 2, 5), status(7002, 2)); rec.Code != http.StatusNoContent {
		t.Fatalf("status: %d %s", rec.Code, rec.Body)
	}
	if rec := roomCall(t, h, "/rooms/status", roomBearer(testKey, 3, 5), status(7003, 9, 42)); rec.Code != http.StatusNoContent {
		t.Fatalf("status: %d %s", rec.Code, rec.Body)
	}
	if room, err := st.pick(ctx, 42); err != nil || room.id != 3 || room.port != 7003 || room.host != "room.test" {
		t.Fatalf("seated account: %+v %v", room, err)
	}
	if room, err := st.pick(ctx, 7); err != nil || room.id != 3 {
		t.Fatalf("company under the ceiling: %+v %v", room, err)
	}
	// A full room still takes its own account back; others go elsewhere.
	roomCall(t, h, "/rooms/status", roomBearer(testKey, 3, 5), status(7003, 0, 42))
	if room, err := st.pick(ctx, 42); err != nil || room.id != 3 {
		t.Fatalf("seated in a full room: %+v %v", room, err)
	}
	if room, err := st.pick(ctx, 7); err != nil || room.id != 2 {
		t.Fatalf("next room with seats: %+v %v", room, err)
	}
	// A room that fell silent is not offered.
	if _, err := st.pool.Exec(ctx, `UPDATE rooms SET seen_at = now() - interval '10 seconds' WHERE id = 2`); err != nil {
		t.Fatal(err)
	}
	if _, err := st.pick(ctx, 7); !errors.Is(err, errNoRoom) {
		t.Fatalf("stale room offered: %v", err)
	}

	// The fence: an older life is refused and changes nothing; the same
	// life reports on; a newer one takes over; the service restarting
	// forgets nothing because the row is the registry.
	if rec := roomCall(t, h, "/rooms/status", roomBearer(testKey, 3, 4), status(7004, 9)); rec.Code != http.StatusConflict {
		t.Fatalf("older life: %d", rec.Code)
	}
	if room, err := st.pick(ctx, 42); err != nil || room.port != 7003 {
		t.Fatalf("older life changed the row: %+v %v", room, err)
	}
	if rec := roomCall(t, h, "/rooms/status", roomBearer(testKey, 3, 5), status(7003, 1, 42)); rec.Code != http.StatusNoContent {
		t.Fatalf("same life: %d", rec.Code)
	}
	if rec := roomCall(t, h, "/rooms/status", roomBearer(testKey, 3, 6), status(7003, 9)); rec.Code != http.StatusNoContent {
		t.Fatalf("newer life: %d", rec.Code)
	}
	restarted := newTestAPI(t, st).handler(testRoutes)
	if rec := roomCall(t, restarted, "/rooms/status", roomBearer(testKey, 3, 5), status(7003, 9)); rec.Code != http.StatusConflict {
		t.Fatalf("older life after a restart: %d", rec.Code)
	}

	// Tickets: forged, expired, of the player kind, without a room or a
	// life — all refused before any work.
	bad := testKey
	bad[0] ^= 1
	expired := encodeTicket(testKey, ticketKindRoom, 5, 3, uint32(time.Now().Add(-time.Minute).Unix()))
	player := encodeTicket(testKey, ticketKindPlayer, 5, 3, uint32(time.Now().Add(time.Minute).Unix()))
	for name, bearer := range map[string]string{
		"forged":  roomBearer(bad, 3, 5),
		"expired": hex.EncodeToString(expired[:]),
		"player":  hex.EncodeToString(player[:]),
		"no room": roomBearer(testKey, 0, 5),
		"no life": roomBearer(testKey, 3, 0),
		"garbage": "zz",
	} {
		if rec := roomCall(t, h, "/rooms/status", bearer, status(7003, 9)); rec.Code != http.StatusUnauthorized {
			t.Errorf("%s ticket: %d", name, rec.Code)
		}
	}
	// A body of another shape is refused, which fences the room: a build
	// before the health fields, a version after this one, a field this
	// decoder has never heard of.
	for name, body := range map[string]string{
		"no port":        `{"v":2,"port":0}`,
		"older build":    `{"port":7003,"free":9,"accounts":[]}`,
		"newer version":  strings.Replace(string(status(7003, 9)), `"v":3`, `"v":4`, 1),
		"unknown field":  strings.Replace(string(status(7003, 9)), `"away":0`, `"away":0,"lag":1`, 1),
		"too many seats": strings.Replace(string(status(7003, 9)), `"seated":0`, `"seated":8`, 1),
		"over capacity":  strings.Replace(string(status(7003, 9)), `"capacity":16`, `"capacity":1000`, 1),
		"bad host":       strings.Replace(string(status(7003, 9)), `"host":"room.test"`, `"host":"room test"`, 1),
		"no host":        strings.Replace(string(status(7003, 9)), `"host":"room.test"`, `"host":""`, 1),
		"wrapped seats":  strings.Replace(string(status(7003, 9)), `"seated":0`, `"seated":4611686018427387920,"away":4611686018427387920`, 1),
	} {
		if rec := roomCall(t, h, "/rooms/status", roomBearer(testKey, 3, 6), []byte(body)); rec.Code != http.StatusBadRequest {
			t.Errorf("%s: %d", name, rec.Code)
		}
	}
	// Three 5-second periods are the liveness budget: the room remains routable
	// just before it, then becomes stale just after it.
	if rec := roomCall(t, h, "/rooms/status", roomBearer(testKey, 2, 5),
		[]byte(strings.Replace(string(status(7002, 1)), `"period_ms":1000`, `"period_ms":5000`, 1))); rec.Code != http.StatusNoContent {
		t.Fatalf("slow reporter: %d", rec.Code)
	}
	if _, err := st.pool.Exec(ctx, `UPDATE rooms SET seen_at = now() - interval '14 seconds' WHERE id = 2`); err != nil {
		t.Fatal(err)
	}
	listed, err := st.listRooms(ctx)
	if err != nil {
		t.Fatal(err)
	}
	found := false
	for _, room := range listed {
		if room.ID != 2 {
			continue
		}
		found = true
		if !room.Alive {
			t.Fatalf("slow reporter counted stale: %+v", room)
		}
	}
	if !found {
		t.Fatal("slow reporter missing from registry")
	}
	if _, err := st.pool.Exec(ctx, `UPDATE rooms SET seen_at = now() - interval '16 seconds' WHERE id = 2`); err != nil {
		t.Fatal(err)
	}
	listed, err = st.listRooms(ctx)
	if err != nil {
		t.Fatal(err)
	}
	found = false
	for _, room := range listed {
		if room.ID != 2 {
			continue
		}
		found = true
		if room.Alive {
			t.Fatalf("slow reporter counted alive after liveness budget: %+v", room)
		}
	}
	if !found {
		t.Fatal("slow reporter missing after staleness check")
	}
	// A health number out of any sane range is an observation, clamped, not a fence.
	huge := strings.Replace(string(status(7003, 9)), `"pass_max_us":900`, `"pass_max_us":2000000000`, 1)
	if rec := roomCall(t, h, "/rooms/status", roomBearer(testKey, 3, 6), []byte(huge)); rec.Code != http.StatusNoContent {
		t.Fatalf("huge pass time fenced the room: %d", rec.Code)
	}
}

// A pick books the seat it hands out, so a burst of entries is placed by
// the booked counts; a player already seated takes no seat; the room's
// next status restores the count.
func TestPickBooksTheSeat(t *testing.T) {
	st, _ := freshStore(t)
	h := newTestAPI(t, st).handler(testRoutes)
	ctx := context.Background()
	// The API fixture's own room is out of the picture here.
	if _, err := st.pool.Exec(ctx, `UPDATE rooms SET seen_at = now() - interval '1 minute' WHERE id = 9`); err != nil {
		t.Fatal(err)
	}
	roomCall(t, h, "/rooms/status", roomBearer(testKey, 2, 5), status(7002, 2))
	roomCall(t, h, "/rooms/status", roomBearer(testKey, 3, 5), status(7003, 1, 42))
	// Both rooms are crowded (14 and 15 of 16 taken), so the emptier one
	// goes first: two players into room 2, the third finds it booked down
	// to 0 and takes room 3's last seat.
	got := []uint32{}
	for _, player := range []int64{7, 8, 9} {
		room, err := st.pick(ctx, player)
		if err != nil {
			t.Fatal(err)
		}
		got = append(got, room.id)
	}
	if got[0] != 2 || got[1] != 2 || got[2] != 3 {
		t.Fatalf("picks: %v", got)
	}
	// Nothing left anywhere for a fourth; the seated account still gets in.
	if _, err := st.pick(ctx, 10); !errors.Is(err, errNoRoom) {
		t.Fatalf("fourth player: %v", err)
	}
	if room, err := st.pick(ctx, 42); err != nil || room.id != 3 {
		t.Fatalf("seated account: %+v %v", room, err)
	}
	// The status is the truth again.
	roomCall(t, h, "/rooms/status", roomBearer(testKey, 2, 5), status(7002, 2))
	if room, err := st.pick(ctx, 11); err != nil || room.id != 2 {
		t.Fatalf("after the status: %+v %v", room, err)
	}
}

func TestConcurrentPicksDoNotOverbookLastSeat(t *testing.T) {
	st, _ := freshStore(t)
	ctx := context.Background()
	if _, err := st.reportRoom(ctx, 2, 5, &roomStatus{Port: 7002, Free: 1, Capacity: 20, PeriodMs: 1000, Host: "room.test"}); err != nil {
		t.Fatal(err)
	}
	lock, err := st.pool.Begin(ctx)
	if err != nil {
		t.Fatal(err)
	}
	defer lock.Rollback(ctx)
	if _, err := lock.Exec(ctx, `SELECT id FROM rooms WHERE id = 2 FOR UPDATE`); err != nil {
		t.Fatal(err)
	}
	results := make(chan error, 8)
	var started sync.WaitGroup
	started.Add(8)
	for player := int64(100); player < 108; player++ {
		go func(player int64) {
			started.Done()
			_, err := st.pick(ctx, player)
			results <- err
		}(player)
	}
	started.Wait()
	// Release only after competing snapshots wait on the row or all picks refuse it.
	deadline := time.Now().Add(5 * time.Second)
	for {
		var waiting int
		if _, err := lock.Exec(ctx, `SELECT pg_stat_clear_snapshot()`); err != nil {
			t.Fatal(err)
		}
		if err := lock.QueryRow(ctx, `SELECT count(*) FROM pg_stat_activity WHERE datname = current_database() AND wait_event_type = 'Lock'`).Scan(&waiting); err != nil {
			t.Fatal(err)
		}
		if waiting >= 2 || len(results) == 8 {
			break
		}
		if time.Now().After(deadline) {
			t.Fatal("picks did not reach the contention boundary")
		}
		time.Sleep(time.Millisecond)
	}
	if err := lock.Commit(ctx); err != nil {
		t.Fatal(err)
	}
	issued := 0
	for range 8 {
		err := <-results
		if err == nil {
			issued++
		} else if !errors.Is(err, errNoRoom) {
			t.Fatal(err)
		}
	}
	if issued > 1 {
		t.Fatalf("issued %d tickets for one free seat", issued)
	}
	if issued == 0 {
		if _, err := st.pick(ctx, 200); err != nil {
			t.Fatalf("unlocked seat unavailable: %v", err)
		}
	}
	if _, err := st.pick(ctx, 201); !errors.Is(err, errNoRoom) {
		t.Fatalf("exhausted seat: %v", err)
	}
}

// The fixture is the one shape both sides pin: the room's test proves it
// writes this shape, this test proves the decoder reads exactly it.
func TestStatusFixtureIsTheContract(t *testing.T) {
	body, err := os.ReadFile(filepath.Join("testdata", "room_status_v3.json"))
	if err != nil {
		t.Fatal(err)
	}
	st, _ := freshStore(t)
	h := newTestAPI(t, st).handler(testRoutes)
	if rec := roomCall(t, h, "/rooms/status", roomBearer(testKey, 4, 5), body); rec.Code != http.StatusNoContent {
		t.Fatalf("fixture refused: %d %s", rec.Code, rec.Body)
	}
	rooms, err := st.listRooms(context.Background())
	if err != nil {
		t.Fatal(err)
	}
	var found *roomRow
	for i := range rooms {
		if rooms[i].ID == 4 {
			found = &rooms[i]
		}
	}
	if found == nil || found.Port != 7001 || found.Free != 3 || found.Seated != 1 || found.Passes != 30 ||
		found.PassAvgUs != 13 || found.PassMaxUs != 129 || found.RoundTripMs != 20 || found.Capacity != 4 ||
		found.PeriodMs != 1000 || found.Host != "room.test" || len(found.Accounts) != 1 || found.Accounts[0] != 4242 {
		t.Fatalf("fixture stored as %+v", found)
	}
}
