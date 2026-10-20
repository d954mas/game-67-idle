package metaserver

import (
	"bytes"
	"context"
	"encoding/binary"
	"encoding/json"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"
)

// The account tests make many calls from one address, so the per-address
// allowance is not what they measure.
func newAccountsAPI(t *testing.T, st *store) *Server {
	t.Helper()
	cfg := Config{TicketTTL: 30 * time.Second, CallsPerHour: 100000, RoomURL: "ws://127.0.0.1:{port}/"}
	if _, err := st.reportRoom(context.Background(), 9, 1, &roomStatus{V: statusVersion, Port: 7001, Free: 4, Capacity: 4, PeriodMs: 1000, Host: "room.test"}); err != nil {
		t.Fatal(err)
	}
	return newAPI(cfg, testKey, st, slog.New(slog.NewTextHandler(io.Discard, nil)))
}

func post(t *testing.T, h http.Handler, path, bearer string, body any) (int, map[string]any, string) {
	t.Helper()
	var reader io.Reader
	if body != nil {
		raw, _ := json.Marshal(body)
		reader = bytes.NewReader(raw)
	}
	req := httptest.NewRequest("POST", path, reader)
	if bearer != "" {
		req.Header.Set("Authorization", "Bearer "+bearer)
	}
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, req)
	text := rec.Body.String()
	answer := map[string]any{}
	if text != "" {
		if err := json.Unmarshal([]byte(text), &answer); err != nil {
			t.Fatalf("%s: status %d, body not JSON: %s", path, rec.Code, text)
		}
	}
	return rec.Code, answer, text
}

func guest(t *testing.T, h http.Handler) (int64, string) {
	t.Helper()
	status, body, _ := post(t, h, "/api/guest", "", nil)
	if status != http.StatusCreated {
		t.Fatalf("guest: %d %v", status, body)
	}
	return int64(body["player_id"].(float64)), body["secret"].(string)
}

func enteredAs(t *testing.T, h http.Handler, secret string) (int, int64, string) {
	t.Helper()
	status, body, _ := post(t, h, "/api/enter", secret, nil)
	if status != http.StatusOK {
		return status, 0, ""
	}
	ticket := ticketBytes(t, body)
	return status, int64(binary.LittleEndian.Uint64(ticket[2:])), body["login"].(string)
}

