package metaserver

import (
	"sort"
	"sync"
	"time"
)

// A room machine's network traffic by UTC day, from the cumulative interface
// counters its rooms report with their perf windows (roomperf.go). Every room
// of a machine reports the same counters at its own moment, so a reading
// behind the newest is late, not new; a large fall is the machine's reboot.
// Kept in the history checkpoint (observability_archive.go), so a month's
// total survives metaserver restarts; the traffic of a reboot's last window
// before it is lost.

const (
	trafficDaysKept = 62
	trafficHostsMax = 64
	// A reading this far below the last is a reboot's counters starting over,
	// not a late report from another room of the machine.
	trafficResetBytes = 1 << 30
)

type trafficDay struct {
	RxBytes uint64 `json:"rx_bytes"`
	TxBytes uint64 `json:"tx_bytes"`
}

type trafficHostState struct {
	LastRx uint64                 `json:"last_rx"`
	LastTx uint64                 `json:"last_tx"`
	Seen   time.Time              `json:"seen"`
	Start  time.Time              `json:"start"` // the first reading: counting starts there
	Days   map[string]*trafficDay `json:"days"`  // "2006-01-02", UTC
}

type trafficLedger struct {
	mu    sync.Mutex
	hosts map[string]*trafficHostState
}

func newTrafficLedger() *trafficLedger {
	return &trafficLedger{hosts: map[string]*trafficHostState{}}
}

func trafficDayKey(t time.Time) string { return t.UTC().Format("2006-01-02") }

// note takes a machine's cumulative counters as one of its rooms read them.
func (l *trafficLedger) note(now time.Time, host string, rx, tx uint64) {
	if l == nil || (rx == 0 && tx == 0) {
		return
	}
	l.mu.Lock()
	defer l.mu.Unlock()
	h := l.hosts[host]
	if h == nil {
		if len(l.hosts) >= trafficHostsMax {
			return
		}
		h = &trafficHostState{Days: map[string]*trafficDay{}}
		l.hosts[host] = h
	}
	first := h.Seen.IsZero()
	reset := !first && (rx+trafficResetBytes < h.LastRx || tx+trafficResetBytes < h.LastTx)
	late := !first && !reset && (rx < h.LastRx || tx < h.LastTx)
	if late {
		return
	}
	if !first && !reset {
		day := h.Days[trafficDayKey(now)]
		if day == nil {
			day = &trafficDay{}
			h.Days[trafficDayKey(now)] = day
		}
		day.RxBytes += rx - h.LastRx
		day.TxBytes += tx - h.LastTx
	}
	if first {
		h.Start = now
	}
	h.LastRx, h.LastTx, h.Seen = rx, tx, now
	oldest := trafficDayKey(now.AddDate(0, 0, -trafficDaysKept))
	for key := range h.Days {
		if key < oldest {
			delete(h.Days, key)
		}
	}
}

type trafficDayView struct {
	Day  string  `json:"day"`
	RxGB float64 `json:"rx_gb"`
	TxGB float64 `json:"tx_gb"`
}

type trafficHostView struct {
	Host    string  `json:"host"`
	AgeS    float64 `json:"age_s"` // since the newest reading
	TodayRx float64 `json:"today_rx_gb"`
	TodayTx float64 `json:"today_tx_gb"`
	MonthRx float64 `json:"month_rx_gb"`
	MonthTx float64 `json:"month_tx_gb"`
	// The month's total at the pace since the month began, or since the
	// first reading when counting started within it.
	ProjectedRx  float64          `json:"projected_month_rx_gb"`
	CountedHours float64          `json:"counted_hours"` // the projection's base: this month since counting began
	ProjectedTx  float64          `json:"projected_month_tx_gb"`
	Days         []trafficDayView `json:"days"` // this month, oldest first
}

const gb = 1e9

func (l *trafficLedger) snapshot(now time.Time) []trafficHostView {
	out := []trafficHostView{}
	if l == nil {
		return out
	}
	l.mu.Lock()
	defer l.mu.Unlock()
	now = now.UTC()
	monthStart := time.Date(now.Year(), now.Month(), 1, 0, 0, 0, 0, time.UTC)
	monthEnd := monthStart.AddDate(0, 1, 0)
	for name, h := range l.hosts {
		v := trafficHostView{Host: name, AgeS: now.Sub(h.Seen).Seconds(), Days: []trafficDayView{}}
		for key, day := range h.Days {
			if key < trafficDayKey(monthStart) {
				continue
			}
			v.MonthRx += float64(day.RxBytes) / gb
			v.MonthTx += float64(day.TxBytes) / gb
			v.Days = append(v.Days, trafficDayView{Day: key, RxGB: float64(day.RxBytes) / gb, TxGB: float64(day.TxBytes) / gb})
		}
		if today := h.Days[trafficDayKey(now)]; today != nil {
			v.TodayRx, v.TodayTx = float64(today.RxBytes)/gb, float64(today.TxBytes)/gb
		}
		sort.Slice(v.Days, func(i, j int) bool { return v.Days[i].Day < v.Days[j].Day })
		since := monthStart
		if h.Start.After(since) {
			since = h.Start
		}
		counted := now.Sub(since).Hours()
		v.CountedHours = counted
		if counted > 1 {
			scale := monthEnd.Sub(since).Hours() / counted
			v.ProjectedRx = v.MonthRx * scale
			v.ProjectedTx = v.MonthTx * scale
		}
		out = append(out, v)
	}
	sort.Slice(out, func(i, j int) bool { return out[i].Host < out[j].Host })
	return out
}

func (l *trafficLedger) checkpoint() map[string]trafficHostState {
	if l == nil {
		return nil
	}
	l.mu.Lock()
	defer l.mu.Unlock()
	out := make(map[string]trafficHostState, len(l.hosts))
	for name, h := range l.hosts {
		days := make(map[string]*trafficDay, len(h.Days))
		for key, day := range h.Days {
			copied := *day
			days[key] = &copied
		}
		out[name] = trafficHostState{LastRx: h.LastRx, LastTx: h.LastTx, Seen: h.Seen, Start: h.Start, Days: days}
	}
	return out
}

func (l *trafficLedger) restore(state map[string]trafficHostState) {
	if l == nil || len(state) > trafficHostsMax {
		return
	}
	l.mu.Lock()
	defer l.mu.Unlock()
	l.hosts = map[string]*trafficHostState{}
	for name, h := range state {
		if len(h.Days) > trafficDaysKept+1 {
			continue
		}
		copied := h
		if copied.Days == nil {
			copied.Days = map[string]*trafficDay{}
		}
		l.hosts[name] = &copied
	}
}
