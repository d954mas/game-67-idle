package admin

import (
	"bytes"
	"encoding/base64"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strconv"
	"testing"
	"time"
)

func TestAdminAPIRequiresSessionAndCSRF(t *testing.T) {
	store := freshAdminStore(t)
	password := "owner password for API test"
	hash, _ := hashPassword(password)
	secret := []byte("12345678901234567890")
	encrypted, _ := encryptSecret(store.totpKey, secret)
	if _, err := store.pool.Exec(t.Context(), `INSERT INTO ops.admin_users
		(username, password_hash, totp_secret, role) VALUES ('owner', $1, $2, 'owner')`, hash, encrypted); err != nil {
		t.Fatal(err)
	}
	if _, err := store.pool.Exec(t.Context(), `INSERT INTO ops.admin_users
		(username, password_hash, totp_secret, role) VALUES ('viewer', $1, $2, 'viewer')`, hash, encrypted); err != nil {
		t.Fatal(err)
	}
	if _, err := store.pool.Exec(t.Context(), `INSERT INTO players (login) VALUES ('ann')`); err != nil {
		t.Fatal(err)
	}
	now := time.Unix(1_780_000_000, 0).UTC()
	api := &adminServer{store: store, origin: "https://admin.example.test", totpRequired: true,
		now: func() time.Time { return now }}
	h := api.handler()

	unauthorized := httptest.NewRecorder()
	h.ServeHTTP(unauthorized, httptest.NewRequest("GET", "/admin-api/v1/players?q=ann", nil))
	if unauthorized.Code != http.StatusUnauthorized {
		t.Fatalf("unauthenticated players: %d", unauthorized.Code)
	}
	viewerSecret, _, err := store.login(t.Context(), "viewer", password, totpCode(secret, now.Unix()/30), now)
	if err != nil {
		t.Fatal(err)
	}
	viewerReq := httptest.NewRequest("GET", "/admin-api/v1/players?q=ann", nil)
	viewerReq.AddCookie(&http.Cookie{Name: api.brand.sessionCookie(), Value: base64.RawURLEncoding.EncodeToString(viewerSecret)})
	viewer := httptest.NewRecorder()
	h.ServeHTTP(viewer, viewerReq)
	if viewer.Code != http.StatusForbidden {
		t.Fatalf("viewer reached player list: %d", viewer.Code)
	}
	occupancyReq := httptest.NewRequest("GET", "/admin-api/v1/audience/occupancy", nil)
	occupancyReq.AddCookie(&http.Cookie{Name: api.brand.sessionCookie(), Value: base64.RawURLEncoding.EncodeToString(viewerSecret)})
	occupancy := httptest.NewRecorder()
	h.ServeHTTP(occupancy, occupancyReq)
	if occupancy.Code != http.StatusOK || !bytes.Contains(occupancy.Body.Bytes(), []byte(`"hours":[]`)) {
		t.Fatalf("viewer occupancy: %d %s", occupancy.Code, occupancy.Body.String())
	}
	for target, want := range map[string]int{"/admin-api/v1/capacity?range=7d": http.StatusOK,
		"/admin-api/v1/capacity?range=1y": http.StatusBadRequest, "/admin-api/v1/capacity?range=30d&step=60": http.StatusBadRequest,
		"/admin-api/v1/incidents": http.StatusServiceUnavailable, "/admin-api/v1/performance": http.StatusServiceUnavailable,
		"/admin-api/v1/rooms/x": http.StatusBadRequest, "/admin-api/v1/rooms/0": http.StatusBadRequest,
		"/admin-api/v1/rooms/5": http.StatusServiceUnavailable} {
		req := httptest.NewRequest("GET", target, nil)
		req.AddCookie(&http.Cookie{Name: api.brand.sessionCookie(), Value: base64.RawURLEncoding.EncodeToString(viewerSecret)})
		got := httptest.NewRecorder()
		h.ServeHTTP(got, req)
		if got.Code != want {
			t.Fatalf("viewer %s: %d %s", target, got.Code, got.Body.String())
		}
		if want == http.StatusOK && (!bytes.Contains(got.Body.Bytes(), []byte(`"collecting_since":null`)) ||
			!bytes.Contains(got.Body.Bytes(), []byte(`"traffic_error"`))) {
			t.Fatalf("empty capacity: %s", got.Body.String())
		}
	}
	loginBody, _ := json.Marshal(map[string]string{"username": "owner", "password": password,
		"totp": totpCode(secret, now.Unix()/30)})
	loginReq := httptest.NewRequest("POST", "/admin-api/v1/login", bytes.NewReader(loginBody))
	loginReq.Header.Set("Origin", "https://admin.example.test")
	loginReq.Header.Set("Content-Type", "application/json")
	login := httptest.NewRecorder()
	h.ServeHTTP(login, loginReq)
	if login.Code != http.StatusOK {
		t.Fatalf("login: %d %s", login.Code, login.Body.String())
	}
	cookies := login.Result().Cookies()
	if len(cookies) != 1 || cookies[0].Name != api.brand.sessionCookie() || !cookies[0].Secure || !cookies[0].HttpOnly ||
		cookies[0].SameSite != http.SameSiteStrictMode || cookies[0].Path != "/" || cookies[0].Domain != "" {
		t.Fatalf("session cookie: %+v", cookies)
	}
	playersReq := httptest.NewRequest("GET", "/admin-api/v1/players?q=ann", nil)
	playersReq.AddCookie(cookies[0])
	players := httptest.NewRecorder()
	h.ServeHTTP(players, playersReq)
	if players.Code != http.StatusOK || !bytes.Contains(players.Body.Bytes(), []byte(`"ann"`)) {
		t.Fatalf("authorized players: %d %s", players.Code, players.Body.String())
	}
	sessionReq := httptest.NewRequest("GET", "/admin-api/v1/session", nil)
	sessionReq.AddCookie(cookies[0])
	session := httptest.NewRecorder()
	h.ServeHTTP(session, sessionReq)
	var sessionBody struct {
		CSRF string `json:"csrf"`
	}
	if session.Code != http.StatusOK || json.Unmarshal(session.Body.Bytes(), &sessionBody) != nil || sessionBody.CSRF == "" {
		t.Fatalf("session response: %d %s", session.Code, session.Body.String())
	}
	logoutReq := httptest.NewRequest("POST", "/admin-api/v1/logout", nil)
	logoutReq.Header.Set("Origin", "https://admin.example.test")
	logoutReq.AddCookie(cookies[0])
	logout := httptest.NewRecorder()
	h.ServeHTTP(logout, logoutReq)
	if logout.Code != http.StatusForbidden {
		t.Fatalf("logout without CSRF: %d", logout.Code)
	}
	logoutReq.Header.Set("X-CSRF-Token", sessionBody.CSRF)
	logout = httptest.NewRecorder()
	h.ServeHTTP(logout, logoutReq)
	if logout.Code != http.StatusNoContent {
		t.Fatalf("logout with CSRF: %d %s", logout.Code, logout.Body.String())
	}
	session = httptest.NewRecorder()
	h.ServeHTTP(session, sessionReq)
	if session.Code != http.StatusUnauthorized {
		t.Fatalf("revoked cookie: %d", session.Code)
	}
}

