package metaserver

import (
	"testing"
	"time"
)

// A first sighting and a repeated status are the same life; a newer
// incarnation is a restart; an older one (a stale status) is not; and
// restarts age out of the window.
func TestRestartsCountNewIncarnations(t *testing.T) {
	r := newRestarts()
	now := time.Now()
	r.note(1, 100, now)
	r.note(1, 100, now)
	if id, count := r.worst(now); count != 0 || id != 0 {
		t.Fatalf("no restart yet: %d %d", id, count)
	}
	r.note(1, 101, now.Add(time.Minute))
	r.note(1, 99, now.Add(2*time.Minute))
	r.note(2, 500, now.Add(2*time.Minute))
	r.note(2, 501, now.Add(3*time.Minute))
	r.note(1, 102, now.Add(4*time.Minute))
	if id, count := r.worst(now.Add(5 * time.Minute)); id != 1 || count != 2 {
		t.Fatalf("room 1 twice: %d %d", id, count)
	}
	if id, count := r.worst(now.Add(62 * time.Minute)); id != 1 || count != 1 {
		t.Fatalf("the first restart aged out: %d %d", id, count)
	}
	if _, count := r.worst(now.Add(2 * time.Hour)); count != 0 {
		t.Fatalf("all aged out: %d", count)
	}
}
