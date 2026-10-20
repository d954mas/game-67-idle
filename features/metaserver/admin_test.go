package metaserver

import (
	"context"
	"encoding/json"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"strconv"
	"strings"
	"testing"
	"time"
)

func adminGet(t *testing.T, h http.Handler, path, peer, host string, header ...string) (int, map[string]any) {
	t.Helper()
	req := httptest.NewRequest("GET", path, nil)
	req.RemoteAddr = peer
	req.Host = host
	for i := 0; i+1 < len(header); i += 2 {
		req.Header.Set(header[i], header[i+1])
	}
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, req)
	body := map[string]any{}
	if strings.HasPrefix(rec.Header().Get("Content-Type"), "application/json") {
		if err := json.NewDecoder(rec.Body).Decode(&body); err != nil {
			t.Fatalf("%s: status %d, body not JSON: %v", path, rec.Code, err)
		}
	}
	return rec.Code, body
}

func TestInternalSnapshotUsesSeparateServiceCredential(t *testing.T) {
	nets, _ := parseNets("127.0.0.0/8")
	ad := &admin{cfg: Config{AdminNets: nets, AdminServiceToken: "internal-service-token-with-enough-entropy"}}
	h := ad.internalGuard(func(w http.ResponseWriter, _ *http.Request) {
		writeJSON(w, http.StatusOK, map[string]any{"now": "2026-09-29", "rooms": []any{}})
	})
	path := "/internal/admin/v1/snapshot"
	host := "127.0.0.1:8081"

	for _, header := range [][]string{nil, {"Authorization", "Bearer wrong"}, {"Authorization", "Basic b3BlcmF0b3I6b3BlcmF0b3ItcGFzc3dvcmQ="}} {
		code, _ := adminGet(t, h, path, "127.0.0.1:5000", host, header...)
		if code != http.StatusUnauthorized {
			t.Fatalf("untrusted internal request: %d", code)
		}
	}
	credential := []string{"Authorization", "Bearer " + ad.cfg.AdminServiceToken}
	if code, _ := adminGet(t, h, path, "203.0.113.4:5000", host, credential...); code != http.StatusForbidden {
		t.Fatalf("external peer: %d", code)
	}
	if code, _ := adminGet(t, h, path, "127.0.0.1:5000", "attacker.test", credential...); code != http.StatusMisdirectedRequest {
		t.Fatalf("foreign host: %d", code)
	}
	code, data := adminGet(t, h, path, "127.0.0.1:5000", host, credential...)
	if code != http.StatusOK || data["rooms"] == nil || data["now"] == nil {
		t.Fatalf("authorized snapshot: %d %v", code, data)
	}
	ad.cfg.AdminServiceToken = ""
	if code, _ := adminGet(t, ad.handler(), path, "127.0.0.1:5000", host); code != http.StatusNotFound {
		t.Fatalf("disabled internal route: %d", code)
	}
}

func TestInternalMetricsKeepsSensitiveRoomFieldsPrivate(t *testing.T) {
	st, _ := freshStore(t)
	a := newTestAPI(t, st)
	a.cfg.AdminNets, _ = parseNets("127.0.0.0/8")
	a.cfg.AdminServiceToken = "internal-service-token-with-enough-entropy"
	roomCall(t, a.handler(testRoutes), "/rooms/status", roomBearer(testKey, 2, 5), status(7002, 15, 42))
	h := newAdmin(a, nil).handler()
	path := "/internal/admin/v1/metrics"
	peer, host := "127.0.0.1:5000", "127.0.0.1:8081"
	if code, _ := adminGet(t, h, path, peer, host); code != http.StatusUnauthorized {
		t.Fatalf("metrics without token: %d", code)
	}
	credential := []string{"Authorization", "Bearer " + a.cfg.AdminServiceToken}
	if code, _ := adminGet(t, h, path, "203.0.113.4:5000", host, credential...); code != http.StatusForbidden {
		t.Fatalf("metrics from outside stand: %d", code)
	}
	code, data := adminGet(t, h, path, peer, host, credential...)
	if code != http.StatusOK || data["room_perf"] == nil || data["observability"] == nil ||
		data["rooms_api"] == nil || data["rooms"] == nil {
		t.Fatalf("metrics response: %d %v", code, data)
	}
	rooms := data["rooms"].([]any)
	if len(rooms) == 0 {
		t.Fatal("metrics omitted room")
	}
	if _, ok := rooms[0].(map[string]any)["accounts"]; ok {
		t.Fatal("metrics exposed room account IDs")
	}
	if _, ok := data["players"]; ok {
		t.Fatal("metrics exposed player counts")
	}
	_, snapshot := adminGet(t, h, "/internal/admin/v1/snapshot", peer, host, credential...)
	if _, ok := snapshot["room_perf"]; ok {
		t.Fatal("admin service snapshot widened")
	}
	for _, key := range []string{"observability", "alerts", "certificates"} {
		if _, ok := snapshot[key]; ok {
			t.Fatalf("admin service snapshot carries %s, which an error storm grows", key)
		}
	}
	if _, ok := snapshot["traffic"]; !ok {
		t.Fatal("admin service snapshot lost the traffic ledger")
	}
	if _, ok := snapshot["traffic_month_gb"]; !ok {
		t.Fatal("admin service snapshot lost the traffic allowance")
	}
	st.close()
	if code, _ := adminGet(t, h, path, peer, host, credential...); code != http.StatusServiceUnavailable {
		t.Fatalf("unavailable room source returned %d", code)
	}
}

