package metaserver

import (
	"context"
	"crypto/subtle"
	"log/slog"
	"net"
	"net/http"
	"net/netip"
	"runtime"
	"strconv"
	"strings"
	"sync"
	"time"
)

// admin serves the operator console (cmd/admin) its internal views of the
// stand: the registry, the counters, observations, incidents and room
// performance. It listens on its own address for peers inside the admin
// networks only, judged by the socket, never by a header: X-Forwarded-For is
// the player API's business and is spoofable from inside the stand's network.
type admin struct {
	cfg          Config
	store        *store
	rooms        *rooms
	certs        *certWatch // nil behind a proxy
	log          *slog.Logger
	roomURL      func(room roomAddress) string
	started      time.Time
	observations *observations
	// The player counts are four table scans; the page polls every
	// second, so they are read every few seconds and served from here.
	countsMu sync.Mutex
	counts   playerCounts
	countsAt time.Time
}

const countsEvery = 5 * time.Second

func newAdmin(a *Server, certs *certWatch) *admin {
	return &admin{cfg: a.cfg, store: a.store, rooms: a.rooms, certs: certs, log: a.log, roomURL: a.roomURL,
		started: a.now(), observations: a.observations}
}

func (ad *admin) handler() http.Handler {
	mux := http.NewServeMux()
	if ad.cfg.AdminServiceToken != "" {
		mux.HandleFunc("GET /internal/admin/v1/snapshot", ad.internalGuard(ad.snapshot))
		mux.HandleFunc("GET /internal/admin/v1/metrics", ad.internalGuard(ad.metrics))
		mux.HandleFunc("GET /internal/admin/v1/incidents", ad.internalGuard(ad.incidents))
		mux.HandleFunc("GET /internal/admin/v1/perf", ad.internalGuard(ad.perfSummary))
		mux.HandleFunc("GET /internal/admin/v1/perf/rooms/{id}", ad.internalGuard(ad.perfRoom))
	}
	return mux
}

// internalGuard admits a socket peer inside the admin networks whose request
// names this host by address, as localhost or by a configured name (a page
// elsewhere that rebinds its name to the loopback still arrives with its own
// Host), carrying the admin service's token.
func (ad *admin) internalGuard(next http.HandlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		peer, err := netip.ParseAddrPort(r.RemoteAddr)
		if err != nil || !ad.allowed(peer.Addr()) {
			http.Error(w, "admin is for the stand's own networks", http.StatusForbidden)
			return
		}
		if !ad.hostIsOurs(r.Host) {
			http.Error(w, "admin answers by address only", http.StatusMisdirectedRequest)
			return
		}
		credential, ok := strings.CutPrefix(r.Header.Get("Authorization"), "Bearer ")
		if !ok || subtle.ConstantTimeCompare([]byte(credential), []byte(ad.cfg.AdminServiceToken)) != 1 {
			http.Error(w, "service credential required", http.StatusUnauthorized)
			return
		}
		w.Header().Set("Cache-Control", "no-store")
		w.Header().Set("X-Content-Type-Options", "nosniff")
		next(w, r)
	}
}

func (ad *admin) allowed(peer netip.Addr) bool {
	peer = peer.Unmap()
	for _, prefix := range ad.cfg.AdminNets {
		if prefix.Contains(peer) {
			return true
		}
	}
	return false
}

func (ad *admin) hostIsOurs(host string) bool {
	if name, _, err := net.SplitHostPort(host); err == nil {
		host = name
	}
	host = strings.ToLower(trimBrackets(host))
	if host == "localhost" {
		return true
	}
	for _, ours := range ad.cfg.AdminHosts {
		if host == ours {
			return true
		}
	}
	_, err := netip.ParseAddr(host)
	return err == nil
}

func trimBrackets(host string) string {
	if len(host) >= 2 && host[0] == '[' && host[len(host)-1] == ']' {
		return host[1 : len(host)-1]
	}
	return host
}

