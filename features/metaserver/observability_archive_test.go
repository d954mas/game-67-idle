package metaserver

import (
	"os"
	"path/filepath"
	"testing"
	"time"
)

func TestObservationArchiveSurvivesRestart(t *testing.T) {
	path := filepath.Join(t.TempDir(), "history.json")
	o := newObservations()
	now := time.Now()
	o.record(now.Add(-12*time.Hour), "enter", 503, 4*time.Millisecond)
	a := newObservationArchive(path, o, &dbHealth{}, nil)
	if err := a.save(); err != nil {
		t.Fatal(err)
	}
	restored := newObservations()
	b := newObservationArchive(path, restored, &dbHealth{}, nil)
	if err := b.load(); err != nil {
		t.Fatal(err)
	}
	restored.record(now, "enter", 200, 2*time.Millisecond)
	s := restored.snapshot(now)
	if s.Endpoints["enter"].Requests != 2 || s.Endpoints["enter"].AvgMs != 3 || len(s.Events) != 1 || len(s.Groups) != 1 || s.Groups[0].Count != 1 {
		t.Fatalf("history lost: %+v", s)
	}
	if restored.historyState().LastSaved.IsZero() {
		t.Fatal("missing persistence timestamp")
	}
	if err := b.save(); err != nil {
		t.Fatal(err)
	}
	if err := a.load(); err != nil {
		t.Fatal(err)
	}
}

func TestObservationArchiveFailureIsVisible(t *testing.T) {
	path := filepath.Join(t.TempDir(), "history.json")
	o := newObservations()
	a := newObservationArchive(path, o, &dbHealth{}, nil)
	if err := a.load(); err != nil {
		t.Fatal("missing initial file must be allowed", err)
	}
	if err := os.WriteFile(path, []byte("broken"), 0600); err != nil {
		t.Fatal(err)
	}
	if err := a.load(); err == nil || o.historyState().Error == "" {
		t.Fatal("corruption hidden")
	}
	bad := newObservationArchive(t.TempDir(), o, &dbHealth{}, nil)
	if err := bad.save(); err == nil || o.historyState().Error == "" {
		t.Fatal("write failure hidden")
	}
}