func TestLoginRejectsOversizedBody(t *testing.T) {
	h := (&adminServer{origin: "https://admin.example.test"}).handler()
	body := []byte(`{"username":"owner","password":"password","totp":"123456"}`)
	body = append(body, bytes.Repeat([]byte(" "), 4096)...)
	r := httptest.NewRequest("POST", "/admin-api/v1/login", bytes.NewReader(body))
	r.Header.Set("Origin", "https://admin.example.test")
	r.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	h.ServeHTTP(w, r)
	if w.Code != http.StatusBadRequest {
		t.Fatalf("oversized login body accepted: %d", w.Code)
	}
}

func TestPasswordOnlyLoginAPIHasNoCodeField(t *testing.T) {
	store := freshAdminStore(t)
	store.totpRequired = false
	password := "owner password for API test"
	hash, _ := hashPassword(password)
	encrypted, _ := encryptSecret(store.totpKey, []byte("12345678901234567890"))
	if _, err := store.pool.Exec(t.Context(), `INSERT INTO ops.admin_users
		(username, password_hash, totp_secret, role) VALUES ('owner', $1, $2, 'owner')`, hash, encrypted); err != nil {
		t.Fatal(err)
	}
	api := &adminServer{store: store, origin: "https://admin.example.test", totpRequired: false}
	h := api.handler()
	config := httptest.NewRecorder()
	h.ServeHTTP(config, httptest.NewRequest("GET", "/admin-api/v1/auth-config", nil))
	if config.Code != http.StatusOK || !bytes.Contains(config.Body.Bytes(), []byte(`"totp_required":false`)) {
		t.Fatalf("auth config: %d %s", config.Code, config.Body.String())
	}
	body, _ := json.Marshal(map[string]string{"username": "owner", "password": password})
	r := httptest.NewRequest("POST", "/admin-api/v1/login", bytes.NewReader(body))
	r.Header.Set("Origin", "https://admin.example.test")
	r.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	h.ServeHTTP(w, r)
	if w.Code != http.StatusOK || len(w.Result().Cookies()) != 1 {
		t.Fatalf("password-only API login: %d %s", w.Code, w.Body.String())
	}
}

