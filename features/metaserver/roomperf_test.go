package metaserver

import (
	"bytes"
	"encoding/json"
	"net/http"
	"os"
	"path/filepath"
	"testing"
	"time"
)

func perfFixture(t *testing.T) []byte {
	t.Helper()
	body, err := os.ReadFile(filepath.Join("testdata", "room_perf_v1.json"))
	if err != nil {
		t.Fatal(err)
	}
	return bytes.TrimSpace(body)
}

// The fixture is the shape the room's test proves it writes; this proves
// every field of it lands somewhere.
func TestPerfFixtureDecodesWhole(t *testing.T) {
	var loose map[string]any
	raw := perfFixture(t)
	if err := json.Unmarshal(raw, &loose); err != nil {
		t.Fatal(err)
	}
	p, ok := decodeRoomPerf(raw)
	if !ok {
		t.Fatal("fixture refused")
	}
	again, _ := json.Marshal(p)
	var round map[string]any
	json.Unmarshal(again, &round)
	var walk func(prefix string, want, got map[string]any)
	walk = func(prefix string, want, got map[string]any) {
		for k, v := range want {
			g, ok := got[k]
			if !ok {
				t.Errorf("%s%s is not decoded", prefix, k)
				continue
			}
			if inner, ok := v.(map[string]any); ok {
				walk(prefix+k+".", inner, g.(map[string]any))
			}
		}
	}
	walk("", loose, round)
}

func TestPerfWindowsTheServerCannotSumAreRefused(t *testing.T) {
	raw := perfFixture(t)
	if _, ok := decodeRoomPerf(bytes.Replace(raw, []byte(`"b":[0,0,0,10,`), []byte(`"b":[0,10,`), 1)); ok {
		t.Error("a histogram of the wrong length was taken")
	}
	if _, ok := decodeRoomPerf(bytes.Replace(raw, []byte(`"window_ms":2000`), []byte(`"window_ms":0`), 1)); ok {
		t.Error("an empty window was taken")
	}
	if _, ok := decodeRoomPerf(bytes.Replace(raw, []byte(`{"window_ms"`), []byte(`{"future":1,"window_ms"`), 1)); !ok {
		t.Error("a field a newer room adds refused the window")
	}
}

func TestQuantilesAreBucketBoundsCappedByTheMax(t *testing.T) {
	h := perfHist{B: make([]uint64, perfBuckets), Max: 50}
	h.B[0], h.B[2] = 90, 10 // 90 under 16 us, 10 in [32, 64)
	v := viewStage(&h)
	if v.Count != 100 || v.P50Us != 16 || v.P95Us != 50 || v.P99Us != 50 || v.MaxUs != 50 {
		t.Fatalf("%+v", v)
	}
	h = perfHist{B: make([]uint64, perfBuckets), Max: 3_000_000}
	h.B[perfBuckets-1] = 1
	if v := viewStage(&h); v.P99Us != 3_000_000 {
		t.Fatalf("the overflow bucket reads %d", v.P99Us)
	}
}

