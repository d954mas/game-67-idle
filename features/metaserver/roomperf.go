package metaserver

import (
	"encoding/json"
	"math"
	"sort"
	"sync"
	"sync/atomic"
	"time"
)

// A room's performance window as its status carries it
// (tests/fixtures/room_perf_v1.json, src/admission/perf.h): counters are the
// growth over `window_ms`, histograms log2 buckets of microseconds.
// Telemetry never costs routing: a window this server cannot read is
// dropped and counted, and the status is routed all the same.

const (
	perfBuckets = 18
	// Over a minute of a room's windows at the two-second status period,
	// with room for the extra statuses a seat change sends.
	perfRoomWindows = 64
	perfMinutes     = 24 * 60
	// A room that stopped reporting leaves the page after this.
	perfRoomForget = 10 * time.Minute
	perfRoomsMax   = 4096
	perfHostsMax   = 256
)

type perfHist struct {
	B   []uint64 `json:"b"`
	Max uint64   `json:"max"`
	Sum uint64   `json:"sum"`
}

type perfStages struct {
	Late      perfHist `json:"late"`
	Step      perfHist `json:"step"`
	Replicate perfHist `json:"replicate"`
	Meta      perfHist `json:"meta"`
	InputWait perfHist `json:"input_wait"`
}

type perfNet struct {
	RxMessages    uint64 `json:"rx_messages"`
	RxBytes       uint64 `json:"rx_bytes"`
	RxFrames      uint64 `json:"rx_frames"`
	RxPongs       uint64 `json:"rx_pongs"`
	TxMessages    uint64 `json:"tx_messages"`
	TxBytes       uint64 `json:"tx_bytes"`
	TxWrites      uint64 `json:"tx_writes"`
	TxPartial     uint64 `json:"tx_partial"`
	TxRefused     uint64 `json:"tx_refused"`
	Writable      uint64 `json:"writable"`
	Services      uint64 `json:"services"`
	ServicePasses uint64 `json:"service_passes"`
	QueuePeak     uint64 `json:"queue_peak"`
}

type perfRepl struct {
	Packets         uint64 `json:"packets"`
	Bytes           uint64 `json:"bytes"`
	RowBytes        uint64 `json:"row_bytes"`
	Reliable        uint64 `json:"reliable"`
	Cosmetic        uint64 `json:"cosmetic"`
	CosmeticDropped uint64 `json:"cosmetic_dropped"`
	RowsSent        uint64 `json:"rows_sent"`
	RowsDeferred    uint64 `json:"rows_deferred"`
	RowsWaitedMost  uint64 `json:"rows_waited_most"`
	Skipped         uint64 `json:"skipped"`
	Refused         uint64 `json:"refused"`
	OutboxPeak      uint64 `json:"outbox_peak"`
	CreditMin       int64  `json:"credit_min"`
	CreditMax       int64  `json:"credit_max"`
}

type perfInput struct {
	Malformed      uint64 `json:"malformed"`
	Underruns      uint64 `json:"underruns"`
	Overruns       uint64 `json:"overruns"`
	Trimmed        uint64 `json:"trimmed"`
	LostEdges      uint64 `json:"lost_edges"`
	MalformedEdges uint64 `json:"malformed_edges"`
	QueuePeak      uint64 `json:"queue_peak"`
}

type perfProcess struct {
	UserUs   uint64 `json:"user_us"`
	SystemUs uint64 `json:"system_us"`
	RssKb    uint64 `json:"rss_kb"`
}

// Jiffies by CPU state and the machine's interfaces, grown over the window.
type perfMachine struct {
	User      uint64 `json:"user"`
	Nice      uint64 `json:"nice"`
	System    uint64 `json:"system"`
	Idle      uint64 `json:"idle"`
	Iowait    uint64 `json:"iowait"`
	Irq       uint64 `json:"irq"`
	Softirq   uint64 `json:"softirq"`
	Steal     uint64 `json:"steal"`
	RxBytes   uint64 `json:"rx_bytes"`
	TxBytes   uint64 `json:"tx_bytes"`
	RxPackets uint64 `json:"rx_packets"`
	TxPackets uint64 `json:"tx_packets"`
	// The interfaces' byte counters since boot, not their growth (traffic.go).
	RxTotal uint64 `json:"rx_total"`
	TxTotal uint64 `json:"tx_total"`
}

