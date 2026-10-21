package metaserver

import (
	"context"
	"crypto/hmac"
	"crypto/rand"
	"crypto/sha256"
	"encoding/base64"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"net/url"
	"strconv"
	"strings"
	"testing"
	"time"

	"github.com/jackc/pgx/v5"
)

// Every test gets a freshly migrated database on the real PostgreSQL named
// by META_TEST_DATABASE_URL (default: the compose stand), dropped at the
// end. Without a reachable server the tests fail, not skip: this service
// is nothing without its database.
func freshStore(t *testing.T) (*store, string) {
	t.Helper()
	st, databaseURL := emptyStore(t)
	if err := st.migrate(context.Background()); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	return st, databaseURL
}

// emptyStore is a new database with no schema yet.
func emptyStore(t *testing.T) (*store, string) {
	t.Helper()
	adminURL := envOr("META_TEST_DATABASE_URL", defaultDatabaseURL)
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()
	admin, err := pgx.Connect(ctx, adminURL)
	if err != nil {
		t.Fatalf("connect to the test database: %v", err)
	}
	var suffix [4]byte
	rand.Read(suffix[:])
	name := "meta_test_" + hex.EncodeToString(suffix[:])
	if _, err := admin.Exec(ctx, fmt.Sprintf(`CREATE DATABASE %s`, name)); err != nil {
		t.Fatalf("create database: %v", err)
	}
	t.Cleanup(func() {
		ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
		defer cancel()
		admin.Exec(ctx, fmt.Sprintf(`DROP DATABASE %s WITH (FORCE)`, name))
		admin.Close(ctx)
	})
	parsed, _ := url.Parse(adminURL)
	parsed.Path = "/" + name
	return openTestStore(t, parsed.String()), parsed.String()
}

func openTestStore(t *testing.T, databaseURL string) *store {
	t.Helper()
	st, err := openStore(context.Background(), databaseURL)
	if err != nil {
		t.Fatalf("open store: %v", err)
	}
	t.Cleanup(st.close)
	return st
}

var testKey = func() ticketKey {
	var key ticketKey
	for i := range key {
		key[i] = byte(i)
	}
	return key
}()

func newTestAPI(t *testing.T, st *store) *Server {
	t.Helper()
	cfg := Config{TicketTTL: 30 * time.Second, CallsPerHour: 5, RoomURL: "ws://127.0.0.1:{port}/"}
	log := slog.New(slog.NewTextHandler(io.Discard, nil))
	if _, err := st.reportRoom(context.Background(), 9, 1, &roomStatus{V: statusVersion, Port: 7001, Free: 4, Capacity: 4, PeriodMs: 1000, Host: "room.test"}); err != nil {
		t.Fatal(err)
	}
	return newAPI(cfg, testKey, st, log)
}

func call(t *testing.T, h http.Handler, method, path, bearer string) (int, map[string]any) {
	t.Helper()
	req := httptest.NewRequest(method, path, nil)
	if bearer != "" {
		req.Header.Set("Authorization", "Bearer "+bearer)
	}
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, req)
	body := map[string]any{}
	if err := json.NewDecoder(rec.Body).Decode(&body); err != nil && rec.Code != http.StatusNoContent {
		t.Fatalf("%s %s: status %d, body not JSON: %v", method, path, rec.Code, err)
	}
	return rec.Code, body
}

func TestGuestBuysATicketAndSurvivesARestart(t *testing.T) {
	st, dbURL := freshStore(t)
	h := newTestAPI(t, st).handler(testRoutes)

	status, guest := call(t, h, "POST", "/api/guest", "")
	if status != http.StatusCreated {
		t.Fatalf("guest: %d %v", status, guest)
	}
	secret := guest["secret"].(string)
	playerID := int64(guest["player_id"].(float64))
	if len(secret) != 43 || playerID <= 0 {
		t.Fatalf("guest response %v", guest)
	}

	status, entry := call(t, h, "POST", "/api/enter", secret)
	if status != http.StatusOK || entry["url"] != "ws://127.0.0.1:7001/" || entry["room_id"].(float64) != 9 {
		t.Fatalf("enter: %d %v", status, entry)
	}
	raw, err := base64.RawURLEncoding.DecodeString(entry["ticket"].(string))
	if err != nil || len(raw) != ticketSize {
		t.Fatalf("ticket bytes: %v, %d", err, len(raw))
	}
	if raw[0] != ticketVersion || raw[1] != ticketKindPlayer || binary.LittleEndian.Uint64(raw[2:]) != uint64(playerID) ||
		binary.LittleEndian.Uint32(raw[10:]) != 9 ||
		int64(binary.LittleEndian.Uint32(raw[14:])) != int64(entry["expires_at"].(float64)) {
		t.Fatalf("ticket fields %x, body %v", raw, entry)
	}
	mac := hmac.New(sha256.New, testKey[:])
	mac.Write(raw[:ticketSignedSize])
	if !hmac.Equal(mac.Sum(nil)[:ticketMacSize], raw[ticketSignedSize:]) {
		t.Fatal("ticket mac does not verify under the shared key")
	}

	// The process restarts: a new store on the same database still knows
	// the guest, and only the secret opens it.
	restarted := newTestAPI(t, openTestStore(t, dbURL)).handler(testRoutes)
	if status, again := call(t, restarted, "POST", "/api/enter", secret); status != http.StatusOK ||
		binary.LittleEndian.Uint64(ticketBytes(t, again)[2:]) != uint64(playerID) {
		t.Fatalf("enter after restart: %d %v", status, again)
	}
	// The last base64url digit carries four data bits and two padding bits:
	// four places on in the alphabet changes the data bits, so it is another
	// secret whatever the digit was.
	const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"
	flipped := string(alphabet[(strings.IndexByte(alphabet, secret[42])+4)%64])
	for _, bad := range []string{"", "not-base64!", strings.Repeat("A", 43), secret[:42] + flipped} {
		if status, _ := call(t, restarted, "POST", "/api/enter", bad); status != http.StatusUnauthorized {
			t.Errorf("secret %q: status %d", bad, status)
		}
	}
	// No room with a seat is a refusal the client retries, not an error.
	a := newTestAPI(t, st)
	if _, err := st.reportRoom(context.Background(), 9, 1, &roomStatus{V: statusVersion, Port: 7001, Capacity: 4, PeriodMs: 1000, Host: "room.test"}); err != nil {
		t.Fatal(err)
	}
	if status, _ := call(t, a.handler(testRoutes), "POST", "/api/enter", secret); status != http.StatusServiceUnavailable {
		t.Fatalf("full pool: %d", status)
	}
}