type roomsAPICounters struct {
	TicketsRefused  int64 `json:"tickets_refused"`
	Statuses        int64 `json:"statuses"`
	StatusesRefused int64 `json:"statuses_refused"`
	Loads           int64 `json:"loads"`
	LoadsRefused    int64 `json:"loads_refused"`
	LoadsFailed     int64 `json:"loads_failed"`
	Saves           int64 `json:"saves"`
	SaveBatches     int64 `json:"save_batches"`
	SavesRefused    int64 `json:"saves_refused"`
	SavesFailed     int64 `json:"saves_failed"`
}

func (ad *admin) roomsAPICounters() roomsAPICounters {
	h := ad.rooms
	return roomsAPICounters{
		TicketsRefused: h.ticketsRefused.Load(),
		Statuses:       h.statuses.Load(), StatusesRefused: h.statusesRefused.Load(),
		Loads: h.loads.Load(), LoadsRefused: h.loadsRefused.Load(), LoadsFailed: h.loadsFailed.Load(),
		Saves: h.saves.Load(), SaveBatches: h.saveBatches.Load(), SavesRefused: h.savesRefused.Load(), SavesFailed: h.savesFailed.Load(),
	}
}

type metricsRoom struct {
	ID          int64   `json:"id"`
	Incarnation int64   `json:"incarnation"`
	Port        int     `json:"port"`
	Free        int     `json:"free"`
	Seated      int     `json:"seated"`
	Away        int     `json:"away"`
	AgeS        float64 `json:"age_s"`
	Alive       bool    `json:"alive"`
	Passes      int     `json:"passes"`
	PassAvgUs   int     `json:"pass_avg_us"`
	PassMaxUs   int     `json:"pass_max_us"`
	RoundTripMs int     `json:"round_trip_ms"`
	Capacity    int     `json:"capacity"`
	PeriodMs    int     `json:"period_ms"`
	Host        string  `json:"host"`
}

type serverPerfView struct {
	perfView
	Clients *clientView `json:"clients,omitempty"`
}

type serverRoomPerfView struct {
	roomPerfView
	Clients *clientView `json:"clients,omitempty"`
}

func serverPerfSnapshot(source perfSnapshot) map[string]any {
	rooms := make([]serverRoomPerfView, 0, len(source.Rooms))
	for _, room := range source.Rooms {
		rooms = append(rooms, serverRoomPerfView{roomPerfView: room})
	}
	var fleet *serverPerfView
	if source.Fleet != nil {
		fleet = &serverPerfView{perfView: *source.Fleet}
	}
	return map[string]any{"refused": source.Refused, "fleet": fleet, "rooms": rooms,
		"machines": source.Machines, "minutes": source.Minutes}
}

// roomPerfSummary is one room's last minute in the few figures a fleet table
// sorts by; the whole window is perfRoom's. About 300 bytes a room keeps the
// fleet's list readable within the admin service's limit at thousands of rooms.
type roomPerfSummary struct {
	ID              uint32   `json:"id"`
	Host            string   `json:"host"`
	Seated          int32    `json:"seated"`
	Capacity        int32    `json:"capacity"`
	AgeS            float64  `json:"age_s"`
	AvgSeated       float64  `json:"avg_seated"`
	CPUCorePct      *float64 `json:"cpu_core_pct"`
	CPUMsPerPlayerS *float64 `json:"cpu_ms_per_player_s"`
	TickWorkCorePct float64  `json:"tick_work_core_pct"`
	StepP99Us       uint64   `json:"step_p99_us"`
	ReplicateP99Us  uint64   `json:"replicate_p99_us"`
	LateP99Us       uint64   `json:"late_p99_us"`
	LateTicks       uint64   `json:"late_ticks"`
	DroppedTicks    uint64   `json:"dropped_ticks"`
	TxKbitPerPlayer float64  `json:"tx_kbit_s_per_player"`
	RttP99Ms        *float64 `json:"rtt_p99_ms"`
	// Closes that are not a player leaving: protocol, rate, slow.
	AbnormalCloses uint64   `json:"abnormal_closes"`
	RssMB          *float64 `json:"rss_mb"`
}