// The clients' network reports a window heard (src/sync/report.h): round
// trips as a histogram of microseconds, the rest summed over the reports.
type perfClients struct {
	Reports     uint64   `json:"reports"`
	Rtt         perfHist `json:"rtt"`
	RttMaxMs    uint64   `json:"rtt_max_ms"`
	Corrections uint64   `json:"corrections"`
	Snaps       uint64   `json:"snaps"`
	Stalls      uint64   `json:"stalls"`
	FpsSum      uint64   `json:"fps_sum"`
	FpsLow      uint64   `json:"fps_low"`
	Reconnects  uint64   `json:"reconnects"`
	Web         uint64   `json:"web"`
	Touch       uint64   `json:"touch"`
	Bench       uint64   `json:"bench"`
}

// Seconds a client report covers.
const clientReportSeconds = 5

type roomPerf struct {
	WindowMs    uint64       `json:"window_ms"`
	Ticks       uint64       `json:"ticks"`
	Late        uint64       `json:"late"`
	Dropped     uint64       `json:"dropped"`
	Pumps       uint64       `json:"pumps"`
	SeatedTicks uint64       `json:"seated_ticks"`
	Stages      perfStages   `json:"stages"`
	Net         perfNet      `json:"net"`
	Repl        perfRepl     `json:"repl"`
	Input       perfInput    `json:"input"`
	Clients     *perfClients `json:"clients,omitempty"`
	Closes      []uint64     `json:"closes"`
	Process     *perfProcess `json:"process,omitempty"`
	Machine     *perfMachine `json:"machine,omitempty"`
	// The room's machine runs rooms RoomIDBase + 1 .. + Rooms (src/admission/perf.c).
	Plan *perfPlan `json:"plan,omitempty"`
}

type perfPlan struct {
	Rooms      uint32 `json:"rooms"`
	RoomIDBase uint32 `json:"room_id_base"`
}

// The most rooms a machine plans; the same bound as the room's flag.
const planRoomsMax = 4096

// planOf is the window's plan when it describes the room that sent it: a plan
// that leaves the room itself out is a misconfigured machine, and is not believed.
func planOf(p *roomPerf, roomID uint32) (perfPlan, bool) {
	if p == nil || p.Plan == nil || p.Plan.Rooms == 0 || p.Plan.Rooms > planRoomsMax {
		return perfPlan{}, false
	}
	plan := *p.Plan
	if uint64(roomID) <= uint64(plan.RoomIDBase) || uint64(roomID) > uint64(plan.RoomIDBase)+uint64(plan.Rooms) {
		return perfPlan{}, false
	}
	return plan, true
}

var closeNames = []string{"peer", "protocol", "rate", "slow", "app", "other"}

// decodeRoomPerf reads a window leniently: fields a newer room adds are
// ignored, a shape that cannot be summed is refused.
func decodeRoomPerf(raw json.RawMessage) (*roomPerf, bool) {
	var p roomPerf
	if err := json.Unmarshal(raw, &p); err != nil {
		return nil, false
	}
	if p.WindowMs == 0 || p.WindowMs > 3_600_000 || len(p.Closes) > len(closeNames) {
		return nil, false
	}
	for _, h := range p.hists() {
		if len(h.B) != perfBuckets {
			return nil, false
		}
	}
	if p.Clients != nil && len(p.Clients.Rtt.B) != perfBuckets {
		return nil, false
	}
	return &p, true
}

func (p *roomPerf) hists() []*perfHist {
	return []*perfHist{&p.Stages.Late, &p.Stages.Step, &p.Stages.Replicate, &p.Stages.Meta, &p.Stages.InputWait}
}

// perfSum adds windows up: counters sum, peaks and histograms merge.
type perfSum struct {
	Windows  int
	WindowMs uint64
	perf     roomPerf
	creditOK bool
	process  bool
}