func ticketBytes(t *testing.T, body map[string]any) []byte {
	t.Helper()
	raw, err := base64.RawURLEncoding.DecodeString(body["ticket"].(string))
	if err != nil {
		t.Fatal(err)
	}
	return raw
}

func TestCallsAreLimitedPerAddress(t *testing.T) {
	st, _ := freshStore(t)
	a := newTestAPI(t, st)
	now := time.Now()
	a.now = func() time.Time { return now }
	h := a.handler(testRoutes)
	for i := 0; i < 5; i++ {
		if status, _ := call(t, h, "POST", "/api/guest", ""); status != http.StatusCreated {
			t.Fatalf("call %d: %d", i, status)
		}
	}
	if status, _ := call(t, h, "POST", "/api/guest", ""); status != http.StatusTooManyRequests {
		t.Fatal("sixth call in the hour was not refused")
	}
	// Without a proxy the header is anyone's to forge and is ignored.
	forged := httptest.NewRequest("POST", "/api/guest", nil)
	forged.Header.Set("X-Forwarded-For", "10.0.0.1")
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, forged)
	if rec.Code != http.StatusTooManyRequests {
		t.Fatalf("forged header opened a new allowance: %d", rec.Code)
	}
	now = now.Add(time.Hour)
	if status, _ := call(t, h, "POST", "/api/guest", ""); status != http.StatusCreated {
		t.Fatal("the allowance did not refill")
	}
	rec = httptest.NewRecorder()
	h.ServeHTTP(rec, httptest.NewRequest("GET", "/readyz", nil))
	if rec.Code != http.StatusOK {
		t.Fatalf("readyz limited: %d", rec.Code)
	}

	// Behind the proxy the player is the last forwarded hop: two players
	// through one proxy get their own allowances, and a hop a client
	// forged before the proxy's counts for nothing.
	a.cfg.BehindProxy = true
	for _, xff := range []string{"10.0.0.1", "10.0.0.2", "1.2.3.4, 10.0.0.2"} {
		for i := 0; i < 5; i++ {
			req := httptest.NewRequest("POST", "/api/guest", nil)
			req.Header.Set("X-Forwarded-For", xff)
			rec = httptest.NewRecorder()
			h.ServeHTTP(rec, req)
			want := http.StatusCreated
			if strings.HasPrefix(xff, "1.2.3.4") {
				want = http.StatusTooManyRequests
			}
			if rec.Code != want {
				t.Fatalf("forwarded %q call %d: %d, want %d", xff, i, rec.Code, want)
			}
		}
	}
	rec = httptest.NewRecorder()
	h.ServeHTTP(rec, httptest.NewRequest("OPTIONS", "/api/enter", nil))
	if rec.Code != http.StatusNoContent || rec.Header().Get("Access-Control-Allow-Origin") != "*" {
		t.Fatalf("preflight: %d %v", rec.Code, rec.Header())
	}
	rec = httptest.NewRecorder()
	h.ServeHTTP(rec, httptest.NewRequest("POST", "/api/nowhere", nil))
	if rec.Code != http.StatusNotFound {
		t.Fatalf("unknown path: %d", rec.Code)
	}
}