func TestServerPerfSnapshotExcludesClientReports(t *testing.T) {
	source := perfSnapshot{Fleet: &perfView{CPUCorePct: ptr(12.5), Clients: &clientView{Reports: 7}},
		Rooms: []roomPerfView{{perfView: perfView{Clients: &clientView{Reports: 3}}}}}
	data, err := json.Marshal(serverPerfSnapshot(source))
	if err != nil {
		t.Fatal(err)
	}
	if strings.Contains(string(data), `"clients"`) || !strings.Contains(string(data), `"cpu_core_pct"`) {
		t.Fatalf("server metrics included client reports or omitted performance: %s", data)
	}
}

// The admin console shows what the rooms reported and what the store holds,
// and opens only for a socket peer inside the admin networks asking for
// this host by address, whatever the headers say.
func TestAdminShowsRoomsAndGuardsTheDoor(t *testing.T) {
	st, _ := freshStore(t)
	a := newTestAPI(t, st)
	a.cfg.BehindProxy = true
	a.cfg.AdminNets, _ = parseNets("127.0.0.0/8,::1/128")
	a.cfg.AdminHosts = []string{"stand.example"}
	a.cfg.AdminServiceToken = "internal-service-token-with-enough-entropy"
	token := []string{"Authorization", "Bearer " + a.cfg.AdminServiceToken}
	h := a.handler(testRoutes)
	ad := newAdmin(a, nil).handler()
	ctx := context.Background()

	roomCall(t, h, "/rooms/status", roomBearer(testKey, 2, 5), status(7002, 15, 42))
	roomCall(t, h, "/rooms/status", roomBearer(testKey, 3, 5), status(7003, 16))
	if _, err := st.pool.Exec(ctx, `INSERT INTO players (id, login) OVERRIDING SYSTEM VALUE VALUES (42, 'ann')`); err != nil {
		t.Fatal(err)
	}
	if _, _, err := st.loadDoc(ctx, 42, 2, 5, defaultProfileMaxBytes); err != nil {
		t.Fatal(err)
	}
	if _, err := st.saveDocs(ctx, 2, 5, []docSaveEntry{{account: 42, saveID: 1, doc: []byte("doc")}}); err != nil {
		t.Fatal(err)
	}
	// One refused save, one refused load, for the counters.
	roomCall(t, h, "/rooms/profiles/42/save", roomBearer(testKey, 3, 5), saveBody(1, 1, "x"))
	roomCall(t, h, "/rooms/profiles/42/load", roomBearer(testKey, 2, 4), nil)

	code, snap := adminGet(t, ad, "/internal/admin/v1/snapshot", "127.0.0.1:5000", "127.0.0.1:8081", token...)
	if code != http.StatusOK {
		t.Fatalf("snapshot: %d %v", code, snap)
	}
	rooms := snap["rooms"].([]any)
	if len(rooms) != 3 {
		t.Fatalf("rooms: %v", rooms)
	}
	room2 := rooms[0].(map[string]any)
	if room2["id"].(float64) != 2 || room2["url"] != "ws://127.0.0.1:7002/" || room2["alive"] != true ||
		room2["seated"].(float64) != 1 || room2["passes"].(float64) != 30 || room2["round_trip_ms"].(float64) != 3 ||
		room2["accounts"].([]any)[0].(float64) != 42 {
		t.Fatalf("room 2: %v", room2)
	}
	players := snap["players"].(map[string]any)
	if players["accounts"].(float64) != 1 || players["profiles"].(float64) != 1 {
		t.Fatalf("players: %v", players)
	}
	counters := snap["rooms_api"].(map[string]any)
	if counters["statuses"].(float64) != 2 || counters["saves_refused"].(float64) != 1 || counters["loads_refused"].(float64) != 1 || counters["statuses_refused"].(float64) != 0 {
		t.Fatalf("counters: %v", counters)
	}
	db := snap["db"].(map[string]any)
	_, incidents := adminGet(t, ad, "/internal/admin/v1/incidents", "127.0.0.1:5000", "127.0.0.1:8081", token...)
	observed := incidents["observability"].(map[string]any)
	endpoints := observed["endpoints"].(map[string]any)
	if endpoints["status"].(map[string]any)["requests"].(float64) != 2 || endpoints["load"].(map[string]any)["client_errors"].(float64) != 1 {
		t.Fatalf("admin lost HTTP observations: %v", observed)
	}
	if snap["process"].(map[string]any)["db_max"].(float64) <= 0 {
		t.Fatal("admin lost process metrics")
	}
	if db["queries"].(float64) < 4 || len(db["minutes"].([]any)) != observationMinutes || db["last_error"] != nil {
		t.Fatalf("db: %v", db)
	}

	// The door: the peer decides, in v4 or mapped v6; the proxy header
	// does not; a foreign host name does not; the configured name does, in any case.
	for name, tc := range map[string]struct {
		peer, host string
		header     []string
		want       int
	}{
		"loopback":        {"127.0.0.1:5000", "127.0.0.1:8081", nil, http.StatusOK},
		"v6 loopback":     {"[::1]:5000", "[::1]:8081", nil, http.StatusOK},
		"mapped loopback": {"[::ffff:127.0.0.1]:5000", "127.0.0.1:8081", nil, http.StatusOK},
		"outside":         {"10.1.2.3:5000", "127.0.0.1:8081", nil, http.StatusForbidden},
		"spoofed header":  {"10.1.2.3:5000", "127.0.0.1:8081", []string{"X-Forwarded-For", "127.0.0.1"}, http.StatusForbidden},
		"foreign host":    {"127.0.0.1:5000", "other.example:8081", nil, http.StatusMisdirectedRequest},
		"public name":     {"127.0.0.1:5000", "Stand.Example:443", nil, http.StatusOK},
	} {
		if code, _ := adminGet(t, ad, "/internal/admin/v1/snapshot", tc.peer, tc.host, append(tc.header, token...)...); code != tc.want {
			t.Errorf("%s: %d, want %d", name, code, tc.want)
		}
	}
	for _, gone := range []string{"/admin", "/admin.json", "/admin/profile?player=42"} {
		if code, _ := adminGet(t, ad, gone, "127.0.0.1:5000", "127.0.0.1:8081"); code != http.StatusNotFound {
			t.Fatalf("old page %s: %d", gone, code)
		}
	}
}