func (s *perfSum) add(p *roomPerf) {
	a := &s.perf
	s.Windows++
	s.WindowMs += p.WindowMs
	a.Ticks += p.Ticks
	a.Late += p.Late
	a.Dropped += p.Dropped
	a.Pumps += p.Pumps
	a.SeatedTicks += p.SeatedTicks
	for i, h := range p.hists() {
		dst := a.hists()[i]
		if dst.B == nil {
			dst.B = make([]uint64, perfBuckets)
		}
		for b := range h.B {
			dst.B[b] += h.B[b]
		}
		dst.Max = max(dst.Max, h.Max)
		dst.Sum += h.Sum
	}
	n, m := &a.Net, &p.Net
	n.RxMessages += m.RxMessages
	n.RxBytes += m.RxBytes
	n.RxFrames += m.RxFrames
	n.RxPongs += m.RxPongs
	n.TxMessages += m.TxMessages
	n.TxBytes += m.TxBytes
	n.TxWrites += m.TxWrites
	n.TxPartial += m.TxPartial
	n.TxRefused += m.TxRefused
	n.Writable += m.Writable
	n.Services += m.Services
	n.ServicePasses += m.ServicePasses
	n.QueuePeak = max(n.QueuePeak, m.QueuePeak)
	r, q := &a.Repl, &p.Repl
	r.Packets += q.Packets
	r.Bytes += q.Bytes
	r.RowBytes += q.RowBytes
	r.Reliable += q.Reliable
	r.Cosmetic += q.Cosmetic
	r.CosmeticDropped += q.CosmeticDropped
	r.RowsSent += q.RowsSent
	r.RowsDeferred += q.RowsDeferred
	r.RowsWaitedMost = max(r.RowsWaitedMost, q.RowsWaitedMost)
	r.Skipped += q.Skipped
	r.Refused += q.Refused
	r.OutboxPeak = max(r.OutboxPeak, q.OutboxPeak)
	if q.Packets > 0 {
		if !s.creditOK {
			r.CreditMin, r.CreditMax, s.creditOK = q.CreditMin, q.CreditMax, true
		} else {
			r.CreditMin, r.CreditMax = min(r.CreditMin, q.CreditMin), max(r.CreditMax, q.CreditMax)
		}
	}
	i, j := &a.Input, &p.Input
	i.Malformed += j.Malformed
	i.Underruns += j.Underruns
	i.Overruns += j.Overruns
	i.Trimmed += j.Trimmed
	i.LostEdges += j.LostEdges
	i.MalformedEdges += j.MalformedEdges
	i.QueuePeak = max(i.QueuePeak, j.QueuePeak)
	if c := p.Clients; c != nil {
		if a.Clients == nil {
			a.Clients = &perfClients{Rtt: perfHist{B: make([]uint64, perfBuckets)}}
		}
		d := a.Clients
		d.Reports += c.Reports
		for b := range c.Rtt.B {
			d.Rtt.B[b] += c.Rtt.B[b]
		}
		d.Rtt.Max = max(d.Rtt.Max, c.Rtt.Max)
		d.Rtt.Sum += c.Rtt.Sum
		d.RttMaxMs = max(d.RttMaxMs, c.RttMaxMs)
		d.Corrections += c.Corrections
		d.Snaps += c.Snaps
		d.Stalls += c.Stalls
		d.FpsSum += c.FpsSum
		d.FpsLow += c.FpsLow
		d.Reconnects += c.Reconnects
		d.Web += c.Web
		d.Touch += c.Touch
		d.Bench += c.Bench
	}
	if a.Closes == nil {
		a.Closes = make([]uint64, len(closeNames))
	}
	for k, c := range p.Closes {
		a.Closes[k] += c
	}
	if p.Process != nil {
		if a.Process == nil {
			a.Process = &perfProcess{}
		}
		a.Process.UserUs += p.Process.UserUs
		a.Process.SystemUs += p.Process.SystemUs
		a.Process.RssKb = p.Process.RssKb
		s.process = true
	}
}

// The stage's quantiles as the upper bound of the bucket they fall in, in
// microseconds, never above the largest value seen.
type stageView struct {
	Count int64   `json:"count"`
	AvgUs float64 `json:"avg_us"`
	P50Us uint64  `json:"p50_us"`
	P95Us uint64  `json:"p95_us"`
	P99Us uint64  `json:"p99_us"`
	MaxUs uint64  `json:"max_us"`
}

func quantile(h *perfHist, total uint64, q float64) uint64 {
	if total == 0 {
		return 0
	}
	want := uint64(math.Ceil(q * float64(total)))
	var seen uint64
	for b, c := range h.B {
		seen += c
		if seen >= want {
			if b == perfBuckets-1 {
				return h.Max
			}
			return min(uint64(16)<<b, h.Max)
		}
	}
	return h.Max
}

