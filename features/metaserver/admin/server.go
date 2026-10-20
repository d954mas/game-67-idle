package admin

import (
	"bytes"
	"context"
	"embed"
	"encoding/base64"
	"encoding/json"
	"errors"
	"html/template"
	"io"
	"io/fs"
	"math"
	"net"
	"net/http"
	"strconv"
	"strings"
	"sync"
	"time"
)

//go:embed web/index.html web/app.css web/app.js web/capacity.js web/overview.js web/incidents.js web/rooms.js web/vendor/uPlot.iife.min.js web/vendor/uPlot.min.css
var webFiles embed.FS

type loginCount struct {
	count int
	until time.Time
}

type adminServer struct {
	brand          Brand
	store          *adminStore
	meta           metaClient
	origin         string
	totpRequired   bool
	now            func() time.Time
	mu             sync.Mutex
	tries          map[string]loginCount
	clients        map[string]loginCount
	global         loginCount
	passwordChecks chan struct{}
	checksOnce     sync.Once
}

func (s *adminServer) handler() http.Handler {
	mux := http.NewServeMux()
	assets, _ := fs.Sub(webFiles, "web")
	var index bytes.Buffer
	if err := template.Must(template.ParseFS(webFiles, "web/index.html")).Execute(&index, s.brand); err != nil {
		panic(err)
	}
	mux.Handle("GET /", http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path == "/" {
			w.Header().Set("Content-Type", "text/html; charset=utf-8")
			w.Write(index.Bytes())
			return
		}
		if !webPaths[r.URL.Path] {
			http.NotFound(w, r)
			return
		}
		http.FileServer(http.FS(assets)).ServeHTTP(w, r)
	}))
	mux.HandleFunc("POST /admin-api/v1/login", s.login)
	mux.HandleFunc("GET /admin-api/v1/auth-config", func(w http.ResponseWriter, _ *http.Request) {
		apiJSON(w, http.StatusOK, map[string]bool{"totp_required": s.totpRequired})
	})
	mux.HandleFunc("POST /admin-api/v1/logout", s.authorized(s.logout))
	mux.HandleFunc("GET /admin-api/v1/session", s.authorized(s.session))
	mux.HandleFunc("GET /admin-api/v1/overview", s.authorized(s.overview))
	mux.HandleFunc("GET /admin-api/v1/rooms", s.authorized(s.rooms))
	mux.HandleFunc("GET /admin-api/v1/audience/occupancy", s.authorized(s.occupancy))
	mux.HandleFunc("GET /admin-api/v1/incidents", s.authorized(s.incidents))
	mux.HandleFunc("GET /admin-api/v1/performance", s.authorized(s.performance))
	mux.HandleFunc("GET /admin-api/v1/rooms/{id}", s.authorized(s.room))
	mux.HandleFunc("GET /admin-api/v1/capacity", s.authorizedWithin(capacityBudget, s.capacity))
	mux.HandleFunc("GET /admin-api/v1/players", s.authorized(s.players))
	mux.HandleFunc("GET /admin-api/v1/players/{id}", s.authorized(s.player))
	mux.HandleFunc("GET /healthz", func(w http.ResponseWriter, _ *http.Request) { w.Write([]byte("ok\n")) })
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Cache-Control", "no-store")
		w.Header().Set("X-Content-Type-Options", "nosniff")
		w.Header().Set("X-Frame-Options", "DENY")
		w.Header().Set("Referrer-Policy", "no-referrer")
		w.Header().Set("Content-Security-Policy", "default-src 'self'; object-src 'none'; frame-ancestors 'none'")
		mux.ServeHTTP(w, r)
	})
}

// webPaths are the only static files served; the vendored license and notes stay out.
var webPaths = map[string]bool{"/app.css": true, "/app.js": true, "/capacity.js": true, "/overview.js": true, "/incidents.js": true, "/rooms.js": true,
	"/vendor/uPlot.iife.min.js": true, "/vendor/uPlot.min.css": true}

func (s *adminServer) clock() time.Time {
	if s.now != nil {
		return s.now()
	}
	return time.Now()
}