func summarizeRoomPerf(room roomPerfView) roomPerfSummary {
	out := roomPerfSummary{ID: room.ID, Host: room.Host, Seated: room.Seated, Capacity: room.Capacity, AgeS: room.AgeS,
		AvgSeated: room.AvgSeated, CPUCorePct: room.CPUCorePct, CPUMsPerPlayerS: room.CPUMsPerPlayerS,
		TickWorkCorePct: room.TickWorkCorePct, StepP99Us: room.Stages["step"].P99Us,
		ReplicateP99Us: room.Stages["replicate"].P99Us, LateP99Us: room.Stages["late"].P99Us,
		LateTicks: room.LateTicks, DroppedTicks: room.DroppedTicks, TxKbitPerPlayer: room.TxKbitPerPlayer,
		AbnormalCloses: room.Closes["protocol"] + room.Closes["rate"] + room.Closes["slow"], RssMB: room.RssMB}
	if room.Clients != nil {
		out.RttP99Ms = &room.Clients.RttP99Ms
	}
	return out
}

// perfSummary is the fleet's last minute for the admin service: the fleet,
// every machine and a summary of every room. History lives in activity.fleet_minutes.
func (ad *admin) perfSummary(w http.ResponseWriter, r *http.Request) {
	now := time.Now()
	snap := ad.rooms.perf.snapshot(now)
	rooms := make([]roomPerfSummary, 0, len(snap.Rooms))
	for _, room := range snap.Rooms {
		rooms = append(rooms, summarizeRoomPerf(room))
	}
	writeJSON(w, http.StatusOK, map[string]any{"now": now, "refused": snap.Refused, "fleet": snap.Fleet,
		"machines": snap.Machines, "rooms": rooms})
}

// perfRoom is one room's whole last minute, client reports included.
func (ad *admin) perfRoom(w http.ResponseWriter, r *http.Request) {
	id, err := strconv.ParseUint(r.PathValue("id"), 10, 32)
	if err != nil {
		writeJSON(w, http.StatusBadRequest, map[string]string{"error": "room id"})
		return
	}
	now := time.Now()
	for _, room := range ad.rooms.perf.snapshot(now).Rooms {
		if room.ID == uint32(id) {
			writeJSON(w, http.StatusOK, map[string]any{"now": now, "room": room})
			return
		}
	}
	writeJSON(w, http.StatusNotFound, map[string]string{"error": "no window from this room in the last minute"})
}

func (ad *admin) metrics(w http.ResponseWriter, r *http.Request) {
	ctx, cancel := context.WithTimeout(adminQueries(r.Context()), storeTimeout)
	defer cancel()
	list, err := ad.store.listRooms(ctx)
	if err != nil {
		ad.log.Error("admin metrics", "err", err)
		writeJSON(w, http.StatusServiceUnavailable, map[string]string{"error": "room metrics unavailable"})
		return
	}
	rooms := make([]metricsRoom, 0, len(list))
	for _, room := range list {
		rooms = append(rooms, metricsRoom{ID: room.ID, Incarnation: room.Incarnation, Port: room.Port,
			Free: room.Free, Seated: room.Seated, Away: room.Away, AgeS: room.AgeS, Alive: room.Alive,
			Passes: room.Passes, PassAvgUs: room.PassAvgUs, PassMaxUs: room.PassMaxUs,
			RoundTripMs: room.RoundTripMs, Capacity: room.Capacity, PeriodMs: room.PeriodMs, Host: room.Host})
	}
	now := time.Now()
	writeJSON(w, http.StatusOK, map[string]any{
		"now": now, "rooms": rooms, "room_perf": serverPerfSnapshot(ad.rooms.perf.snapshot(now)),
		"observability": ad.observations.snapshot(now), "rooms_api": ad.roomsAPICounters(),
	})
}

// snapshot answers 200 even with the database gone: the counters and the
// database's own history are what the operator needs then; `rooms` and
// `players` are null and `error` says why. `certificates` is null when
// the rooms are behind a proxy, empty until the first hourly check.
// The incidents view keeps under the admin service's 1 MiB read limit even in
// an error storm: the newest events only, and details cut short.
const (
	incidentEvents      = 100
	incidentDetailRunes = 512
)