func viewStage(h *perfHist) stageView {
	var total uint64
	for _, c := range h.B {
		total += c
	}
	v := stageView{Count: int64(total), MaxUs: h.Max}
	if total > 0 {
		v.AvgUs = float64(h.Sum) / float64(total)
		v.P50Us, v.P95Us, v.P99Us = quantile(h, total, .50), quantile(h, total, .95), quantile(h, total, .99)
	}
	return v
}

// perfView is what the page and a load run read: rates per second of the
// windows' own time, and the costs per player-second that size machines.
type perfView struct {
	Windows       int     `json:"windows"`
	WindowS       float64 `json:"window_s"`
	AvgSeated     float64 `json:"avg_seated"`
	PlayerSeconds float64 `json:"player_seconds"`
	TicksPerS     float64 `json:"ticks_per_s"`
	LateTicks     uint64  `json:"late_ticks"`
	DroppedTicks  uint64  `json:"dropped_ticks"`
	PumpsPerS     float64 `json:"pumps_per_s"`
	// The room process, where the platform reports it; null otherwise.
	CPUCorePct        *float64             `json:"cpu_core_pct"`
	UserCorePct       *float64             `json:"user_core_pct"`
	SystemCorePct     *float64             `json:"system_core_pct"`
	CPUMsPerPlayerS   *float64             `json:"cpu_ms_per_player_s"`
	TickWorkCorePct   float64              `json:"tick_work_core_pct"` // step and replication, wall time
	RssMB             *float64             `json:"rss_mb"`
	Stages            map[string]stageView `json:"stages"`
	RxMsgPerS         float64              `json:"rx_msg_per_s"`
	RxMsgPerPlayerS   float64              `json:"rx_msg_per_player_s"`
	RxFramesPerS      float64              `json:"rx_frames_per_s"`
	TxMsgPerS         float64              `json:"tx_msg_per_s"`
	TxWritesPerS      float64              `json:"tx_writes_per_s"`
	WritesPerMsg      float64              `json:"writes_per_msg"`
	RxKbitS           float64              `json:"rx_kbit_s"`
	TxKbitS           float64              `json:"tx_kbit_s"`
	TxKbitPerPlayer   float64              `json:"tx_kbit_s_per_player"`
	PongsPerS         float64              `json:"pongs_per_s"`
	WritablePerS      float64              `json:"writable_per_s"`
	ServicePassesPerS float64              `json:"service_passes_per_s"`
	TxPartial         uint64               `json:"tx_partial"`
	TxRefused         uint64               `json:"tx_refused"`
	QueuePeakBytes    uint64               `json:"queue_peak_bytes"`
	PacketsPerS       float64              `json:"packets_per_s"`
	BytesPerPacket    float64              `json:"bytes_per_packet"`
	RowBytesPct       float64              `json:"row_bytes_pct"`
	Reliable          uint64               `json:"reliable"`
	Cosmetic          uint64               `json:"cosmetic"`
	CosmeticDropped   uint64               `json:"cosmetic_dropped"`
	RowsDeferredPct   float64              `json:"rows_deferred_pct"`
	RowsWaitedMost    uint64               `json:"rows_waited_most"`
	Skipped           uint64               `json:"skipped"`
	Refused           uint64               `json:"refused"`
	OutboxPeak        uint64               `json:"outbox_peak"`
	CreditMin         int64                `json:"credit_min"`
	CreditMax         int64                `json:"credit_max"`
	Input             perfInput            `json:"input"`
	Closes            map[string]uint64    `json:"closes"`
	// How the network plays for the players, from their reports; nil
	// without any. Rates are per player-minute of reports.
	Clients *clientView `json:"clients"`
}

type clientView struct {
	Reports        uint64  `json:"reports"`
	RttP50Ms       float64 `json:"rtt_p50_ms"`
	RttP95Ms       float64 `json:"rtt_p95_ms"`
	RttP99Ms       float64 `json:"rtt_p99_ms"`
	RttMaxMs       uint64  `json:"rtt_max_ms"`
	CorrectionsMin float64 `json:"corrections_per_min"`
	SnapsMin       float64 `json:"snaps_per_min"`
	StallsMin      float64 `json:"stalls_per_min"`
	FpsAvg         float64 `json:"fps_avg"`
	LowFpsPct      float64 `json:"low_fps_pct"` // reports under 30 frames a second
	Reconnects     uint64  `json:"reconnects"`
	WebPct         float64 `json:"web_pct"`
	TouchPct       float64 `json:"touch_pct"`
	BenchPct       float64 `json:"bench_pct"`
}