func clientIP(r *http.Request) string {
	if lines := r.Header.Values("X-Forwarded-For"); len(lines) > 0 {
		hops := strings.Split(lines[len(lines)-1], ",")
		if forwarded := strings.TrimSpace(hops[len(hops)-1]); net.ParseIP(forwarded) != nil {
			return forwarded
		}
	}
	host, _, err := net.SplitHostPort(r.RemoteAddr)
	if err == nil && net.ParseIP(host) != nil {
		return host
	}
	return r.RemoteAddr
}

func (s *adminServer) allowLogin(username, ip string, now time.Time) bool {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.tries == nil {
		s.tries = make(map[string]loginCount)
	}
	if s.clients == nil {
		s.clients = make(map[string]loginCount)
	}
	if now.After(s.global.until) {
		s.global = loginCount{until: now.Add(time.Minute)}
	}
	if s.global.count >= 100 {
		return false
	}
	key := strings.ToLower(username)
	entry := s.tries[key]
	if now.After(entry.until) {
		entry = loginCount{until: now.Add(5 * time.Minute)}
	}
	if entry.count >= 10 {
		return false
	}
	client := s.clients[ip]
	if now.After(client.until) {
		client = loginCount{until: now.Add(time.Minute)}
	}
	if client.count >= 20 {
		return false
	}
	if len(s.tries) >= 1024 {
		for name, count := range s.tries {
			if now.After(count.until) {
				delete(s.tries, name)
			}
		}
		if len(s.tries) >= 1024 {
			return false
		}
	}
	if len(s.clients) >= 1024 {
		for address, count := range s.clients {
			if now.After(count.until) {
				delete(s.clients, address)
			}
		}
		if len(s.clients) >= 1024 {
			return false
		}
	}
	entry.count++
	client.count++
	s.global.count++
	s.tries[key] = entry
	s.clients[ip] = client
	return true
}

func (s *adminServer) login(w http.ResponseWriter, r *http.Request) {
	if !validOrigin(r, s.origin) || !strings.HasPrefix(r.Header.Get("Content-Type"), "application/json") {
		apiError(w, http.StatusForbidden, "request origin rejected")
		return
	}
	var input struct {
		Username string `json:"username"`
		Password string `json:"password"`
		TOTP     string `json:"totp"`
	}
	decoder := json.NewDecoder(http.MaxBytesReader(w, r.Body, 4096))
	decoder.DisallowUnknownFields()
	decodeErr := decoder.Decode(&input)
	validCode := input.TOTP == ""
	if s.totpRequired {
		validCode = len(input.TOTP) == 6
	}
	if decodeErr != nil || len(input.Username) == 0 || len(input.Username) > 64 ||
		len(input.Password) > 1024 || !validCode || decoder.Decode(new(any)) != io.EOF {
		apiError(w, http.StatusBadRequest, "invalid login request")
		return
	}
	now := s.clock()
	s.checksOnce.Do(func() {
		if s.passwordChecks == nil {
			s.passwordChecks = make(chan struct{}, 2)
		}
	})
	select {
	case s.passwordChecks <- struct{}{}:
		defer func() { <-s.passwordChecks }()
	default:
		apiError(w, http.StatusTooManyRequests, "try again later")
		return
	}
	if !s.allowLogin(input.Username, clientIP(r), now) {
		apiError(w, http.StatusTooManyRequests, "try again later")
		return
	}
	ctx, cancel := context.WithTimeout(r.Context(), 3*time.Second)
	defer cancel()
	secret, user, err := s.store.login(ctx, input.Username, input.Password, input.TOTP, now)
	if errors.Is(err, errInvalidCredentials) {
		apiError(w, http.StatusUnauthorized, "invalid credentials")
		return
	}
	if err != nil {
		apiError(w, http.StatusServiceUnavailable, "login unavailable")
		return
	}
	http.SetCookie(w, &http.Cookie{Name: s.brand.sessionCookie(), Value: base64.RawURLEncoding.EncodeToString(secret),
		Path: "/", Secure: true, HttpOnly: true, SameSite: http.SameSiteStrictMode,
		Expires: now.Add(12 * time.Hour), MaxAge: 12 * 60 * 60})
	apiJSON(w, http.StatusOK, map[string]any{"user": user, "csrf": csrfToken(s.brand, secret)})
}

