package metaserver

import (
	"sync"
	"time"
)

// A room's life is its incarnation, and a status under a newer one is
// the room back from a crash or from its own watchdog. The registry's
// count of alive rooms never shows a room that keeps coming back, so
// the restarts of the last hour are kept here for the heartbeat: one
// per room is a release, three is a room in trouble.
const (
	restartWindow = time.Hour
	restartAlarm  = 3
)

type restarts struct {
	mu      sync.Mutex
	current map[uint32]uint64
	recent  map[uint32][]time.Time
}

func newRestarts() *restarts {
	return &restarts{current: map[uint32]uint64{}, recent: map[uint32][]time.Time{}}
}

// note is every accepted status: a first sighting is not a restart, an
// incarnation already known is the same life.
func (r *restarts) note(id uint32, incarnation uint64, now time.Time) {
	r.mu.Lock()
	defer r.mu.Unlock()
	previous, known := r.current[id]
	if known && incarnation <= previous {
		return
	}
	r.current[id] = incarnation
	if !known {
		return
	}
	r.recent[id] = append(prune(r.recent[id], now.Add(-restartWindow)), now)
}

// worst is the room restarted most within the window and how often;
// 0 when no room restarted.
func (r *restarts) worst(now time.Time) (id uint32, count int) {
	r.mu.Lock()
	defer r.mu.Unlock()
	since := now.Add(-restartWindow)
	for room, times := range r.recent {
		times = prune(times, since)
		if len(times) == 0 {
			delete(r.recent, room)
			continue
		}
		r.recent[room] = times
		// A tie goes to the lower id, so the answer does not follow map order.
		if len(times) > count || (len(times) == count && room < id) {
			id, count = room, len(times)
		}
	}
	return id, count
}

func prune(times []time.Time, since time.Time) []time.Time {
	for len(times) > 0 && !times[0].After(since) {
		times = times[1:]
	}
	return times
}