func per(n, d float64) float64 {
	if d <= 0 {
		return 0
	}
	return n / d
}

func ptr(v float64) *float64 { return &v }

// view turns the sum into rates over `seconds` of wall clock: one room's
// windows add up to its own time, a fleet's overlap within the same minute.
// Player-seconds come from the ticks the players sat through, at the rate
// the rooms actually ticked.
func (s *perfSum) view(seconds float64) perfView {
	p := &s.perf
	v := perfView{Windows: s.Windows, WindowS: seconds, TicksPerS: per(float64(p.Ticks), seconds),
		LateTicks: p.Late, DroppedTicks: p.Dropped, PumpsPerS: per(float64(p.Pumps), seconds),
		Stages: map[string]stageView{}, Closes: map[string]uint64{}, Input: p.Input}
	v.PlayerSeconds = per(float64(p.SeatedTicks)*float64(s.WindowMs)/1000, float64(p.Ticks))
	v.AvgSeated = per(v.PlayerSeconds, seconds)
	names := []string{"late", "step", "replicate", "meta", "input_wait"}
	for i, h := range p.hists() {
		if h.B != nil {
			v.Stages[names[i]] = viewStage(h)
		}
	}
	v.TickWorkCorePct = 100 * per(float64(p.Stages.Step.Sum+p.Stages.Replicate.Sum)/1e6, seconds)
	if s.process && p.Process != nil {
		cpu := float64(p.Process.UserUs+p.Process.SystemUs) / 1e6
		v.CPUCorePct = ptr(100 * per(cpu, seconds))
		v.UserCorePct = ptr(100 * per(float64(p.Process.UserUs)/1e6, seconds))
		v.SystemCorePct = ptr(100 * per(float64(p.Process.SystemUs)/1e6, seconds))
		if v.PlayerSeconds > 0 {
			v.CPUMsPerPlayerS = ptr(1000 * cpu / v.PlayerSeconds)
		}
		v.RssMB = ptr(float64(p.Process.RssKb) / 1024)
	}
	n := &p.Net
	v.RxMsgPerS = per(float64(n.RxMessages), seconds)
	v.RxMsgPerPlayerS = per(float64(n.RxMessages), v.PlayerSeconds)
	v.RxFramesPerS = per(float64(n.RxFrames), seconds)
	v.TxMsgPerS = per(float64(n.TxMessages), seconds)
	v.TxWritesPerS = per(float64(n.TxWrites), seconds)
	v.WritesPerMsg = per(float64(n.TxWrites), float64(n.TxMessages))
	v.RxKbitS = per(float64(n.RxBytes)*8/1000, seconds)
	v.TxKbitS = per(float64(n.TxBytes)*8/1000, seconds)
	v.TxKbitPerPlayer = per(v.TxKbitS, v.AvgSeated)
	v.PongsPerS = per(float64(n.RxPongs), seconds)
	v.WritablePerS = per(float64(n.Writable), seconds)
	v.ServicePassesPerS = per(float64(n.ServicePasses), seconds)
	v.TxPartial, v.TxRefused, v.QueuePeakBytes = n.TxPartial, n.TxRefused, n.QueuePeak
	r := &p.Repl
	v.PacketsPerS = per(float64(r.Packets), seconds)
	v.BytesPerPacket = per(float64(r.Bytes), float64(r.Packets))
	v.RowBytesPct = 100 * per(float64(r.RowBytes), float64(r.Bytes))
	v.Reliable, v.Cosmetic, v.CosmeticDropped = r.Reliable, r.Cosmetic, r.CosmeticDropped
	v.RowsDeferredPct = 100 * per(float64(r.RowsDeferred), float64(r.RowsSent+r.RowsDeferred))
	v.RowsWaitedMost, v.Skipped, v.Refused, v.OutboxPeak = r.RowsWaitedMost, r.Skipped, r.Refused, r.OutboxPeak
	v.CreditMin, v.CreditMax = r.CreditMin, r.CreditMax
	for k, c := range p.Closes {
		v.Closes[closeNames[k]] = c
	}
	if c := p.Clients; c != nil && c.Reports > 0 {
		minutes := float64(c.Reports) * clientReportSeconds / 60
		rtt := viewStage(&c.Rtt)
		reports := float64(c.Reports)
		v.Clients = &clientView{Reports: c.Reports, RttP50Ms: float64(rtt.P50Us) / 1000, RttP95Ms: float64(rtt.P95Us) / 1000,
			RttP99Ms: float64(rtt.P99Us) / 1000, RttMaxMs: c.RttMaxMs, CorrectionsMin: per(float64(c.Corrections), minutes),
			SnapsMin: per(float64(c.Snaps), minutes), StallsMin: per(float64(c.Stalls), minutes), FpsAvg: per(float64(c.FpsSum), reports),
			LowFpsPct: 100 * per(float64(c.FpsLow), reports), Reconnects: c.Reconnects, WebPct: 100 * per(float64(c.Web), reports),
			TouchPct: 100 * per(float64(c.Touch), reports), BenchPct: 100 * per(float64(c.Bench), reports)}
	}
	return v
}