// redactedDB passes the last driver error through the redaction events get:
// it can name the host, the role or a value.
func redactedDB(summary dbHealthSummary) dbHealthSummary {
	if summary.Last != nil {
		last := *summary.Last
		last.Text = observationDetail(last.Text)
		summary.Last = &last
	}
	return summary
}

// incidents is what went wrong in the observation window, for the admin service.
func (ad *admin) incidents(w http.ResponseWriter, r *http.Request) {
	now := time.Now()
	observed := ad.observations.snapshot(now)
	if len(observed.Events) > incidentEvents {
		observed.Events = observed.Events[:incidentEvents]
	}
	for i := range observed.Events {
		observed.Events[i].Detail = observationTextLimit(observed.Events[i].Detail, incidentDetailRunes)
	}
	for i := range observed.Groups {
		observed.Groups[i].Detail = observationTextLimit(observed.Groups[i].Detail, incidentDetailRunes)
	}
	db := redactedDB(ad.store.health.summary(now))
	db.Minutes = nil
	writeJSON(w, http.StatusOK, map[string]any{"now": now, "since": ad.started, "observability": observed,
		"alerts": ad.rooms.alerts.snapshot(), "certificates": ad.certs.snapshot(), "db": db,
		"rooms_api": ad.roomsAPICounters()})
}

func (ad *admin) snapshot(w http.ResponseWriter, r *http.Request) {
	ctx, cancel := context.WithTimeout(adminQueries(r.Context()), storeTimeout)
	defer cancel()
	now := time.Now()
	h := ad.rooms
	var memory runtime.MemStats
	runtime.ReadMemStats(&memory)
	pool := ad.store.pool.Stat()
	out := map[string]any{
		"observability": ad.observations.snapshot(now),
		"process": map[string]any{"heap_bytes": memory.HeapAlloc, "goroutines": runtime.NumGoroutine(),
			"db_connections": pool.TotalConns(), "db_acquired": pool.AcquiredConns(), "db_max": pool.MaxConns(),
			"db_empty_acquires": pool.EmptyAcquireCount()},
		"now":              now,
		"since":            ad.started,
		"rooms":            nil,
		"players":          nil,
		"db":               redactedDB(ad.store.health.summary(now)),
		"certificates":     ad.certs.snapshot(),
		"room_perf":        h.perf.snapshot(now), // nil-safe
		"traffic":          h.traffic.snapshot(now),
		"traffic_month_gb": ad.cfg.TrafficMonthGB, // the plan allowance; 0: none
		"alerts":           h.alerts.snapshot(),
		"rooms_api":        ad.roomsAPICounters(),
	}
	list, err := ad.store.listRooms(ctx)
	if err == nil {
		for i := range list {
			list[i].URL = ad.roomURL(roomAddress{id: uint32(list[i].ID), port: uint16(list[i].Port), host: list[i].Host})
		}
		out["rooms"] = list
		var counts playerCounts
		if counts, err = ad.playerCounts(ctx, now); err == nil {
			out["players"] = counts
		}
	}
	if err != nil {
		ad.log.Error("admin snapshot", "err", err)
		out["error"] = "database: " + err.Error()
	}
	// Every admin page reads this snapshot, so what an error storm grows
	// (events, groups, alerts) goes through the bounded incidents view instead.
	if r.URL.Path == "/internal/admin/v1/snapshot" {
		delete(out, "observability")
		delete(out, "room_perf")
		delete(out, "alerts")
		delete(out, "certificates")
	}
	writeJSON(w, http.StatusOK, out)
}

func (ad *admin) playerCounts(ctx context.Context, now time.Time) (playerCounts, error) {
	ad.countsMu.Lock()
	defer ad.countsMu.Unlock()
	if now.Sub(ad.countsAt) < countsEvery {
		return ad.counts, nil
	}
	counts, err := ad.store.countPlayers(ctx)
	if err != nil {
		return counts, err
	}
	ad.counts, ad.countsAt = counts, now
	return counts, nil
}
