package metaserver

import (
	"strings"
	"testing"
	"time"
)

// A condition pages only after it held alertHold, however often the heartbeat
// beats; one beat without it starts it over.
func TestAlertsFireOnlyAfterHolding(t *testing.T) {
	tr := newAlertTracker()
	late := []alertCondition{{"tick_late", "tick lateness past two ticks (max 90 ms)"}}
	at := time.Unix(1_800_000_000, 0)
	// Beats every 20 s: the hold is time, not a count of beats.
	for s := 0; s < int(alertHold/time.Second); s += 20 {
		if firing := tr.observe(at.Add(time.Duration(s)*time.Second), late); len(firing) != 0 {
			t.Fatalf("at %d s fired %v", s, firing)
		}
	}
	if firing := tr.observe(at.Add(alertHold), late); len(firing) != 1 || !strings.Contains(firing[0], "90 ms") {
		t.Fatalf("held %v, fired %v", alertHold, firing)
	}
	if v := tr.snapshot(); len(v) != 1 || !v[0].Firing || !v[0].Since.Equal(at) {
		t.Fatalf("page %+v", v)
	}
	tr.observe(at.Add(alertHold+time.Minute), nil)
	if firing := tr.observe(at.Add(alertHold+2*time.Minute), late); len(firing) != 0 {
		t.Fatal("a condition that went away and came back fired at once")
	}
}

func TestAlertConditionsReadTicksDisconnectsAndTraffic(t *testing.T) {
	p, _ := decodeRoomPerf(perfFixture(t))
	bad := *p
	bad.Stages.Late = perfHist{B: make([]uint64, perfBuckets), Max: 90_000, Sum: 90_000}
	bad.Stages.Late.B[13] = 1 // one tick in [64, 131) ms
	bad.Dropped = 2
	bad.Closes = []uint64{30, 0, 0, 9, 0, 0} // players leaving do not count; slow closes do
	b := newPerfBoard()
	now := time.Unix(1_800_000_000, 0)
	b.record(now, 1, 7, &roomStatus{Host: "b.test", Seated: 16, Capacity: 16}, &bad)
	traffic := newTrafficLedger()
	traffic.note(now.Add(-30*time.Hour), "b.test", 0, 1)
	traffic.note(now, "b.test", 50*gb, 50*gb)
	kinds := map[string]bool{}
	for _, c := range alertConditions(now, b, traffic, 1000) {
		kinds[c.Kind] = true
	}
	for _, want := range []string{"tick_late", "room_late", "tick_dropped", "disconnects", "traffic:b.test"} {
		if !kinds[want] {
			t.Errorf("%s did not hold: %v", want, kinds)
		}
	}
	// A projection from less than a day's counting is not trusted yet.
	young := newTrafficLedger()
	young.note(now.Add(-2*time.Hour), "b.test", 0, 1)
	young.note(now, "b.test", 50*gb, 50*gb)
	if c := alertConditions(now, newPerfBoard(), young, 1000); len(c) != 0 {
		t.Fatalf("a two-hour projection alerted: %+v", c)
	}
	// A quiet room and a plan without an allowance hold nothing.
	quiet := newPerfBoard()
	quiet.record(now, 1, 7, &roomStatus{Host: "b.test", Seated: 16, Capacity: 16}, p)
	if c := alertConditions(now, quiet, traffic, 0); len(c) != 0 {
		t.Fatalf("quiet stand held %+v", c)
	}
}
