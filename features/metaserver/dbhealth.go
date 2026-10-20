package metaserver

import (
	"context"
	"sync"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"
)

// dbHealth is the pool's tracer: every query and every acquire passes
// through it, so the last day of the database as this process saw it
// (count, errors, time, the latest error) is known without a line in the
// store. Acquires count only when they fail: a wait is the pool's queue,
// which the period line reports, not the database answering slowly.
type dbHealth struct {
	mu      sync.Mutex
	minutes [observationMinutes]dbMinute
	last    *dbError
}

type dbMinute struct {
	Minute  int64 `json:"minute"` // unix minute; a bucket outside retention is empty
	Queries int   `json:"queries"`
	Errors  int   `json:"errors"`
	total   time.Duration
	MaxMs   float64 `json:"max_ms"`
}

type dbError struct {
	At   time.Time `json:"at"`
	Text string    `json:"text"`
}

type dbHealthSummary struct {
	Queries int        `json:"queries"`
	Errors  int        `json:"errors"`
	AvgMs   float64    `json:"avg_ms"`
	MaxMs   float64    `json:"max_ms"`
	Minutes []dbMinute `json:"minutes"` // the retained window, oldest first
	Last    *dbError   `json:"last_error"`
}

type dbMinuteState struct {
	Minute  int64         `json:"minute"`
	Queries int           `json:"queries"`
	Errors  int           `json:"errors"`
	TotalNs time.Duration `json:"total_ns"`
	MaxMs   float64       `json:"max_ms"`
}

type dbHealthState struct {
	Minutes [observationMinutes]dbMinuteState `json:"minutes"`
	Last    *dbError                          `json:"last_error"`
}

func (h *dbHealth) checkpoint() dbHealthState {
	h.mu.Lock()
	defer h.mu.Unlock()
	now := time.Now()
	minute := now.Unix() / 60
	var out dbHealthState
	for i, m := range h.minutes {
		if m.Minute <= minute-observationMinutes || m.Minute > minute {
			continue
		}
		out.Minutes[i] = dbMinuteState{m.Minute, m.Queries, m.Errors, m.total, m.MaxMs}
	}
	if h.last != nil && h.last.At.After(now.Add(-observationRetention)) && !h.last.At.After(now) {
		last := *h.last
		out.Last = &last
	}
	return out
}

func (h *dbHealth) restore(state dbHealthState) {
	h.mu.Lock()
	defer h.mu.Unlock()
	for i, m := range state.Minutes {
		h.minutes[i] = dbMinute{m.Minute, m.Queries, m.Errors, m.TotalNs, m.MaxMs}
	}
	h.last = nil
	if state.Last != nil {
		last := *state.Last
		h.last = &last
	}
}

type dbTraceKey struct{}

// adminQueries marks a context whose queries are the admin console's own:
// they must not appear in the numbers the page shows.
type adminQueriesKey struct{}

func adminQueries(ctx context.Context) context.Context {
	return context.WithValue(ctx, adminQueriesKey{}, true)
}

func (h *dbHealth) TraceQueryStart(ctx context.Context, _ *pgx.Conn, _ pgx.TraceQueryStartData) context.Context {
	return context.WithValue(ctx, dbTraceKey{}, time.Now())
}

func (h *dbHealth) TraceQueryEnd(ctx context.Context, _ *pgx.Conn, data pgx.TraceQueryEndData) {
	start, ok := ctx.Value(dbTraceKey{}).(time.Time)
	if !ok || ctx.Value(adminQueriesKey{}) != nil {
		return
	}
	h.record(time.Since(start), data.Err)
}

func (h *dbHealth) TraceAcquireStart(ctx context.Context, _ *pgxpool.Pool, _ pgxpool.TraceAcquireStartData) context.Context {
	return ctx
}

func (h *dbHealth) TraceAcquireEnd(ctx context.Context, _ *pgxpool.Pool, data pgxpool.TraceAcquireEndData) {
	if data.Err == nil || ctx.Value(adminQueriesKey{}) != nil {
		return
	}
	h.note(0, data.Err, false)
}

func (h *dbHealth) record(took time.Duration, err error) {
	h.note(took, err, true)
}

// note counts an error, and the query it came from when there was one:
// a failed acquire never reached the database and must not dilute its
// average.
func (h *dbHealth) note(took time.Duration, err error, query bool) {
	now := time.Now()
	h.mu.Lock()
	defer h.mu.Unlock()
	m := h.bucket(now)
	if query {
		m.Queries++
		m.total += took
		if ms := float64(took.Microseconds()) / 1000; ms > m.MaxMs {
			m.MaxMs = ms
		}
	}
	if err != nil {
		m.Errors++
		h.last = &dbError{At: now, Text: err.Error()}
	}
}

// bucket returns this minute's slot, cleared when its prior data has expired.
func (h *dbHealth) bucket(now time.Time) *dbMinute {
	minute := now.Unix() / 60
	m := &h.minutes[minute%int64(len(h.minutes))]
	if m.Minute != minute {
		*m = dbMinute{Minute: minute}
	}
	return m
}

func (h *dbHealth) summary(now time.Time) dbHealthSummary {
	h.mu.Lock()
	defer h.mu.Unlock()
	minute := now.Unix() / 60
	out := dbHealthSummary{Minutes: make([]dbMinute, 0, len(h.minutes))}
	if h.last != nil && h.last.At.After(now.Add(-observationRetention)) && !h.last.At.After(now) {
		last := *h.last
		out.Last = &last
	}
	var total time.Duration
	for back := int64(len(h.minutes) - 1); back >= 0; back-- {
		at := minute - back
		m := h.minutes[at%int64(len(h.minutes))]
		if m.Minute != at {
			m = dbMinute{Minute: at}
		}
		out.Minutes = append(out.Minutes, m)
		out.Queries += m.Queries
		out.Errors += m.Errors
		total += m.total
		if m.MaxMs > out.MaxMs {
			out.MaxMs = m.MaxMs
		}
	}
	if out.Queries > 0 {
		out.AvgMs = float64(total.Microseconds()) / 1000 / float64(out.Queries)
	}
	return out
}