func TestAdminStaticAssetsAreServedWithoutGameData(t *testing.T) {
	h := (&adminServer{brand: Brand{Name: "Example <Game>", Mark: "EG"}}).handler()
	page := httptest.NewRecorder()
	h.ServeHTTP(page, httptest.NewRequest("GET", "/", nil))
	if page.Code != http.StatusOK || !bytes.Contains(page.Body.Bytes(), []byte("<title>Example &lt;Game&gt; · сервер</title>")) ||
		!bytes.Contains(page.Body.Bytes(), []byte(`aria-hidden="true">EG</span>`)) {
		t.Fatalf("branded page: %d", page.Code)
	}
	for path := range webPaths {
		response := httptest.NewRecorder()
		h.ServeHTTP(response, httptest.NewRequest("GET", path, nil))
		if response.Code != http.StatusOK || len(response.Body.Bytes()) == 0 ||
			bytes.Contains(response.Body.Bytes(), []byte("password_hash")) {
			t.Fatalf("asset %s: %d", path, response.Code)
		}
	}
	unknown := httptest.NewRecorder()
	h.ServeHTTP(unknown, httptest.NewRequest("GET", "/admin-api/v1/unknown", nil))
	if unknown.Code != http.StatusNotFound {
		t.Fatalf("unknown API path: %d", unknown.Code)
	}
}

func TestLoginRejectsBurstBeforePasswordCheck(t *testing.T) {
	api := &adminServer{origin: "https://admin.example.test", passwordChecks: make(chan struct{}, 2)}
	api.passwordChecks <- struct{}{}
	api.passwordChecks <- struct{}{}
	r := httptest.NewRequest("POST", "/admin-api/v1/login", bytes.NewBufferString(`{"username":"owner","password":"guess"}`))
	r.Header.Set("Origin", api.origin)
	r.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	api.handler().ServeHTTP(w, r)
	if w.Code != http.StatusTooManyRequests {
		t.Fatalf("busy login: %d %s", w.Code, w.Body.String())
	}
}

func TestLoginLimitsClientAndAccount(t *testing.T) {
	api := &adminServer{}
	now := time.Unix(1_780_000_000, 0)
	for i := 0; i < 20; i++ {
		if !api.allowLogin("user"+strconv.Itoa(i), "192.0.2.1", now) {
			t.Fatalf("client attempt %d rejected early", i)
		}
	}
	if api.allowLogin("next", "192.0.2.1", now) {
		t.Fatal("client limit did not apply")
	}
	for i := 0; i < 10; i++ {
		if !api.allowLogin("owner", "198.51.100."+strconv.Itoa(i), now) {
			t.Fatalf("account attempt %d rejected early", i)
		}
	}
	if api.allowLogin("owner", "203.0.113.1", now.Add(2*time.Minute)) {
		t.Fatal("account cooldown expired too early")
	}
	if !api.allowLogin("owner", "203.0.113.1", now.Add(5*time.Minute+time.Second)) {
		t.Fatal("account cooldown did not expire")
	}
}

func TestLoginClientIPIgnoresForwardedPrefix(t *testing.T) {
	r := httptest.NewRequest("POST", "/admin-api/v1/login", nil)
	r.Header.Add("X-Forwarded-For", "192.0.2.10")
	r.Header.Add("X-Forwarded-For", "203.0.113.20, 198.51.100.30")
	if got := clientIP(r); got != "198.51.100.30" {
		t.Fatalf("client IP: %q", got)
	}
}