func TestTheBoardSumsRoomsMachinesAndMinutes(t *testing.T) {
	p, _ := decodeRoomPerf(perfFixture(t))
	b := newPerfBoard()
	at := time.Unix(1_800_000_000, 0)
	st := &roomStatus{Host: "b.test", Seated: 16, Capacity: 16}
	for i := 0; i < 30; i++ {
		now := at.Add(time.Duration(i) * 2 * time.Second)
		b.record(now, 1, 7, st, p)
		b.record(now, 2, 7, st, p)
	}
	snap := b.snapshot(at.Add(59 * time.Second))
	if len(snap.Rooms) != 2 || len(snap.Machines) != 1 || snap.Fleet == nil {
		t.Fatalf("rooms %d machines %d fleet %v", len(snap.Rooms), len(snap.Machines), snap.Fleet)
	}
	room := snap.Rooms[0]
	// 960 player-ticks in 60 ticks over 2 s: 16 players, 32 player-seconds a window.
	if room.WindowS != 60 || room.AvgSeated != 16 || room.PlayerSeconds != 16*60 {
		t.Fatalf("room %+v", room.perfView)
	}
	// 100 ms of CPU a window over 32 player-seconds.
	if room.CPUMsPerPlayerS == nil || *room.CPUMsPerPlayerS < 3.12 || *room.CPUMsPerPlayerS > 3.13 {
		t.Fatalf("cpu per player-second %v", room.CPUMsPerPlayerS)
	}
	if room.RxMsgPerPlayerS != 30 || room.WritesPerMsg != 1 || room.Closes["app"] != 30 {
		t.Fatalf("room net %+v closes %v", room.perfView, room.Closes)
	}
	if room.Stages["step"].P99Us != 250 || room.Stages["input_wait"].P50Us != 65536 {
		t.Fatalf("stages %+v", room.Stages)
	}
	// Six reports a window, thirty windows: 15 player-minutes, 90 corrections.
	if c := room.Clients; c == nil || c.Reports != 180 || c.RttP50Ms != 65.536 || c.RttMaxMs != 120 || c.CorrectionsMin != 6 ||
		c.WebPct != 100 || c.FpsAvg != 55 {
		t.Fatalf("clients %+v", room.Clients)
	}
	if f := snap.Fleet; f.AvgSeated != 32 || f.RxMsgPerS != 2*480 {
		t.Fatalf("fleet seated %v rx %v", f.AvgSeated, f.RxMsgPerS)
	}
	m := snap.Machines[0]
	// 760 jiffies: 360 busy, 60 of them softirq.
	if m.Rooms != 2 || m.Seated != 32 || m.CPUPct < 47.3 || m.CPUPct > 47.4 || m.SoftirqPct < 7.8 || m.SoftirqPct > 7.9 || m.RxPps != 15000 {
		t.Fatalf("machine %+v", m)
	}
	if len(snap.Minutes) != 0 {
		t.Fatalf("a minute still running is history: %+v", snap.Minutes)
	}
	later := b.snapshot(at.Add(2 * time.Minute))
	if len(later.Minutes) == 0 || len(later.Rooms) != 0 {
		t.Fatalf("minutes %d rooms %d", len(later.Minutes), len(later.Rooms))
	}
	// A new life of the room starts its windows over; a silent room is forgotten.
	b.record(at.Add(3*time.Minute), 1, 8, st, p)
	if snap := b.snapshot(at.Add(3 * time.Minute)); len(snap.Rooms) != 1 || snap.Rooms[0].Windows != 1 {
		t.Fatalf("after restart %+v", snap.Rooms)
	}
	if snap := b.snapshot(at.Add(20 * time.Minute)); len(snap.Rooms) != 0 || len(snap.Machines) != 0 {
		t.Fatal("a silent room stayed on the page")
	}
}

func statusWithPerf(t *testing.T, perf []byte) []byte {
	t.Helper()
	body, err := os.ReadFile(filepath.Join("testdata", "room_status_v3.json"))
	if err != nil {
		t.Fatal(err)
	}
	body = bytes.TrimSpace(body)
	return append(append(append(body[:len(body)-1:len(body)-1], `,"perf":`...), perf...), '}')
}

// Telemetry rides the status and never costs it: a window the server cannot
// read is counted and the room is routed all the same.
func TestTheStatusCarriesThePerfWindowToThePage(t *testing.T) {
	st, _ := freshStore(t)
	a := newTestAPI(t, st)
	h := a.handler(testRoutes)
	if rec := roomCall(t, h, "/rooms/status", roomBearer(testKey, 4, 5), statusWithPerf(t, perfFixture(t))); rec.Code != http.StatusNoContent {
		t.Fatalf("status with perf refused: %d %s", rec.Code, rec.Body)
	}
	broken := bytes.Replace(perfFixture(t), []byte(`"b":[0,0,0,10,`), []byte(`"b":[0,10,`), 1)
	if rec := roomCall(t, h, "/rooms/status", roomBearer(testKey, 5, 5), statusWithPerf(t, broken)); rec.Code != http.StatusNoContent {
		t.Fatalf("an unreadable window refused the status: %d %s", rec.Code, rec.Body)
	}
	snap := a.rooms.perf.snapshot(time.Now())
	if len(snap.Rooms) != 1 || snap.Rooms[0].ID != 4 || snap.Refused != 1 || len(snap.Machines) != 1 {
		t.Fatalf("page sees %+v", snap)
	}
}