func TestRegistrationBindsTheGuestAndLoginOpensItElsewhere(t *testing.T) {
	st, _ := freshStore(t)
	h := newAccountsAPI(t, st).handler(testRoutes)
	id, device1 := guest(t, h)

	creds := credentials{Login: "Nick_01", Password: "correct horse"}
	if status, body, _ := post(t, h, "/api/register", device1, creds); status != http.StatusOK || body["login"] != "nick_01" {
		t.Fatalf("register: %d %v", status, body)
	}
	// The registering device keeps its session and now sees its login.
	if status, player, login := enteredAs(t, h, device1); status != http.StatusOK || player != id || login != "nick_01" {
		t.Fatalf("enter after register: %d player %d login %q", status, player, login)
	}
	if status, body, _ := post(t, h, "/api/register", device1, credentials{Login: "other", Password: "correct horse"}); status != http.StatusConflict || body["error"] != "registered" {
		t.Fatalf("second registration: %d %v", status, body)
	}

	// Another guest cannot take the login, whatever its case.
	_, device2 := guest(t, h)
	if status, body, _ := post(t, h, "/api/register", device2, credentials{Login: "NICK_01", Password: "another one"}); status != http.StatusConflict || body["error"] != "taken" {
		t.Fatalf("duplicate login: %d %v", status, body)
	}

	// Login on a second device opens the same player, and its guest session
	// is left behind: the guest secret no longer enters.
	status, body, _ := post(t, h, "/api/login", device2, credentials{Login: " NICK_01 ", Password: "correct horse"})
	if status != http.StatusOK || body["login"] != "nick_01" || int64(body["player_id"].(float64)) != id {
		t.Fatalf("login: %d %v", status, body)
	}
	device2Account := body["secret"].(string)
	if status, player, _ := enteredAs(t, h, device2Account); status != http.StatusOK || player != id {
		t.Fatalf("enter as the account elsewhere: %d player %d", status, player)
	}
	if status, _, _ := enteredAs(t, h, device2); status != http.StatusUnauthorized {
		t.Fatalf("the guest session left behind still enters: %d", status)
	}

	// Rules: the same answer for every malformed pair, before any lookup;
	// the longest allowed login and password go through.
	_, device3 := guest(t, h)
	for _, bad := range []credentials{{"ab", "correct horse"}, {strings.Repeat("a", loginMax+1), "correct horse"},
		{"nick-01", "correct horse"}, {"nick_01", "short"}, {"nick_01", strings.Repeat("p", passwordMax+1)}} {
		if status, body, _ := post(t, h, "/api/login", "", bad); status != http.StatusBadRequest || body["error"] != "rules" {
			t.Errorf("login %v: %d %v", bad, status, body)
		}
		if status, body, _ := post(t, h, "/api/register", device3, bad); status != http.StatusBadRequest || body["error"] != "rules" {
			t.Errorf("register %v: %d %v", bad, status, body)
		}
	}
	longest := credentials{strings.Repeat("l", loginMax), strings.Repeat("p", passwordMax)}
	if status, _, _ := post(t, h, "/api/register", device3, longest); status != http.StatusOK {
		t.Errorf("longest login and password refused: %d", status)
	}
	if status, _, _ := post(t, h, "/api/login", "", longest); status != http.StatusOK {
		t.Errorf("longest login and password do not log in: %d", status)
	}
	if status, _, _ := post(t, h, "/api/register", "", creds); status != http.StatusUnauthorized {
		t.Errorf("register without a session: %d", status)
	}
}

func TestWrongPasswordsAreCountedAndTellNoLoginsApart(t *testing.T) {
	st, _ := freshStore(t)
	a := newAccountsAPI(t, st)
	now := time.Now()
	a.now = func() time.Time { return now }
	h := a.handler(testRoutes)
	_, device := guest(t, h)
	if status, _, _ := post(t, h, "/api/register", device, credentials{"nick", "correct horse"}); status != http.StatusOK {
		t.Fatalf("register: %d", status)
	}

	_, _, known := post(t, h, "/api/login", "", credentials{"nick", "wrong password"})
	status, _, unknown := post(t, h, "/api/login", "", credentials{"nobody", "wrong password"})
	if status != http.StatusForbidden || known != unknown {
		t.Fatalf("unknown login answers differently: %d %q vs %q", status, unknown, known)
	}

	// Every wrong answer counts against the name, whatever its case and
	// whether it exists, and the allowance runs out before the next lookup.
	for i := 1; i < attemptBurst; i++ {
		for _, login := range []string{"NICK", "nobody"} {
			if status, _, _ := post(t, h, "/api/login", "", credentials{login, "wrong password"}); status != http.StatusForbidden {
				t.Fatalf("attempt %d on %s: %d", i, login, status)
			}
		}
	}
	for _, c := range []credentials{{"nick", "wrong password"}, {"nick", "correct horse"}, {"nobody", "wrong password"}} {
		if status, body, _ := post(t, h, "/api/login", "", c); status != http.StatusTooManyRequests || body["error"] != "attempts" {
			t.Fatalf("over the allowance: %d %v", status, body)
		}
	}
	now = now.Add(attemptWindow)
	if status, _, _ := post(t, h, "/api/login", "", credentials{"nick", "correct horse"}); status != http.StatusOK {
		t.Fatalf("after the window: %d", status)
	}

	// Wrong old passwords count against the session, not the name, so a
	// locked-out name can still be changed from a logged-in device, and a
	// busy server spends nothing.
	for i := 0; i < attemptBurst; i++ {
		if status, _, _ := post(t, h, "/api/login", "", credentials{"nick", "wrong password"}); status != http.StatusForbidden {
			t.Fatalf("lockout attempt %d: %d", i, status)
		}
	}
	for i := 0; i < attemptBurst-1; i++ {
		if status, _, _ := post(t, h, "/api/password", device, passwordChange{"not it", "battery staple"}); status != http.StatusForbidden {
			t.Fatalf("wrong old %d: %d", i, status)
		}
	}
	for i := 0; i < cap(a.hashes); i++ {
		a.hashes <- struct{}{}
	}
	if status, body, _ := post(t, h, "/api/password", device, passwordChange{"correct horse", "battery staple"}); status != http.StatusServiceUnavailable || body["error"] != "busy" {
		t.Fatalf("busy server: %d %v", status, body)
	}
	for i := 0; i < cap(a.hashes); i++ {
		<-a.hashes
	}
	if status, _, _ := post(t, h, "/api/password", device, passwordChange{"correct horse", "battery staple"}); status != http.StatusNoContent {
		t.Fatalf("change on a locked-out name: %d", status)
	}
	if status, _, _ := post(t, h, "/api/password", device, passwordChange{"battery staple", "correct horse"}); status != http.StatusNoContent {
		t.Fatalf("the match did not refill the session's allowance: %d", status)
	}
}