type authorizedHandler func(http.ResponseWriter, *http.Request, adminUser, []byte)

func (s *adminServer) authorized(next authorizedHandler) http.HandlerFunc {
	return s.authorizedWithin(2*time.Second, next)
}

// authorizedWithin gives the session check and the handler one deadline.
func (s *adminServer) authorizedWithin(budget time.Duration, next authorizedHandler) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		cookie, err := r.Cookie(s.brand.sessionCookie())
		if err != nil {
			apiError(w, http.StatusUnauthorized, "admin session required")
			return
		}
		secret, err := base64.RawURLEncoding.DecodeString(cookie.Value)
		if err != nil || len(secret) != 32 {
			apiError(w, http.StatusUnauthorized, "admin session required")
			return
		}
		ctx, cancel := context.WithTimeout(r.Context(), budget)
		defer cancel()
		user, err := s.store.session(ctx, secret, s.clock())
		if errors.Is(err, errNoSession) {
			apiError(w, http.StatusUnauthorized, "admin session required")
			return
		}
		if err != nil {
			apiError(w, http.StatusServiceUnavailable, "session unavailable")
			return
		}
		next(w, r.WithContext(ctx), user, secret)
	}
}

func (s *adminServer) session(w http.ResponseWriter, _ *http.Request, user adminUser, secret []byte) {
	apiJSON(w, http.StatusOK, map[string]any{"user": user, "csrf": csrfToken(s.brand, secret)})
}

func (s *adminServer) logout(w http.ResponseWriter, r *http.Request, _ adminUser, secret []byte) {
	if !validOrigin(r, s.origin) || !validCSRF(s.brand, secret, r.Header.Get("X-CSRF-Token")) {
		apiError(w, http.StatusForbidden, "request confirmation rejected")
		return
	}
	if err := s.store.logout(r.Context(), secret, s.clock()); err != nil {
		apiError(w, http.StatusServiceUnavailable, "logout unavailable")
		return
	}
	http.SetCookie(w, &http.Cookie{Name: s.brand.sessionCookie(), Path: "/", Secure: true, HttpOnly: true,
		SameSite: http.SameSiteStrictMode, MaxAge: -1})
	w.WriteHeader(http.StatusNoContent)
}

func canReadPlayers(role string) bool {
	return role == "owner" || role == "operator" || role == "support"
}

func (s *adminServer) players(w http.ResponseWriter, r *http.Request, user adminUser, _ []byte) {
	if !canReadPlayers(user.Role) {
		apiError(w, http.StatusForbidden, "permission denied")
		return
	}
	query := r.URL.Query().Get("q")
	if len(query) > 64 {
		apiError(w, http.StatusBadRequest, "search query too long")
		return
	}
	after, err := strconv.ParseInt(r.URL.Query().Get("after"), 10, 64)
	if r.URL.Query().Get("after") == "" {
		after, err = 0, nil
	}
	if err != nil || after < 0 {
		apiError(w, http.StatusBadRequest, "invalid cursor")
		return
	}
	items, err := s.store.players(r.Context(), query, after, 50)
	if err != nil {
		apiError(w, http.StatusServiceUnavailable, "player search unavailable")
		return
	}
	var next int64
	if len(items) == 50 {
		next = items[len(items)-1].ID
	}
	apiJSON(w, http.StatusOK, map[string]any{"items": items, "next_cursor": next})
}

type playerCard struct {
	playerProjection
	Profile      *playerProfile `json:"profile"`
	ProfileError string         `json:"profile_error,omitempty"`
}

