package metaserver

import (
	"context"
	"fmt"
	"log/slog"
	"net/http"
	"strings"
	"time"
)

// heartbeat tells an outside watcher (healthchecks.io: a check with a
// period and a grace, alerting when the pings stop) that the stand can
// seat players: the database answers, a room is alive, the rooms'
// certificate will still be good in two weeks, and a walk down the
// player's path (entry, the room's socket, WELCOME) comes back. Anything
// less is reported to the check's /fail address at once, with the reason,
// so the alert does not wait for the grace to run out.
type heartbeat struct {
	url     string
	every   time.Duration
	store   *store
	certs   *certWatch // nil behind a proxy
	probe   *probe     // nil = no walk
	lives   *restarts  // nil = restarts not watched
	rooms   *rooms     // nil = no performance or traffic alerts
	monthGB float64
	log     *slog.Logger
	client  *http.Client
	down    bool // the ping itself failed; logged once per outage
	failing bool // the check failed; logged once per outage, with the reason
}

func newHeartbeat(cfg Config, st *store, certs *certWatch, walker *probe, lives *restarts, watched *rooms, log *slog.Logger) *heartbeat {
	return &heartbeat{url: cfg.HeartbeatURL, every: cfg.HeartbeatEvery, store: st, certs: certs, probe: walker,
		lives: lives, rooms: watched, monthGB: cfg.TrafficMonthGB, log: log, client: &http.Client{Timeout: 10 * time.Second}}
}

// The rooms report every couple of seconds and every one of them met a closed door
// while this service restarted: the first beat waits for their statuses
// to land, or every restart would page as "no room alive".
const firstBeatAfter = 10 * time.Second

func (h *heartbeat) run(ctx context.Context) {
	first := time.NewTimer(min(firstBeatAfter, h.every))
	defer first.Stop()
	select {
	case <-ctx.Done():
		return
	case <-first.C:
		h.beat(ctx)
	}
	ticker := time.NewTicker(h.every)
	defer ticker.Stop()
	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			h.beat(ctx)
		}
	}
}

// beat checks and pings once; the outcome is what went over the wire.
func (h *heartbeat) beat(ctx context.Context) (ok bool, report string) {
	ctx, cancel := context.WithTimeout(ctx, h.every/2)
	defer cancel()
	ok, report = h.check(ctx)
	switch {
	case !ok && !h.failing:
		h.log.Warn("stand unhealthy", "reason", report)
		h.failing = true
	case ok && h.failing:
		h.log.Info("stand healthy again", "report", report)
		h.failing = false
	}
	target := h.url
	if !ok {
		target += "/fail"
	}
	req, err := http.NewRequestWithContext(ctx, http.MethodPost, target, strings.NewReader(report))
	if err != nil {
		h.log.Error("heartbeat url", "err", err)
		return ok, report
	}
	req.Header.Set("Content-Type", "text/plain")
	res, err := h.client.Do(req)
	if err == nil {
		res.Body.Close()
		if res.StatusCode >= 300 {
			err = fmt.Errorf("status %d", res.StatusCode)
		}
	}
	switch {
	case err != nil && !h.down:
		h.log.Warn("heartbeat not delivered", "err", err)
		h.down = true
	case err == nil && h.down:
		h.log.Info("heartbeat delivered again")
		h.down = false
	}
	return ok, report
}

// check is the stand as a player would find it: the database within a
// ping, and a room the router would send them to.
func (h *heartbeat) check(ctx context.Context) (bool, string) {
	// Every beat observes the rooms' conditions, so a failing check neither
	// freezes their clocks nor lets a stale one fire on the next pass.
	var firing []string
	if h.rooms != nil {
		now := time.Now()
		firing = h.rooms.alerts.observe(now, alertConditions(now, h.rooms.perf, h.rooms.traffic, h.monthGB))
	}
	start := time.Now()
	if err := h.store.pool.Ping(ctx); err != nil {
		return false, "database: " + err.Error()
	}
	dbMs := time.Since(start).Milliseconds()
	var alive, seated, recent int64
	err := h.store.pool.QueryRow(ctx,
		`SELECT count(*) FILTER (WHERE `+roomAlive+`), coalesce(sum(seated) FILTER (WHERE `+roomAlive+`), 0),
		     count(*) FILTER (WHERE seen_at > now() - interval '10 minutes')
		 FROM rooms`).Scan(&alive, &seated, &recent)
	if err != nil {
		return false, "registry: " + err.Error()
	}
	if alive == 0 {
		return false, fmt.Sprintf("no room alive (db %d ms)", dbMs)
	}
	// Rooms that reported within the last minutes and stopped are parked
	// or dead: a fence that spared a few rooms would otherwise pass.
	if alive*2 < recent {
		return false, fmt.Sprintf("only %d of %d recent rooms alive (db %d ms)", alive, recent, dbMs)
	}
	if reason, bad := h.certs.alarm(time.Now()); bad {
		return false, reason
	}
	// A room that keeps coming back is a crash loop or a hang its watchdog
	// keeps ending; the count of alive rooms never shows it.
	if h.lives != nil {
		if room, count := h.lives.worst(time.Now()); count >= restartAlarm {
			return false, fmt.Sprintf("room %d restarted %d times in the last hour", room, count)
		}
	}
	walked := ""
	if h.probe != nil {
		report, err := h.probe.run(ctx)
		if err != nil {
			return false, "probe: " + err.Error()
		}
		walked = " " + report
	}
	// After the walk, so a condition that holds for days never hides an outage.
	if len(firing) > 0 {
		return false, firingReport(firing)
	}
	return true, fmt.Sprintf("rooms alive=%d seated=%d db_ms=%d%s", alive, seated, dbMs, walked)
}