type perfSample struct {
	at     time.Time
	seated int32
	perf   *roomPerf
}

type roomPerfState struct {
	incarnation uint64
	host        string
	seated      int32
	capacity    int32
	at          time.Time
	windows     []perfSample // the newest perfRoomWindows, oldest first
}

type hostPerfState struct {
	at       time.Time
	windowMs uint64
	machine  perfMachine
	// Every window the machine's rooms sent, summed by the minute it arrived
	// in: overlapping windows weigh rates and shares by their length, so the
	// sums still give the minute's average (capacity.go reads the last whole one).
	minute, lastMinute int64
	minuteMs, lastMs   uint64
	minuteSum, lastSum perfMachine
}

func (m *perfMachine) add(o perfMachine) {
	m.User += o.User
	m.Nice += o.Nice
	m.System += o.System
	m.Idle += o.Idle
	m.Iowait += o.Iowait
	m.Irq += o.Irq
	m.Softirq += o.Softirq
	m.Steal += o.Steal
	m.RxBytes += o.RxBytes
	m.TxBytes += o.TxBytes
	m.RxPackets += o.RxPackets
	m.TxPackets += o.TxPackets
}

type perfMinute struct {
	minute int64
	sum    perfSum
}

type perfBoard struct {
	mu       sync.Mutex
	forgotAt time.Time
	rooms    map[uint32]*roomPerfState
	hosts    map[string]*hostPerfState
	minutes  [perfMinutes]perfMinute
	refused  atomic.Int64
}

func newPerfBoard() *perfBoard {
	return &perfBoard{rooms: map[uint32]*roomPerfState{}, hosts: map[string]*hostPerfState{}}
}

// record keeps a room's window; a new life of the room starts its history over.
func (b *perfBoard) record(now time.Time, roomID uint32, incarnation uint64, st *roomStatus, p *roomPerf) {
	b.mu.Lock()
	defer b.mu.Unlock()
	if now.Sub(b.forgotAt) > time.Minute {
		b.forgetLocked(now)
	}
	room := b.rooms[roomID]
	if room == nil || room.incarnation != incarnation {
		if room == nil && len(b.rooms) >= perfRoomsMax {
			return
		}
		room = &roomPerfState{incarnation: incarnation}
		b.rooms[roomID] = room
	}
	room.host, room.seated, room.capacity, room.at = st.Host, st.Seated, st.Capacity, now
	room.windows = append(room.windows, perfSample{at: now, seated: st.Seated, perf: p})
	if len(room.windows) > perfRoomWindows {
		room.windows = room.windows[len(room.windows)-perfRoomWindows:]
	}
	if p.Machine != nil {
		host := b.hosts[st.Host]
		if host == nil && len(b.hosts) < perfHostsMax {
			host = &hostPerfState{}
			b.hosts[st.Host] = host
		}
		if host != nil {
			host.at, host.windowMs, host.machine = now, p.WindowMs, *p.Machine
			if minute := now.Unix() / 60; host.minute != minute {
				host.lastMinute, host.lastMs, host.lastSum = host.minute, host.minuteMs, host.minuteSum
				host.minute, host.minuteMs, host.minuteSum = minute, 0, perfMachine{}
			}
			// A window after a reporting stall spans past minutes; it would outweigh this one.
			if p.WindowMs <= 60_000 {
				host.minuteMs += p.WindowMs
				host.minuteSum.add(*p.Machine)
			}
		}
	}
	minute := now.Unix() / 60
	m := &b.minutes[minute%perfMinutes]
	if m.minute != minute {
		*m = perfMinute{minute: minute}
	}
	m.sum.add(p)
}

