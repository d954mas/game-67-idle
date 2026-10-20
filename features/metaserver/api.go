package metaserver

import (
	"bytes"
	"context"
	"encoding/base64"
	"encoding/json"
	"errors"
	"io"
	"log/slog"
	"net"
	"net/http"
	"net/netip"
	"strconv"
	"strings"
	"time"

	"golang.org/x/crypto/bcrypt"
)

// Server is the metaserver: player identity, room registry, entry tickets
// and profile storage. A guest is made once, and its session secret buys
// entry tickets; a login and password can attach to it later. Game traffic
// never comes here; after entry the client talks WebSocket to the room
// directly. The game mounts its routes on these handlers (Main).
// A session may enter this often: a burst covers the retry ladder and a
// run of FULL re-entries, the trickle covers a client stuck in a loop.
const (
	sessionBurst = 30
	sessionEvery = 2 * time.Second
	// "Bearer " and a base64url secret of secretBytes.
	sessionKeyMax = 64
	// Entries one address may make: a burst for a venue's launch, then a
	// rate that is a lookup flood's ceiling rather than any crowd's need.
	entryBurst = 300
	entryEvery = 10 * time.Millisecond
)

type Server struct {
	cfg          Config
	key          ticketKey
	store        *store
	log          *slog.Logger
	calls        *limiter // per address: rows and hashes
	sessionCalls *limiter // per session bearer: entries
	entries      *limiter // per address: entries, a ceiling not a budget
	attempts     *limiter
	hashes       chan struct{}
	// Compared against for a login nobody has, so the answer takes as long
	// as a wrong password; minted at start at the same cost.
	dummyHash string
	now       func() time.Time
	tally     *tally
	// Embedded so the room handlers are the Server's own exported methods.
	*rooms
	observations *observations
}

func newAPI(cfg Config, key ticketKey, st *store, log *slog.Logger) *Server {
	dummy, err := bcrypt.GenerateFromPassword([]byte("nobody"), bcrypt.DefaultCost)
	if err != nil {
		panic(err)
	}
	profileMax := cfg.ProfileMaxBytes
	if profileMax == 0 {
		profileMax = defaultProfileMaxBytes
	}
	return &Server{
		cfg: cfg, key: key, store: st, log: log,
		calls:        newLimiter(cfg.CallsPerHour, time.Hour/time.Duration(cfg.CallsPerHour)),
		sessionCalls: newLenientLimiter(sessionBurst, sessionEvery),
		entries:      newLimiter(entryBurst, entryEvery),
		attempts:     newLimiter(attemptBurst, attemptWindow/attemptBurst),
		hashes:       make(chan struct{}, hashingSlots()),
		dummyHash:    string(dummy),
		now:          time.Now,
		tally:        newTally(),
		observations: newObservations(),
		rooms: &rooms{key: key, store: st, log: log, now: time.Now, restarts: newRestarts(), profileMax: profileMax,
			perf: newPerfBoard(), traffic: newTrafficLedger(), alerts: newAlertTracker()},
	}
}

// handler is what the player port serves: the game's routes, the probes,
// any CORS preflight, all observed.
func (a *Server) handler(mount func(*Server, *http.ServeMux)) http.Handler {
	mux := http.NewServeMux()
	// Probes are polled every few seconds by the supervisor: no log line each.
	mux.HandleFunc("GET /healthz", labeled("probe", func(w http.ResponseWriter, _ *http.Request) { w.Write([]byte("ok\n")) }))
	mux.HandleFunc("GET /readyz", labeled("probe", a.ready))
	mount(a, mux)
	// A preflight is answered for any path: a pattern for it would turn
	// every unknown path into 405 instead of 404.
	preflight := a.cors(http.NotFoundHandler())
	return a.observations.handler(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Method == http.MethodOptions {
			preflight.ServeHTTP(w, r)
			return
		}
		mux.ServeHTTP(w, r)
	}))
}

// Limit is the budget a player route draws on before any work is done.
type Limit int

const (
	// PerAddress: calls that make rows or hash passwords (META_CALLS_PER_HOUR).
	PerAddress Limit = iota
	// PerSession: calls a whole venue behind one address makes in bursts
	// (every FULL re-entry is one): a session budget, and per address only a
	// ceiling no venue reaches.
	PerSession
)

// Player wraps a player route: CORS, its budget, a log line for what went
// wrong, the store deadline, and `name` as its tally and observation label.
func (a *Server) Player(limit Limit, name string, h http.HandlerFunc) http.HandlerFunc {
	return labeled(name, a.cors(a.limited(limit, a.logged(a.deadline(a.tally.timed(name, h))))).ServeHTTP)
}

