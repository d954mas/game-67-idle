package metaserver

import (
	"context"
	"log/slog"
	"net/http"
	"sort"
	"sync"
	"time"

	"github.com/jackc/pgx/v5/pgxpool"
)

// tally counts and times every endpoint between two flushes and prints one
// line per period, with the pool's state: how a load run sees where the
// metaserver waits. Room calls are not logged per request, so this is the
// only place their latency shows.
type tally struct {
	mu   sync.Mutex
	ends map[string]*endpointTally
	// The pool's counters only grow; a period reports their growth, and
	// the busiest moment it saw (acquired connections are sampled often,
	// since a flush-time reading is nearly always zero).
	emptyAcquires int64
	acquireWait   time.Duration
	acquiredMax   int32
}

type endpointTally struct {
	count   int
	maxMs   float64
	samples []float64 // capped; the percentile of a busy period is over its last samples
}

const tallySamples = 4096

func newTally() *tally { return &tally{ends: map[string]*endpointTally{}} }

func (t *tally) timed(name string, next http.HandlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		start := time.Now()
		next(w, r)
		t.add(name, float64(time.Since(start).Microseconds())/1000)
	}
}

func (t *tally) add(name string, ms float64) {
	t.mu.Lock()
	defer t.mu.Unlock()
	e := t.ends[name]
	if e == nil {
		e = &endpointTally{}
		t.ends[name] = e
	}
	e.count++
	if ms > e.maxMs {
		e.maxMs = ms
	}
	if len(e.samples) < tallySamples {
		e.samples = append(e.samples, ms)
	} else {
		e.samples[e.count%tallySamples] = ms
	}
}

// flush logs the period and starts the next one; nothing when nothing happened.
func (t *tally) flush(log *slog.Logger, pool *pgxpool.Pool) {
	t.mu.Lock()
	ends := t.ends
	t.ends = map[string]*endpointTally{}
	t.mu.Unlock()
	if len(ends) == 0 {
		return
	}
	attrs := []any{}
	for name, e := range ends {
		sort.Float64s(e.samples)
		p50 := e.samples[len(e.samples)/2]
		p95 := e.samples[len(e.samples)*95/100]
		attrs = append(attrs, slog.Group(name, "n", e.count, "p50_ms", p50, "p95_ms", p95, "max_ms", e.maxMs))
	}
	s := pool.Stat()
	t.mu.Lock()
	empty := s.EmptyAcquireCount() - t.emptyAcquires
	wait := s.AcquireDuration() - t.acquireWait
	peak := t.acquiredMax
	t.emptyAcquires, t.acquireWait, t.acquiredMax = s.EmptyAcquireCount(), s.AcquireDuration(), 0
	t.mu.Unlock()
	attrs = append(attrs, slog.Group("pool", "total", s.TotalConns(), "acquired_peak", peak, "max", s.MaxConns(),
		"empty_acquires", empty, "acquire_wait_ms", wait.Milliseconds()))
	log.Info("period", attrs...)
}

func (t *tally) sample(pool *pgxpool.Pool) {
	acquired := pool.Stat().AcquiredConns()
	t.mu.Lock()
	if acquired > t.acquiredMax {
		t.acquiredMax = acquired
	}
	t.mu.Unlock()
}

func (t *tally) run(ctx context.Context, log *slog.Logger, pool *pgxpool.Pool, every time.Duration) {
	ticker := time.NewTicker(every)
	sampler := time.NewTicker(every / 100)
	defer ticker.Stop()
	defer sampler.Stop()
	for {
		select {
		case <-ctx.Done():
			return
		case <-sampler.C:
			t.sample(pool)
		case <-ticker.C:
			t.flush(log, pool)
		}
	}
}