func (b *perfBoard) forgetLocked(now time.Time) {
	b.forgotAt = now
	for id, room := range b.rooms {
		if now.Sub(room.at) > perfRoomForget {
			delete(b.rooms, id)
		}
	}
	for name, host := range b.hosts {
		if now.Sub(host.at) > perfRoomForget {
			delete(b.hosts, name)
		}
	}
}

type roomPerfView struct {
	ID       uint32  `json:"id"`
	Host     string  `json:"host"`
	Seated   int32   `json:"seated"`
	Capacity int32   `json:"capacity"`
	AgeS     float64 `json:"age_s"`
	perfView
}

type machineView struct {
	Host       string  `json:"host"`
	AgeS       float64 `json:"age_s"`
	Rooms      int     `json:"rooms"`
	Seated     int32   `json:"seated"`
	CPUPct     float64 `json:"cpu_pct"` // of every core, busy of all but idle and iowait
	UserPct    float64 `json:"user_pct"`
	SystemPct  float64 `json:"system_pct"`
	SoftirqPct float64 `json:"softirq_pct"`
	IrqPct     float64 `json:"irq_pct"`
	StealPct   float64 `json:"steal_pct"`
	IowaitPct  float64 `json:"iowait_pct"`
	RxPps      float64 `json:"rx_pps"`
	TxPps      float64 `json:"tx_pps"`
	RxMbitS    float64 `json:"rx_mbit_s"`
	TxMbitS    float64 `json:"tx_mbit_s"`
}

type minuteView struct {
	At              time.Time `json:"at"`
	Windows         int       `json:"windows"`
	AvgSeated       float64   `json:"avg_seated"` // the fleet's players, averaged over the minute
	CPUMsPerPlayerS *float64  `json:"cpu_ms_per_player_s"`
	StepP99Us       uint64    `json:"step_p99_us"`
	ReplicateP99Us  uint64    `json:"replicate_p99_us"`
	LateP99Us       uint64    `json:"late_p99_us"`
	LateTicks       uint64    `json:"late_ticks"`
	DroppedTicks    uint64    `json:"dropped_ticks"`
	RxMsgPerS       float64   `json:"rx_msg_per_s"`
	TxWritesPerS    float64   `json:"tx_writes_per_s"`
	TxMbitS         float64   `json:"tx_mbit_s"`
}

type perfSnapshot struct {
	Refused  int64          `json:"refused"` // windows this server could not read
	Fleet    *perfView      `json:"fleet"`   // every room's last minute together, averaged over the minute
	Rooms    []roomPerfView `json:"rooms"`
	Machines []machineView  `json:"machines"`
	Minutes  []minuteView   `json:"minutes"` // whole minutes, newest last
}

