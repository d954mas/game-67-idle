package metaserver

import (
	"encoding/json"
	"testing"
	"time"
)

func TestDBHealthDayWindow(t *testing.T) {
	now := time.Unix(1800000000, 0)
	h := &dbHealth{}
	for _, age := range []time.Duration{24 * time.Hour, 23 * time.Hour, 2 * time.Hour, 0} {
		m := h.bucket(now.Add(-age))
		m.Queries++
		m.total += 4 * time.Millisecond
		m.MaxMs = 4
	}
	s := h.summary(now)
	if len(s.Minutes) != 1440 || s.Queries != 3 || s.AvgMs != 4 {
		t.Fatalf("wrong day window: minutes=%d queries=%d avg=%f", len(s.Minutes), s.Queries, s.AvgMs)
	}
	if got := h.summary(now.Add(24 * time.Hour)); got.Queries != 0 {
		t.Fatalf("expired queries remain: %d", got.Queries)
	}
}

func TestDBHealthLastErrorExpiry(t *testing.T) {
	now := time.Unix(1800000000, 0)
	h := &dbHealth{last: &dbError{At: now.Add(-23 * time.Hour), Text: "database unavailable"}}
	if h.summary(now).Last == nil {
		t.Fatal("recent error disappeared")
	}
	if h.summary(now.Add(time.Hour)).Last != nil {
		t.Fatal("expired last error remains")
	}
}

func TestDBHealthCheckpointPreservesTotalsAndOwnership(t *testing.T) {
	now := time.Now()
	h := &dbHealth{last: &dbError{At: now, Text: "database unavailable"}}
	m := h.bucket(now.Add(-2 * time.Hour))
	m.Queries, m.Errors, m.total, m.MaxMs = 2, 1, 7*time.Millisecond, 5
	state := h.checkpoint()
	encoded, err := json.Marshal(state)
	if err != nil {
		t.Fatal(err)
	}
	var decoded dbHealthState
	if err := json.Unmarshal(encoded, &decoded); err != nil {
		t.Fatal(err)
	}
	restored := &dbHealth{}
	restored.restore(decoded)
	state.Last.Text = "changed checkpoint"
	decoded.Last.Text = "changed input"
	if h.summary(now).Last.Text != "database unavailable" {
		t.Fatal("checkpoint aliases live error")
	}
	s := restored.summary(now)
	if s.Queries != 2 || s.Errors != 1 || s.AvgMs != 3.5 || s.MaxMs != 5 || s.Last.Text != "database unavailable" {
		t.Fatalf("checkpoint lost metrics: queries=%d errors=%d avg=%f max=%f last=%+v", s.Queries, s.Errors, s.AvgMs, s.MaxMs, s.Last)
	}
	s.Last.Text = "changed summary"
	if restored.summary(now).Last.Text != "database unavailable" {
		t.Fatal("summary aliases live error")
	}
}

func TestDBHealthCheckpointExcludesExpiredData(t *testing.T) {
	now := time.Now()
	h := &dbHealth{last: &dbError{At: now.Add(-25 * time.Hour), Text: "expired error"}}
	old := h.bucket(now.Add(-25 * time.Hour))
	old.Queries, old.Errors, old.total = 10, 1, time.Second
	recent := h.bucket(now)
	recent.Queries, recent.total = 1, time.Millisecond
	state := h.checkpoint()
	if state.Last != nil {
		t.Fatal("checkpoint retained expired error")
	}
	queries := 0
	for _, m := range state.Minutes {
		queries += m.Queries
		if m.Errors != 0 {
			t.Fatal("checkpoint retained expired bucket")
		}
	}
	if queries != 1 {
		t.Fatalf("wrong checkpoint query count: %d", queries)
	}
}