// The heartbeat pings the check while a room is alive and the database
// answers, and its /fail address, with the reason, when either is gone.
func TestHeartbeatPingsWhileTheStandCanSeat(t *testing.T) {
	st, _ := freshStore(t)
	newTestAPI(t, st) // registers room 9 with free seats
	var paths []string
	var bodies []string
	watcher := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		body, _ := io.ReadAll(r.Body)
		paths = append(paths, r.URL.Path)
		bodies = append(bodies, string(body))
	}))
	defer watcher.Close()
	cfg := Config{HeartbeatURL: watcher.URL + "/abc", HeartbeatEvery: 20 * time.Second}
	lives := newRestarts()
	h := newHeartbeat(cfg, st, nil, nil, lives, nil, slog.New(slog.NewTextHandler(io.Discard, nil)))
	ctx := context.Background()
	if ok, report := h.beat(ctx); !ok || !strings.HasPrefix(report, "rooms alive=1 seated=0") {
		t.Fatalf("healthy: %v %q", ok, report)
	}
	/* A room back under a new incarnation twice is a release or a hiccup;
	   the third time within the hour is a room in trouble. */
	now := time.Now()
	for incarnation := uint64(1); incarnation <= 3; incarnation++ {
		lives.note(9, incarnation, now)
	}
	if ok, _ := h.beat(ctx); !ok {
		t.Fatal("two restarts paged")
	}
	lives.note(9, 4, now)
	if ok, report := h.beat(ctx); ok || report != "room 9 restarted 3 times in the last hour" {
		t.Fatalf("three restarts: %v %q", ok, report)
	}
	lives = newRestarts()
	h.lives = lives
	if _, err := st.pool.Exec(ctx, `UPDATE rooms SET seen_at = now() - interval '1 minute'`); err != nil {
		t.Fatal(err)
	}
	if ok, report := h.beat(ctx); ok || !strings.HasPrefix(report, "no room alive") {
		t.Fatalf("no room: %v %q", ok, report)
	}
	st.pool.Close()
	if ok, report := h.beat(ctx); ok || !strings.HasPrefix(report, "database:") {
		t.Fatalf("no database: %v %q", ok, report)
	}
	if strings.Join(paths, " ") != "/abc /abc /abc/fail /abc/fail /abc/fail" || !strings.HasPrefix(bodies[0], "rooms alive=1") {
		t.Fatalf("pings: %v %v", paths, bodies)
	}
}