func (b *perfBoard) snapshot(now time.Time) perfSnapshot {
	if b == nil {
		return perfSnapshot{Rooms: []roomPerfView{}, Machines: []machineView{}, Minutes: []minuteView{}}
	}
	b.mu.Lock()
	defer b.mu.Unlock()
	b.forgetLocked(now)
	out := perfSnapshot{Refused: b.refused.Load(), Rooms: []roomPerfView{}, Machines: []machineView{}, Minutes: []minuteView{}}
	var fleet perfSum
	var fleetRssKb uint64
	perHost := map[string][2]int32{}
	for id, room := range b.rooms {
		var sum perfSum
		for _, w := range room.windows {
			if now.Sub(w.at) <= time.Minute {
				sum.add(w.perf)
				fleet.add(w.perf)
			}
		}
		if sum.Windows == 0 {
			continue
		}
		if sum.perf.Process != nil {
			fleetRssKb += sum.perf.Process.RssKb
		}
		out.Rooms = append(out.Rooms, roomPerfView{ID: id, Host: room.host, Seated: room.seated, Capacity: room.capacity,
			AgeS: now.Sub(room.at).Seconds(), perfView: sum.view(float64(sum.WindowMs) / 1000)})
		h := perHost[room.host]
		perHost[room.host] = [2]int32{h[0] + 1, h[1] + room.seated}
	}
	sort.Slice(out.Rooms, func(i, j int) bool { return out.Rooms[i].ID < out.Rooms[j].ID })
	if fleet.Windows > 0 {
		// The fleet's minute: a room that started within it adds only its part.
		v := fleet.view(60)
		if v.RssMB != nil {
			v.RssMB = ptr(float64(fleetRssKb) / 1024)
		}
		out.Fleet = &v
	}
	for name, host := range b.hosts {
		counts := perHost[name]
		v := machineFigures(name, host.machine, host.windowMs)
		v.AgeS, v.Rooms, v.Seated = now.Sub(host.at).Seconds(), int(counts[0]), counts[1]
		out.Machines = append(out.Machines, v)
	}
	sort.Slice(out.Machines, func(i, j int) bool { return out.Machines[i].Host < out.Machines[j].Host })
	current := now.Unix() / 60
	for back := int64(perfMinutes - 1); back >= 1; back-- {
		m := &b.minutes[(current-back)%perfMinutes]
		if m.minute != current-back || m.sum.Windows == 0 {
			continue
		}
		out.Minutes = append(out.Minutes, m.view())
	}
	return out
}

func machineFigures(host string, m perfMachine, windowMs uint64) machineView {
	total := float64(m.User + m.Nice + m.System + m.Idle + m.Iowait + m.Irq + m.Softirq + m.Steal)
	pct := func(x uint64) float64 { return 100 * per(float64(x), total) }
	seconds := float64(windowMs) / 1000
	return machineView{Host: host,
		CPUPct: pct(m.User + m.Nice + m.System + m.Irq + m.Softirq + m.Steal), UserPct: pct(m.User + m.Nice), SystemPct: pct(m.System),
		SoftirqPct: pct(m.Softirq), IrqPct: pct(m.Irq), StealPct: pct(m.Steal), IowaitPct: pct(m.Iowait),
		RxPps: per(float64(m.RxPackets), seconds), TxPps: per(float64(m.TxPackets), seconds),
		RxMbitS: per(float64(m.RxBytes)*8/1e6, seconds), TxMbitS: per(float64(m.TxBytes)*8/1e6, seconds)}
}

func (m *perfMinute) view() minuteView {
	v := m.sum.view(60)
	mv := minuteView{At: time.Unix(m.minute*60, 0).UTC(), Windows: m.sum.Windows, AvgSeated: v.AvgSeated, CPUMsPerPlayerS: v.CPUMsPerPlayerS,
		LateTicks: v.LateTicks, DroppedTicks: v.DroppedTicks, RxMsgPerS: v.RxMsgPerS, TxWritesPerS: v.TxWritesPerS, TxMbitS: v.TxKbitS / 1000}
	mv.StepP99Us, mv.ReplicateP99Us, mv.LateP99Us = v.Stages["step"].P99Us, v.Stages["replicate"].P99Us, v.Stages["late"].P99Us
	return mv
}

// minuteHistory is one whole minute (Unix minutes) for the history sampler:
// the fleet's figures, nil without windows, and every machine heard in it,
// averaged over the windows that arrived within it.
func (b *perfBoard) minuteHistory(minute int64) (*minuteView, []machineView) {
	if b == nil {
		return nil, nil
	}
	b.mu.Lock()
	defer b.mu.Unlock()
	var fleet *minuteView
	if m := &b.minutes[minute%perfMinutes]; m.minute == minute && m.sum.Windows > 0 {
		v := m.view()
		fleet = &v
	}
	machines := []machineView{}
	for name, host := range b.hosts {
		switch {
		case host.minute == minute && host.minuteMs > 0:
			machines = append(machines, machineFigures(name, host.minuteSum, host.minuteMs))
		case host.lastMinute == minute && host.lastMs > 0:
			machines = append(machines, machineFigures(name, host.lastSum, host.lastMs))
		}
	}
	sort.Slice(machines, func(i, j int) bool { return machines[i].Host < machines[j].Host })
	return fleet, machines
}