func (s *adminServer) player(w http.ResponseWriter, r *http.Request, user adminUser, _ []byte) {
	if !canReadPlayers(user.Role) {
		apiError(w, http.StatusForbidden, "permission denied")
		return
	}
	id, err := strconv.ParseInt(r.PathValue("id"), 10, 64)
	if err != nil || id < 1 {
		apiError(w, http.StatusBadRequest, "invalid player id")
		return
	}
	player, err := s.store.player(r.Context(), id)
	if errors.Is(err, errNoPlayer) {
		apiError(w, http.StatusNotFound, "player not found")
		return
	}
	if err != nil || s.store.auditPlayerRead(r.Context(), user.ID, id) != nil {
		apiError(w, http.StatusServiceUnavailable, "player unavailable")
		return
	}
	card := playerCard{playerProjection: player}
	// The account stays readable when its profile is not: the card says which.
	if card.Profile, err = s.store.playerProfile(r.Context(), id); err != nil {
		card.Profile = nil
		card.ProfileError = "profile unreadable"
		if !errors.Is(err, errProfileFormat) {
			card.ProfileError = "profile unavailable"
		}
	}
	apiJSON(w, http.StatusOK, card)
}

func (s *adminServer) overview(w http.ResponseWriter, r *http.Request, _ adminUser, _ []byte) {
	snapshot, err := s.meta.snapshot(r.Context())
	if err != nil {
		apiError(w, http.StatusServiceUnavailable, "metaserver snapshot unavailable")
		return
	}
	if err := s.includeKnownRooms(r.Context(), &snapshot); err != nil {
		snapshot.Error = appendSourceError(snapshot.Error, "known rooms unavailable")
	}
	apiJSON(w, http.StatusOK, snapshot)
}

func (s *adminServer) rooms(w http.ResponseWriter, r *http.Request, _ adminUser, _ []byte) {
	snapshot, err := s.meta.snapshot(r.Context())
	if err != nil {
		apiError(w, http.StatusServiceUnavailable, "rooms unavailable")
		return
	}
	if err := s.includeKnownRooms(r.Context(), &snapshot); err != nil {
		snapshot.Error = appendSourceError(snapshot.Error, "known rooms unavailable")
	}
	apiJSON(w, http.StatusOK, map[string]any{"rooms": snapshot.Rooms, "updated_at": snapshot.Now,
		"plans": snapshot.RoomPlans, "retired": snapshot.RoomsRetired, "source_error": snapshot.Error})
}

// includeKnownRooms adds the registry's rooms the live snapshot leaves out
// and judges every room against its machine's plan.
func (s *adminServer) includeKnownRooms(ctx context.Context, snapshot *metaSnapshot) error {
	known, err := s.store.knownRooms(ctx, snapshot.Now)
	if err != nil {
		return err
	}
	byID := make(map[int64]bool, len(snapshot.Rooms))
	for _, room := range snapshot.Rooms {
		byID[room.ID] = true
	}
	for _, room := range known {
		if !byID[room.ID] {
			snapshot.Rooms = append(snapshot.Rooms, room)
		}
	}
	// Without the plans the merged list stands as it is, unjudged.
	plans, err := s.store.roomPlans(ctx)
	if err != nil {
		snapshot.Error = appendSourceError(snapshot.Error, "room plans unavailable")
		return nil
	}
	snapshot.Rooms, snapshot.RoomPlans, snapshot.RoomsRetired = applyPlans(snapshot.Rooms, plans)
	return nil
}

func appendSourceError(current, next string) string {
	if current == "" {
		return next
	}
	return current + "; " + next
}

func (s *adminServer) occupancy(w http.ResponseWriter, r *http.Request, _ adminUser, _ []byte) {
	now := s.clock()
	hours, err := s.store.occupancyHistory(r.Context(), now)
	if err != nil {
		apiError(w, http.StatusServiceUnavailable, "occupancy history unavailable")
		return
	}
	apiJSON(w, http.StatusOK, map[string]any{"as_of": now, "source": "room_status",
		"definition": "peak connected seats in live rooms per UTC hour; one fleet sample per minute",
		"hours":      hours})
}

