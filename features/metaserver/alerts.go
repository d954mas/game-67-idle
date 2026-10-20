package metaserver

import (
	"fmt"
	"sort"
	"strings"
	"sync"
	"time"
)

// Conditions the operator is paged for through the heartbeat's /fail, beside
// its own checks: each counts only once it held for alertHold, so a burst that
// passes within a couple of minutes pages nobody. The admin console shows every
// condition as soon as it holds.
const (
	alertHold = 3 * time.Minute
	// A lateness p99 in the bucket past two ticks (quantiles are bucket
	// edges: 65 ms reads "between 33 and 65 ms late, some worse").
	alertLateP99Us = 65_536
	// Abnormal closes (protocol, rate, slow: not a player leaving) in a
	// minute past the larger of these.
	alertClosesMin   = 5
	alertClosesShare = 0.02
	// Projected month traffic past this share of the plan's allowance,
	// once a whole day's cycle is counted.
	alertTrafficShare = 0.8
	alertTrafficHours = 24
)

type alertCondition struct {
	Kind string `json:"kind"`
	Text string `json:"text"`
}

type alertView struct {
	alertCondition
	Since  time.Time `json:"since"`  // the first heartbeat of this run that found it
	Firing bool      `json:"firing"` // held alertHold: the heartbeat reports it
}

// alertConditions reads the admin console's last minute and the traffic ledger.
// monthGB is the plan's monthly allowance (0: no traffic condition).
func alertConditions(now time.Time, perf *perfBoard, traffic *trafficLedger, monthGB float64) []alertCondition {
	var out []alertCondition
	if snap := perf.snapshot(now); snap.Fleet != nil {
		f := snap.Fleet
		if late := f.Stages["late"]; late.P99Us >= alertLateP99Us {
			out = append(out, alertCondition{"tick_late", fmt.Sprintf("tick lateness p99 past two ticks over the last minute (max %d ms)", late.MaxUs/1000)})
		}
		// One bad room is 1 % of a hundred rooms' ticks: the fleet p99 hides it.
		for _, room := range snap.Rooms {
			if late := room.Stages["late"]; late.P99Us >= alertLateP99Us {
				out = append(out, alertCondition{"room_late", fmt.Sprintf("room %d: tick lateness p99 past two ticks", room.ID)})
				break
			}
		}
		if f.DroppedTicks > 0 {
			out = append(out, alertCondition{"tick_dropped", fmt.Sprintf("rooms gave up %d ticks in the last minute", f.DroppedTicks)})
		}
		closes := f.Closes["protocol"] + f.Closes["rate"] + f.Closes["slow"]
		if float64(closes) > max(alertClosesMin, alertClosesShare*f.AvgSeated) {
			out = append(out, alertCondition{"disconnects", fmt.Sprintf("%d abnormal closes in the last minute (%.0f players)", closes, f.AvgSeated)})
		}
	}
	if monthGB > 0 {
		for _, h := range traffic.snapshot(now) {
			if projected := h.ProjectedRx + h.ProjectedTx; h.CountedHours >= alertTrafficHours && projected > alertTrafficShare*monthGB {
				out = append(out, alertCondition{"traffic:" + h.Host,
					fmt.Sprintf("%s: month traffic projected %.0f GB of the plan's %.0f GB", h.Host, projected, monthGB)})
			}
		}
	}
	return out
}

type alertTracker struct {
	mu     sync.Mutex
	since  map[string]time.Time
	latest []alertView
}

func newAlertTracker() *alertTracker { return &alertTracker{since: map[string]time.Time{}} }

// observe takes one heartbeat's conditions at `now`: a condition absent from
// a beat starts over. Returns what fires.
func (t *alertTracker) observe(now time.Time, conditions []alertCondition) []string {
	t.mu.Lock()
	defer t.mu.Unlock()
	next := map[string]time.Time{}
	t.latest = []alertView{}
	var firing []string
	for _, c := range conditions {
		since, held := t.since[c.Kind]
		if !held {
			since = now
		}
		next[c.Kind] = since
		v := alertView{alertCondition: c, Since: since, Firing: now.Sub(since) >= alertHold}
		t.latest = append(t.latest, v)
		if v.Firing {
			firing = append(firing, c.Text)
		}
	}
	t.since = next
	sort.Strings(firing)
	return firing
}

func (t *alertTracker) snapshot() []alertView {
	if t == nil {
		return []alertView{}
	}
	t.mu.Lock()
	defer t.mu.Unlock()
	return append([]alertView{}, t.latest...)
}

func firingReport(firing []string) string { return "alert: " + strings.Join(firing, "; ") }