func TestAdminNetsParse(t *testing.T) {
	nets, err := parseNets(" 10.0.0.0/8, 172.16.0.0/12 ,192.168.1.5/24")
	if err != nil || len(nets) != 3 || nets[2].String() != "192.168.1.0/24" {
		t.Fatalf("nets: %v %v", nets, err)
	}
	for _, bad := range []string{"", "10.0.0.0", "10.0.0.0/8,zz"} {
		if _, err := parseNets(bad); err == nil {
			t.Errorf("%q parsed", bad)
		}
	}
}

// The database health sees a failing query and an acquire that timed out
// while the admin console's own reads stay out of the count.
func TestDBHealthRecordsFailures(t *testing.T) {
	st, _ := freshStore(t)
	ctx := context.Background()
	before := st.health.summary(time.Now()).Queries
	st.pool.Exec(ctx, `SELECT nothing FROM nowhere`)
	st.pool.Exec(adminQueries(ctx), `SELECT 1`)
	s := st.health.summary(time.Now())
	if s.Queries != before+1 || s.Errors != 1 || s.Last == nil || !strings.Contains(s.Last.Text, "nowhere") {
		t.Fatalf("summary: %+v", s)
	}
	expired, cancel := context.WithDeadline(ctx, time.Now().Add(-time.Second))
	defer cancel()
	st.pool.Exec(expired, `SELECT 1`)
	if s := st.health.summary(time.Now()); s.Errors != 2 || s.Queries != before+1 {
		t.Fatalf("expired acquire not counted as an error alone: %+v", s)
	}
}

// With the database gone the page still answers: the counters and the
// database's own history, and null where the registry would be.
func TestAdminAnswersWithoutTheDatabase(t *testing.T) {
	st, _ := freshStore(t)
	a := newTestAPI(t, st)
	a.cfg.AdminNets, _ = parseNets("127.0.0.0/8")
	a.cfg.AdminServiceToken = "internal-service-token-with-enough-entropy"
	ad := newAdmin(a, nil).handler()
	st.pool.Close()
	code, snap := adminGet(t, ad, "/internal/admin/v1/snapshot", "127.0.0.1:5000", "127.0.0.1:8081",
		"Authorization", "Bearer "+a.cfg.AdminServiceToken)
	if code != http.StatusOK || snap["rooms"] != nil || snap["players"] != nil || snap["error"] == nil ||
		snap["rooms_api"] == nil || snap["db"] == nil {
		t.Fatalf("without the database: %d %v", code, snap)
	}
}