func (s *adminServer) capacity(w http.ResponseWriter, r *http.Request, _ adminUser, _ []byte) {
	span, err := parseCapacityRange(r.URL.Query().Get("range"), r.URL.Query().Get("step"))
	if err != nil {
		apiError(w, http.StatusBadRequest, err.Error())
		return
	}
	host := r.URL.Query().Get("host")
	if len(host) > 253 {
		apiError(w, http.StatusBadRequest, "host is too long")
		return
	}
	now := s.clock()
	since, newest, err := s.store.capacityBounds(r.Context())
	if err != nil {
		apiError(w, http.StatusServiceUnavailable, "capacity history unavailable")
		return
	}
	win := windowFor(span, now, newest)
	points, err := s.store.capacitySeries(r.Context(), span, win, host)
	if err != nil {
		apiError(w, http.StatusServiceUnavailable, "capacity history unavailable")
		return
	}
	out := map[string]any{"as_of": now, "range": span.name, "bucket_s": int(span.bucket / time.Second),
		"from": win.start, "to": win.end, "last_minute": win.last, "collecting_since": since, "host": host,
		"norms": map[string]any{"cpu_pct": cpuNormPct, "cpu_alarm_pct": cpuAlarmPct, "traffic_share": 0.8},
		"definition": "per minute: machines summed (seats, traffic) or averaged (CPU) over machines that reported; " +
			"per point: the mean or peak of its minutes; a point without minutes is missing, not zero",
		"points": points}
	if host == "" {
		machines, err := s.store.capacityMachines(r.Context(), win, now)
		if err != nil {
			apiError(w, http.StatusServiceUnavailable, "capacity history unavailable")
			return
		}
		endpoints, err := s.store.capacityEndpoints(r.Context(), win)
		if err != nil {
			apiError(w, http.StatusServiceUnavailable, "capacity history unavailable")
			return
		}
		out["machines"], out["endpoints"] = machines, endpoints
		if snapshot, err := s.meta.snapshot(r.Context()); err != nil {
			out["traffic_error"] = "metaserver snapshot unavailable"
		} else {
			out["traffic"] = map[string]any{"month_allowance_gb": snapshot.TrafficMonthGB, "hosts": snapshot.Traffic}
		}
	}
	apiJSON(w, http.StatusOK, out)
}

// incidents is the metaserver's live picture of what went wrong: alerts,
// error groups and events of the last 24 hours, requests by route, database
// and room API failures, and the rooms' certificates.
func (s *adminServer) incidents(w http.ResponseWriter, r *http.Request, _ adminUser, _ []byte) {
	view, err := s.meta.incidents(r.Context())
	if err != nil {
		apiError(w, http.StatusServiceUnavailable, "metaserver incidents unavailable")
		return
	}
	if view.Alerts == nil {
		view.Alerts = []metaAlert{}
	}
	if view.Certificates == nil {
		view.Certificates = []metaCertificate{}
	}
	apiJSON(w, http.StatusOK, map[string]any{"as_of": view.Now, "since": view.Since, "alerts": view.Alerts,
		"certificates": view.Certificates, "db": view.DB, "rooms_api": view.RoomsAPI,
		"observability": view.Observability})
}

// performance is the fleet, its machines and a summary of every room over
// the last minute, as the metaserver measured them.
func (s *adminServer) performance(w http.ResponseWriter, r *http.Request, _ adminUser, _ []byte) {
	view, err := s.meta.perf(r.Context())
	if err != nil {
		apiError(w, http.StatusServiceUnavailable, "room performance unavailable")
		return
	}
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.Write(view)
}

// room is one room's whole last minute; a room silent for a minute has none.
func (s *adminServer) room(w http.ResponseWriter, r *http.Request, _ adminUser, _ []byte) {
	id, err := strconv.ParseInt(r.PathValue("id"), 10, 64)
	if err != nil || id <= 0 || id > math.MaxUint32 {
		apiError(w, http.StatusBadRequest, "room id")
		return
	}
	view, err := s.meta.perfRoom(r.Context(), id)
	var refused metaStatusError
	if errors.As(err, &refused) && refused.code == http.StatusNotFound {
		apiError(w, http.StatusNotFound, "no measurements from this room in the last minute")
		return
	}
	if err != nil {
		apiError(w, http.StatusServiceUnavailable, "room performance unavailable")
		return
	}
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.Write(view)
}

func apiJSON(w http.ResponseWriter, code int, value any) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.WriteHeader(code)
	json.NewEncoder(w).Encode(value)
}

func apiError(w http.ResponseWriter, code int, message string) {
	apiJSON(w, code, map[string]string{"error": message})
}
