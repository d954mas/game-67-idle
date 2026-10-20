package metaserver

import (
	"bytes"
	"context"
	"encoding/json"
	"net/http"
	"testing"
	"time"
)

func TestPlanBelievedOnlyWhenItHoldsTheRoom(t *testing.T) {
	plan := &perfPlan{Rooms: 32, RoomIDBase: 1000}
	for id, want := range map[uint32]bool{1000: false, 1001: true, 1032: true, 1033: false} {
		if _, ok := planOf(&roomPerf{Plan: plan}, id); ok != want {
			t.Fatalf("room %d: %v", id, ok)
		}
	}
	if _, ok := planOf(&roomPerf{Plan: &perfPlan{Rooms: planRoomsMax + 1}}, 1); ok {
		t.Fatal("a plan past the bound was believed")
	}
	if _, ok := planOf(&roomPerf{}, 1001); ok {
		t.Fatal("a window without a plan has one")
	}
}

func TestPlanWrittenOnChangeAndOnceAMinute(t *testing.T) {
	var c planCache
	at := time.Unix(1_800_000_000, 0)
	plan := perfPlan{Rooms: 32, RoomIDBase: 1000}
	if !c.due("b.test", plan, at) || c.due("b.test", plan, at.Add(time.Second)) {
		t.Fatal("the first report writes, a repeat within the minute does not")
	}
	if !c.due("b.test", perfPlan{Rooms: 64, RoomIDBase: 1000}, at.Add(2*time.Second)) {
		t.Fatal("a changed plan waits")
	}
	if !c.due("b.test", perfPlan{Rooms: 64, RoomIDBase: 1000}, at.Add(2*time.Second+planRefresh)) {
		t.Fatal("a minute-old plan is not refreshed")
	}
	c.forget("b.test")
	if !c.due("b.test", perfPlan{Rooms: 64, RoomIDBase: 1000}, at.Add(3*time.Second+planRefresh)) {
		t.Fatal("a failed write is not retried")
	}
}

func TestRegistryKeepsPlannedRoomsAndPrunesSwitchedOff(t *testing.T) {
	st, _ := freshStore(t)
	ctx := context.Background()
	for id, host := range map[uint32]string{1001: "b.test", 1032: "b.test", 1040: "b.test", 1050: "b.test", 7: "old.test"} {
		status := &roomStatus{V: statusVersion, Port: 7001, Host: host, Free: 3, Capacity: 4, PeriodMs: 1000}
		if accepted, err := st.reportRoom(ctx, id, 1, status); err != nil || !accepted {
			t.Fatalf("room %d: %v %v", id, accepted, err)
		}
	}
	if err := st.reportPlan(ctx, "b.test", perfPlan{Rooms: 32, RoomIDBase: 1000}); err != nil {
		t.Fatal(err)
	}
	// A day of silence for every room but 1050, which was switched off a moment ago.
	if _, err := st.pool.Exec(ctx, `UPDATE rooms SET seen_at = now() - interval '25 hours' WHERE id <> 1050`); err != nil {
		t.Fatal(err)
	}
	removed, err := st.pruneRegistry(ctx, time.Now())
	if err != nil || removed != 2 {
		t.Fatalf("pruned %d: %v", removed, err)
	}
	var left []int64
	rows, err := st.pool.Query(ctx, `SELECT id FROM rooms ORDER BY id`)
	if err != nil {
		t.Fatal(err)
	}
	for rows.Next() {
		var id int64
		rows.Scan(&id)
		left = append(left, id)
	}
	rows.Close()
	// Planned 1001 and 1032 stay (their silence is an alarm), 1050 is recent;
	// 1040 is outside the plan and the room of a host without one is gone.
	if len(left) != 3 || left[0] != 1001 || left[1] != 1032 || left[2] != 1050 {
		t.Fatalf("registry after pruning: %v", left)
	}
	if _, err := st.pool.Exec(ctx, `UPDATE room_machines SET reported_at = now() - interval '8 days'`); err != nil {
		t.Fatal(err)
	}
	// The plan goes with a week of silence; its rooms, a day silent, follow it.
	if removed, err := st.pruneRegistry(ctx, time.Now()); err != nil || removed != 2 {
		t.Fatalf("a week-silent machine: pruned %d: %v", removed, err)
	}
}

// The plan rides the perf window, so an older metaserver, which reads that
// window loosely, takes the status of a room that sends one.
func TestStatusPerfCarriesTheMachinePlan(t *testing.T) {
	st, _ := freshStore(t)
	a := newTestAPI(t, st)
	h := a.handler(testRoutes)
	perf := bytes.TrimSpace(perfFixture(t))
	withPlan := append(append(perf[:len(perf)-1:len(perf)-1], `,"plan":{"rooms":8,"room_id_base":0}`...), '}')
	if rec := roomCall(t, h, "/rooms/status", roomBearer(testKey, 4, 5), statusWithPerf(t, withPlan)); rec.Code != http.StatusNoContent {
		t.Fatalf("status with a plan refused: %d %s", rec.Code, rec.Body)
	}
	var rooms, base int64
	if err := st.pool.QueryRow(context.Background(), `SELECT rooms, room_id_base FROM room_machines`).Scan(&rooms, &base); err != nil ||
		rooms != 8 || base != 0 {
		t.Fatalf("machine plan: %d %d %v", rooms, base, err)
	}
	var old roomPerf
	if err := json.Unmarshal(withPlan, &struct{ *roomPerf }{&old}); err != nil {
		t.Fatalf("a window with a plan does not decode loosely: %v", err)
	}
}