func TestInternalIncidentsStayUnderTheAdminReadLimitInAStorm(t *testing.T) {
	st, _ := freshStore(t)
	a := newTestAPI(t, st)
	a.cfg.AdminNets, _ = parseNets("127.0.0.0/8")
	a.cfg.AdminServiceToken = "internal-service-token-with-enough-entropy"
	// Two-byte runes and markup JSON escapes: the longest bytes per rune.
	long := strings.Repeat("ж<", 8192)
	now := time.Now()
	a.observations.mu.Lock()
	for i := 0; i < 3*observationEventLimit; i++ {
		a.observations.eventLocked(observationEvent{At: now, Level: "ERROR",
			Message: strconv.Itoa(i%400) + long, Detail: long})
	}
	a.observations.mu.Unlock()
	h := newAdmin(a, nil).handler()
	req := httptest.NewRequest("GET", "/internal/admin/v1/incidents", nil)
	req.RemoteAddr, req.Host = "127.0.0.1:5000", "127.0.0.1:8081"
	req.Header.Set("Authorization", "Bearer "+a.cfg.AdminServiceToken)
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, req)
	// The admin service refuses a body past 1 MiB (cmd/admin/meta.go).
	if rec.Code != http.StatusOK || rec.Body.Len() > 1<<20 {
		t.Fatalf("incidents in a storm: %d, %d bytes", rec.Code, rec.Body.Len())
	}
	var body struct {
		Observability observationSnapshot `json:"observability"`
	}
	if err := json.Unmarshal(rec.Body.Bytes(), &body); err != nil || len(body.Observability.Events) != incidentEvents ||
		len(body.Observability.Groups) == 0 {
		t.Fatalf("incidents body: %v events=%d groups=%d", err, len(body.Observability.Events), len(body.Observability.Groups))
	}
}

func TestInternalPerfSummarizesRoomsAndServesOneWhole(t *testing.T) {
	st, _ := freshStore(t)
	a := newTestAPI(t, st)
	a.cfg.AdminNets, _ = parseNets("127.0.0.0/8")
	a.cfg.AdminServiceToken = "internal-service-token-with-enough-entropy"
	p, _ := decodeRoomPerf(perfFixture(t))
	now := time.Now()
	status := &roomStatus{Host: "b.test", Seated: 16, Capacity: 16}
	const rooms = 500
	for id := uint32(1); id <= rooms; id++ {
		a.rooms.perf.record(now, id, 7, status, p)
	}
	h := newAdmin(a, nil).handler()
	get := func(path string) *httptest.ResponseRecorder {
		req := httptest.NewRequest("GET", path, nil)
		req.RemoteAddr, req.Host = "127.0.0.1:5000", "127.0.0.1:8081"
		req.Header.Set("Authorization", "Bearer "+a.cfg.AdminServiceToken)
		rec := httptest.NewRecorder()
		h.ServeHTTP(rec, req)
		return rec
	}
	summary := get("/internal/admin/v1/perf")
	var body struct {
		Fleet    *perfView         `json:"fleet"`
		Machines []machineView     `json:"machines"`
		Rooms    []roomPerfSummary `json:"rooms"`
	}
	if err := json.Unmarshal(summary.Body.Bytes(), &body); err != nil || summary.Code != http.StatusOK ||
		len(body.Rooms) != rooms || body.Fleet == nil || len(body.Machines) != 1 || body.Rooms[0].StepP99Us != 250 {
		t.Fatalf("perf summary: %d %v rooms=%d", summary.Code, err, len(body.Rooms))
	}
	// The admin service reads at most 1 MiB: a summary must leave room for thousands of rooms.
	if perRoom := summary.Body.Len() / rooms; perRoom > 400 {
		t.Fatalf("a room's summary takes %d bytes", perRoom)
	}
	if whole := get("/internal/admin/v1/perf/rooms/3"); whole.Code != http.StatusOK || !strings.Contains(whole.Body.String(), `"stages"`) {
		t.Fatalf("one room: %d", whole.Code)
	}
	if missing := get("/internal/admin/v1/perf/rooms/9999"); missing.Code != http.StatusNotFound {
		t.Fatalf("a silent room: %d", missing.Code)
	}
	if bad := get("/internal/admin/v1/perf/rooms/x"); bad.Code != http.StatusBadRequest {
		t.Fatalf("a bad id: %d", bad.Code)
	}
}