// Timed wraps a room route: `name` is its tally and observation label. Room
// calls are neither a browser's nor a player's: no CORS, no per-address
// allowance, and a status a second per room is not worth a log line each.
func (a *Server) Timed(name string, h http.HandlerFunc) http.HandlerFunc {
	return labeled(name, a.tally.timed(name, h))
}

// Every store call of a player's request runs under one bound: a hung
// database answers 503 in two seconds instead of holding the request,
// its memory and the player for the server's full timeout.
func (a *Server) deadline(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		ctx, cancel := context.WithTimeout(r.Context(), storeTimeout)
		defer cancel()
		next.ServeHTTP(w, r.WithContext(ctx))
	})
}

// roomURL fills the template: `{id}` for the room id, `{port}` and
// `{host}` for what it reported.
func (a *Server) roomURL(room roomAddress) string {
	url := strings.ReplaceAll(a.cfg.RoomURL, "{id}", strconv.FormatUint(uint64(room.id), 10))
	url = strings.ReplaceAll(url, "{host}", room.host)
	return strings.ReplaceAll(url, "{port}", strconv.Itoa(int(room.port)))
}

func (a *Server) ready(w http.ResponseWriter, r *http.Request) {
	ctx, cancel := context.WithTimeout(r.Context(), time.Second)
	defer cancel()
	if err := a.store.pool.Ping(ctx); err != nil {
		http.Error(w, "database unreachable", http.StatusServiceUnavailable)
		return
	}
	w.Write([]byte("ready\n"))
}

// Guest hands out a new identity. The secret goes over the wire
// exactly once; the client that loses it starts over as somebody new.
func (a *Server) Guest(w http.ResponseWriter, r *http.Request) {
	secret, err := newSecret()
	if err != nil {
		a.fail(w, err)
		return
	}
	hash, _ := secretHash(secret)
	id, err := a.store.createGuest(r.Context(), hash)
	if err != nil {
		a.fail(w, err)
		return
	}
	writeJSON(w, http.StatusCreated, map[string]any{"player_id": id, "secret": secret})
}

func readPageEntryIntent(w http.ResponseWriter, r *http.Request) (bool, error) {
	decoder := json.NewDecoder(http.MaxBytesReader(w, r.Body, 128))
	first, err := decoder.Token()
	if err == io.EOF {
		return false, nil
	}
	if err != nil {
		return false, err
	}
	if first != json.Delim('{') {
		return false, errors.New("entry intent must be an object")
	}
	pageEntry := false
	seen := false
	for decoder.More() {
		key, err := decoder.Token()
		if err != nil {
			return false, err
		}
		if key != "page_entry" || seen {
			return false, errors.New("invalid entry intent key")
		}
		seen = true
		var raw json.RawMessage
		if err := decoder.Decode(&raw); err != nil {
			return false, err
		}
		switch string(bytes.TrimSpace(raw)) {
		case "true":
			pageEntry = true
		case "false":
			pageEntry = false
		default:
			return false, errors.New("page entry must be a boolean")
		}
	}
	if _, err := decoder.Token(); err != nil {
		return false, err
	}
	if _, err := decoder.Token(); err != io.EOF {
		return false, errors.New("trailing entry intent")
	}
	return pageEntry, nil
}

// Enter routes the session's player to a room and issues its ticket. No
// room with a seat is a plain refusal: the client's retry ladder asks
// again, and a full room reached by a lost race sends it back here the
// same way. The login rides along so the client can show whose progress
// this is without keeping a copy. A body of {"page_entry": true} asks for
// a page-entry ticket: the page was reloaded, not a seat lost.
func (a *Server) Enter(w http.ResponseWriter, r *http.Request) {
	who, _, ok := a.session(w, r)
	if !ok {
		return
	}
	pageEntry, err := readPageEntryIntent(w, r)
	if err != nil {
		writeError(w, http.StatusBadRequest, "entry_intent")
		return
	}
	id := who.id
	now := a.now()
	room, err := a.store.pick(r.Context(), id)
	if errors.Is(err, errNoRoom) {
		writeError(w, http.StatusServiceUnavailable, "no room has a free seat")
		return
	}
	if err != nil {
		a.fail(w, err)
		return
	}
	roomURL := a.roomURL(room)
	expiresAt := now.Add(a.cfg.TicketTTL).Unix()
	kind := byte(ticketKindPlayer)
	if pageEntry {
		kind = ticketKindPageEntry
	}
	ticket := encodeTicket(a.key, kind, uint64(id), room.id, uint32(expiresAt))
	writeJSON(w, http.StatusOK, map[string]any{
		"room_id":    room.id,
		"url":        roomURL,
		"ticket":     base64.RawURLEncoding.EncodeToString(ticket[:]),
		"expires_at": expiresAt,
		"login":      who.login,
	})
}