// The operator's load generators skip the per-address budget; everyone
// else behind the same proxy keeps theirs.
func TestUnlimitedAddressesSkipTheAddressBudget(t *testing.T) {
	st, _ := freshStore(t)
	a := newTestAPI(t, st)
	a.cfg.BehindProxy = true
	nets, err := parseAddrs("203.0.113.7, 198.51.100.0/24")
	if err != nil {
		t.Fatal(err)
	}
	a.cfg.UnlimitedAddrs = nets
	h := a.handler(testRoutes)
	guest := func(xff string) int {
		req := httptest.NewRequest("POST", "/api/guest", nil)
		req.Header.Set("X-Forwarded-For", xff)
		rec := httptest.NewRecorder()
		h.ServeHTTP(rec, req)
		return rec.Code
	}
	for _, xff := range []string{"203.0.113.7", "198.51.100.42", "::ffff:203.0.113.7"} {
		for i := 0; i < 8; i++ {
			if code := guest(xff); code != http.StatusCreated {
				t.Fatalf("%s call %d: %d", xff, i, code)
			}
		}
	}
	for i := 0; i < 5; i++ {
		guest("203.0.113.8")
	}
	if code := guest("203.0.113.8"); code != http.StatusTooManyRequests {
		t.Fatalf("a neighbour of the load generator got %d", code)
	}
	if _, err := parseAddrs("203.0.113.7, nonsense"); err == nil {
		t.Fatal("a bad entry was accepted")
	}
}

// Entries are metered per session, not per address: a crowd behind one
// router enters freely, one session looping is stopped, and a spray of
// made-up bearers cannot lock the honest ones out.
func TestEntriesAreMeteredPerSession(t *testing.T) {
	st, _ := freshStore(t)
	a := newTestAPI(t, st)
	a.cfg.CallsPerHour = 2
	a.calls = newLimiter(2, time.Hour)
	h := a.handler(testRoutes)
	_, guest := call(t, h, "POST", "/api/guest", "")
	secret := guest["secret"].(string)
	// The address's two guests are spent; entries from it still flow.
	call(t, h, "POST", "/api/guest", "")
	if status, _ := call(t, h, "POST", "/api/guest", ""); status != http.StatusTooManyRequests {
		t.Fatalf("third guest: %d", status)
	}
	// Every entry books a seat, so the room needs more than the burst.
	if _, err := st.reportRoom(context.Background(), 9, 1, &roomStatus{V: statusVersion, Port: 7001, Free: 100, Capacity: 128, PeriodMs: 1000, Host: "room.test"}); err != nil {
		t.Fatal(err)
	}
	for i := 0; i < sessionBurst; i++ {
		if status, _ := call(t, h, "POST", "/api/enter", secret); status != http.StatusOK {
			t.Fatalf("entry %d: %d", i, status)
		}
	}
	if status, _ := call(t, h, "POST", "/api/enter", secret); status != http.StatusTooManyRequests {
		t.Fatal("a session in a loop was not stopped")
	}
	// Another session from the same address is its own budget.
	for i := 0; i < 10001; i++ {
		a.sessionCalls.allow("fake-"+strconv.Itoa(i), a.now())
	}
	other := "other-session-bearer"
	if !a.sessionCalls.allow(other, a.now()) {
		t.Fatal("a full map of fakes locked a new session out")
	}
	// A long bearer buys no long key.
	req := httptest.NewRequest("POST", "/api/enter", nil)
	req.Header.Set("Authorization", strings.Repeat("x", 5000))
	if key := sessionKey(req); len(key) != sessionKeyMax {
		t.Fatalf("session key of %d bytes", len(key))
	}
}

// Entries from one address are capped at a rate no venue reaches, so a
// spray of made-up sessions cannot keep the store looking them up.
func TestEntriesHaveAnAddressCeiling(t *testing.T) {
	st, _ := freshStore(t)
	a := newTestAPI(t, st)
	a.entries = newLimiter(5, time.Hour)
	h := a.handler(testRoutes)
	for i := 0; i < 5; i++ {
		if status, _ := call(t, h, "POST", "/api/enter", "made-up-"+strconv.Itoa(i)); status != http.StatusUnauthorized {
			t.Fatalf("made-up session %d: %d", i, status)
		}
	}
	if status, _ := call(t, h, "POST", "/api/enter", "made-up-5"); status != http.StatusTooManyRequests {
		t.Fatalf("past the ceiling: %d", status)
	}
}

// testRoutes is the route table a game mounts, as the games' own mains do.
func testRoutes(s *Server, mux *http.ServeMux) {
	mux.HandleFunc("POST /api/guest", s.Player(PerAddress, "guest", s.Guest))
	mux.HandleFunc("POST "+PathEnter, s.Player(PerSession, "enter", s.Enter))
	mux.HandleFunc("POST /api/register", s.Player(PerAddress, "register", s.Register))
	mux.HandleFunc("POST /api/login", s.Player(PerAddress, "login", s.Login))
	mux.HandleFunc("POST /api/password", s.Player(PerAddress, "password", s.Password))
	mux.HandleFunc("POST /api/logout", s.Player(PerSession, "logout", s.Logout))
	mux.HandleFunc("POST /rooms/status", s.Timed("status", s.RoomStatus))
	mux.HandleFunc("POST /rooms/profiles/{player}/load", s.Timed("load", s.ProfileLoad))
	mux.HandleFunc("POST /rooms/profiles/{player}/save", s.Timed("save", s.ProfileSave))
	mux.HandleFunc("POST /rooms/profiles/save", s.Timed("saves", s.ProfileSaves))
}