func TestPasswordChangeRevokesEveryOtherSession(t *testing.T) {
	st, _ := freshStore(t)
	h := newAccountsAPI(t, st).handler(testRoutes)
	id, device1 := guest(t, h)
	if status, _, _ := post(t, h, "/api/password", device1, passwordChange{"x", "correct horse"}); status != http.StatusConflict {
		t.Fatalf("a guest has no password to change: %d", status)
	}
	if status, _, _ := post(t, h, "/api/register", device1, credentials{"nick", "correct horse"}); status != http.StatusOK {
		t.Fatalf("register: %d", status)
	}
	_, body, _ := post(t, h, "/api/login", "", credentials{"nick", "correct horse"})
	device2 := body["secret"].(string)

	if status, body, _ := post(t, h, "/api/password", device1, passwordChange{"not it", "battery staple"}); status != http.StatusForbidden || body["error"] != "wrong" {
		t.Fatalf("wrong old password: %d %v", status, body)
	}
	if status, _, _ := post(t, h, "/api/password", device1, passwordChange{"correct horse", "short"}); status != http.StatusBadRequest {
		t.Fatalf("new password rules: %d", status)
	}
	if status, _, _ := post(t, h, "/api/password", device1, passwordChange{"correct horse", "battery staple"}); status != http.StatusNoContent {
		t.Fatalf("change: %d", status)
	}
	if status, player, _ := enteredAs(t, h, device1); status != http.StatusOK || player != id {
		t.Fatalf("the changing device lost its session: %d", status)
	}
	if status, _, _ := enteredAs(t, h, device2); status != http.StatusUnauthorized {
		t.Fatalf("the other device survived the change: %d", status)
	}
	if status, _, _ := post(t, h, "/api/login", "", credentials{"nick", "correct horse"}); status != http.StatusForbidden {
		t.Fatalf("the old password still opens the account: %d", status)
	}
	if status, _, _ := post(t, h, "/api/login", "", credentials{"nick", "battery staple"}); status != http.StatusOK {
		t.Fatalf("the new password does not: %d", status)
	}

	// Logout ends this session only.
	if status, _, _ := post(t, h, "/api/logout", device1, nil); status != http.StatusNoContent {
		t.Fatalf("logout: %d", status)
	}
	for _, path := range []string{"/api/enter", "/api/logout", "/api/password"} {
		if status, body, _ := post(t, h, path, device1, passwordChange{"battery staple", "correct horse"}); status != http.StatusUnauthorized || body["error"] != "unknown_session" {
			t.Errorf("%s after logout: %d %v", path, status, body)
		}
	}
	// A bearer that is no secret at all is not a forgotten session.
	for _, bearer := range []string{"", "not a secret!"} {
		if status, body, _ := post(t, h, "/api/enter", bearer, nil); status != http.StatusUnauthorized || body["error"] != "session" {
			t.Errorf("enter with bearer %q: %d %v", bearer, status, body)
		}
	}
}