// fail answers a store error: a wait that ran out is the database being
// slow or gone (503, the client's ladder retries), anything else is ours.
func (a *Server) fail(w http.ResponseWriter, err error) {
	a.log.Error("request failed", "error", err)
	if errors.Is(err, context.DeadlineExceeded) {
		writeError(w, http.StatusServiceUnavailable, "database busy")
		return
	}
	writeError(w, http.StatusInternalServerError, "internal error")
}

func writeJSON(w http.ResponseWriter, status int, body any) {
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Cache-Control", "no-store")
	w.WriteHeader(status)
	json.NewEncoder(w).Encode(body)
}

func writeError(w http.ResponseWriter, status int, message string) {
	writeJSON(w, status, map[string]string{"error": message})
}

// clientIP is the socket's peer, or, behind our own reverse proxy, the
// address the proxy appended last to X-Forwarded-For: earlier hops are
// whatever the client sent and are not trusted.
func (a *Server) clientIP(r *http.Request) string {
	if a.cfg.BehindProxy {
		if lines := r.Header.Values("X-Forwarded-For"); len(lines) > 0 {
			hops := strings.Split(lines[len(lines)-1], ",")
			if last := strings.TrimSpace(hops[len(hops)-1]); last != "" {
				return last
			}
		}
	}
	host, _, err := net.SplitHostPort(r.RemoteAddr)
	if err != nil {
		return r.RemoteAddr
	}
	return host
}

// limited bounds what an address can cost before any work is done. A
// PerSession route is metered per session, so a crowd is never refused for
// sharing a router, and per address only at a rate no venue reaches, so a
// spray of made-up sessions from one address cannot keep the store busy
// looking them up.
func (a *Server) limited(limit Limit, next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		address := a.clientIP(r)
		unlimited := a.unlimited(address)
		switch limit {
		case PerAddress:
			if !unlimited && !a.calls.allow(address, a.now()) {
				writeError(w, http.StatusTooManyRequests, "too many calls from this address")
				return
			}
		case PerSession:
			if !unlimited && !a.entries.allow(address, a.now()) {
				writeError(w, http.StatusTooManyRequests, "too many calls from this address")
				return
			}
			if !a.sessionCalls.allow(sessionKey(r), a.now()) {
				writeError(w, http.StatusTooManyRequests, "too many calls for this session")
				return
			}
		}
		next.ServeHTTP(w, r)
	})
}

// unlimited: the address is one of the operator's load generators (Config.UnlimitedAddrs).
func (a *Server) unlimited(address string) bool {
	addr, err := netip.ParseAddr(address)
	if err != nil {
		return false
	}
	addr = addr.Unmap()
	for _, prefix := range a.cfg.UnlimitedAddrs {
		if prefix.Contains(addr) {
			return true
		}
	}
	return false
}

// sessionKey is the bearer as the session check reads it, cut to the
// length of a real one so a long header cannot buy a long key in the
// limiter's map, and stripped of the spacing a sender could vary to be
// several keys at once.
func sessionKey(r *http.Request) string {
	secret, _ := strings.CutPrefix(r.Header.Get("Authorization"), "Bearer ")
	key := strings.TrimSpace(secret)
	if len(key) > sessionKeyMax {
		key = key[:sessionKeyMax]
	}
	return key
}

// cors lets a game page on any origin call the API: nothing here relies
// on cookies, so a wildcard origin gives away nothing.
func (a *Server) cors(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Access-Control-Allow-Origin", "*")
		w.Header().Set("Access-Control-Allow-Headers", "Authorization, Content-Type")
		w.Header().Set("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
		w.Header().Set("Access-Control-Max-Age", "600")
		if r.Method == http.MethodOptions {
			w.WriteHeader(http.StatusNoContent)
			return
		}
		next.ServeHTTP(w, r)
	})
}

// logged writes a line for what went wrong; the calls that went right
// are counted in the period line, since a thousand of them a second are
// not worth a line each.
func (a *Server) logged(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		start := a.now()
		rec := &statusRecorder{ResponseWriter: w, status: http.StatusOK}
		next.ServeHTTP(rec, r)
		if rec.status >= 400 && rec.status != http.StatusUnauthorized {
			a.log.Info("http", "method", r.Method, "path", r.URL.Path, "status", rec.status,
				"ms", time.Since(start).Milliseconds(), "ip", a.clientIP(r))
		}
	})
}

type statusRecorder struct {
	http.ResponseWriter
	status int
}

func (r *statusRecorder) WriteHeader(status int) {
	r.status = status
	r.ResponseWriter.WriteHeader(status)
}
