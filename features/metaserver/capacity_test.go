package metaserver

import (
	"context"
	"testing"
	"time"
)

func testCapacityMinute(at time.Time) capacityMinute {
	cpu := 3.5
	return capacityMinute{at: at,
		fleet: &minuteView{Windows: 30, AvgSeated: 1, CPUMsPerPlayerS: &cpu, StepP99Us: 256, LateTicks: 2, TxMbitS: 0.5},
		machines: []machineView{{Host: "room.test", CPUPct: 40, UserPct: 30, RxMbitS: 2, TxMbitS: 8, RxPps: 100, TxPps: 200},
			{Host: "idle.test", CPUPct: 5}},
		api: map[string]endpointObservation{"enter": {Requests: 10, ServerErrors: 1, AvgMs: 2, MaxMs: 5}}}
}

func TestCapacitySamplerWritesEachMinuteOnce(t *testing.T) {
	st, _ := freshStore(t)
	ctx := context.Background()
	for id, host := range map[int64]string{9: "room.test", 10: "silent.test"} {
		status := &roomStatus{V: statusVersion, Port: 7001, Host: host, Free: 3, Seated: 1, Away: 0, Capacity: 4, PeriodMs: 1000}
		if accepted, err := st.reportRoom(ctx, uint32(id), 1, status); err != nil || !accepted {
			t.Fatalf("room status: %v %v", accepted, err)
		}
	}
	minute := time.Now().UTC().Truncate(time.Minute)
	if err := st.sampleCapacity(ctx, testCapacityMinute(minute)); err != nil {
		t.Fatal(err)
	}
	retry := testCapacityMinute(minute)
	retry.machines[0].CPUPct = 99
	if err := st.sampleCapacity(ctx, retry); err != nil {
		t.Fatal(err)
	}
	type machine struct {
		rooms, seated, capacity int
		cpu                     *float64
	}
	got := map[string]machine{}
	rows, err := st.pool.Query(ctx, `SELECT host, rooms, seated, capacity, cpu_pct FROM activity.machine_minutes WHERE minute_utc=$1`, minute)
	if err != nil {
		t.Fatal(err)
	}
	for rows.Next() {
		var host string
		var m machine
		if err := rows.Scan(&host, &m.rooms, &m.seated, &m.capacity, &m.cpu); err != nil {
			t.Fatal(err)
		}
		got[host] = m
	}
	if err := rows.Err(); err != nil {
		t.Fatal(err)
	}
	if m := got["room.test"]; len(got) != 3 || m.rooms != 1 || m.seated != 1 || m.capacity != 4 || m.cpu == nil || *m.cpu != 40 {
		t.Fatalf("machines after retry: %+v", got)
	}
	if m := got["idle.test"]; m.rooms != 0 || m.cpu == nil || *m.cpu != 5 {
		t.Fatalf("a machine without rooms: %+v", m)
	}
	if m := got["silent.test"]; m.rooms != 1 || m.cpu != nil {
		t.Fatalf("rooms without a load window: %+v", m)
	}
	var stepP99, errors5xx int
	if err := st.pool.QueryRow(ctx, `SELECT f.step_p99_us, a.server_errors FROM activity.fleet_minutes f
		JOIN activity.api_minutes a USING (minute_utc) WHERE minute_utc=$1 AND a.endpoint='enter'`, minute).Scan(&stepP99, &errors5xx); err != nil {
		t.Fatal(err)
	}
	if stepP99 != 256 || errors5xx != 1 {
		t.Fatalf("fleet step p99 %d, enter 5xx %d", stepP99, errors5xx)
	}
	// A quiet minute is a run without rows: zero requests, not a missing minute.
	quiet := minute.Add(time.Minute)
	if err := st.sampleCapacity(ctx, capacityMinute{at: quiet}); err != nil {
		t.Fatal(err)
	}
	var runs, apiRows int
	if err := st.pool.QueryRow(ctx, `SELECT count(*), (SELECT count(*) FROM activity.api_minutes WHERE minute_utc=$1)
		FROM activity.capacity_runs`, quiet).Scan(&runs, &apiRows); err != nil {
		t.Fatal(err)
	}
	if runs != 2 || apiRows != 0 {
		t.Fatalf("quiet minute: runs=%d api rows=%d", runs, apiRows)
	}
}

func TestCapacityRetentionPrunesOldMinutesWithTheirRows(t *testing.T) {
	st, _ := freshStore(t)
	ctx := context.Background()
	now := time.Now().UTC().Truncate(time.Minute)
	old := now.Add(-91 * 24 * time.Hour)
	if err := st.sampleCapacity(ctx, testCapacityMinute(old)); err != nil {
		t.Fatal(err)
	}
	if err := st.sampleCapacity(ctx, testCapacityMinute(now)); err != nil {
		t.Fatal(err)
	}
	if removed, err := st.pruneCapacity(ctx, now); err != nil || removed != 1 {
		t.Fatalf("prune removed=%d err=%v", removed, err)
	}
	var runs, machines, fleet, api int
	if err := st.pool.QueryRow(ctx, `SELECT (SELECT count(*) FROM activity.capacity_runs),
		(SELECT count(*) FROM activity.machine_minutes WHERE minute_utc < $1),
		(SELECT count(*) FROM activity.fleet_minutes WHERE minute_utc < $1),
		(SELECT count(*) FROM activity.api_minutes WHERE minute_utc < $1)`, now).Scan(&runs, &machines, &fleet, &api); err != nil {
		t.Fatal(err)
	}
	if runs != 1 || machines != 0 || fleet != 0 || api != 0 {
		t.Fatalf("retention left runs=%d machines=%d fleet=%d api=%d", runs, machines, fleet, api)
	}
}

func TestMinuteHistoryAveragesEachMachineOverTheMinute(t *testing.T) {
	p, _ := decodeRoomPerf(perfFixture(t))
	b := newPerfBoard()
	at := time.Unix(1_800_000_000, 0) // a whole minute
	st := &roomStatus{Host: "b.test", Seated: 16, Capacity: 16}
	for i := 0; i < 30; i++ {
		b.record(at.Add(time.Duration(i)*2*time.Second), 1, 7, st, p)
		b.record(at.Add(time.Duration(i)*2*time.Second), 2, 7, st, p)
	}
	minute := at.Unix() / 60
	fleet, machines := b.minuteHistory(minute)
	// Overlapping windows from both rooms still give the machine's average: 360 of 760 jiffies busy.
	if fleet == nil || fleet.AvgSeated != 32 || len(machines) != 1 || machines[0].CPUPct < 47.3 || machines[0].CPUPct > 47.4 ||
		machines[0].RxPps != 15000 {
		t.Fatalf("minute: fleet %+v machines %+v", fleet, machines)
	}
	// The next minute's first window moves the whole one aside, still readable.
	b.record(at.Add(time.Minute), 1, 7, st, p)
	if _, machines := b.minuteHistory(minute); len(machines) != 1 || machines[0].RxPps != 15000 {
		t.Fatalf("ended minute after the next began: %+v", machines)
	}
	if fleet, machines := b.minuteHistory(minute - 5); fleet != nil || len(machines) != 0 {
		t.Fatalf("a minute nobody reported in: %+v %+v", fleet, machines)
	}
}

func TestObservationsKeepEachMinuteByRoute(t *testing.T) {
	o := newObservations()
	at := time.Unix(1_800_000_000, 0)
	o.record(at, "enter", 200, 2*time.Millisecond)
	o.record(at.Add(time.Second), "enter", 500, 4*time.Millisecond)
	o.record(at.Add(time.Minute), "enter", 200, time.Millisecond)
	got := o.minuteEndpoints(at.Unix() / 60)["enter"]
	if got.Requests != 2 || got.ServerErrors != 1 || got.AvgMs != 3 || got.MaxMs != 4 {
		t.Fatalf("minute %+v", got)
	}
	if len(o.minuteEndpoints(at.Unix()/60-1)) != 0 {
		t.Fatal("an empty minute returned requests")
	}
}
