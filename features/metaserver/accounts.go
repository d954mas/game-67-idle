package metaserver

import (
	"encoding/hex"
	"encoding/json"
	"errors"
	"net/http"
	"runtime"
	"strings"
	"time"

	"golang.org/x/crypto/bcrypt"
)

// Accounts: a login and a password attached to a guest's player. There is
// no email and no recovery; the password is the only key, and every device
// holds its own session secret.
const (
	loginMin    = 3
	loginMax    = 20
	passwordMin = 8
	passwordMax = 72 // bcrypt reads no further

	// Wrong passwords per login name before a pause; a match refills it.
	attemptBurst  = 10
	attemptWindow = 15 * time.Minute
	// A caller waits this long for a bcrypt slot before 503: a launch
	// burst is a moment's queue, a flood is still refused.
	hashingWait = 200 * time.Millisecond
)

// hashingSlots is the bcrypt calls in flight at once: one per core but
// one, so a login burst cannot take every core from the player API, and
// never fewer than two.
func hashingSlots() int {
	if n := runtime.NumCPU() - 1; n > 2 {
		return n
	}
	return 2
}

// normalizeLogin lowercases ASCII and accepts [a-z0-9_]{3,20}; the stored
// form is the normalized one.
func normalizeLogin(raw string) (string, bool) {
	login := strings.ToLower(strings.TrimSpace(raw))
	if len(login) < loginMin || len(login) > loginMax {
		return "", false
	}
	for _, c := range login {
		if !(c >= 'a' && c <= 'z' || c >= '0' && c <= '9' || c == '_') {
			return "", false
		}
	}
	return login, true
}

func passwordOK(password string) bool {
	return len(password) >= passwordMin && len(password) <= passwordMax
}

// readJSON bounds the body and rejects anything that is not the expected
// shape with the same "rules" answer a bad login gets.
func (a *Server) readJSON(w http.ResponseWriter, r *http.Request, dst any) bool {
	r.Body = http.MaxBytesReader(w, r.Body, 1024)
	if err := json.NewDecoder(r.Body).Decode(dst); err != nil {
		writeError(w, http.StatusBadRequest, "rules")
		return false
	}
	return true
}

// session resolves the bearer or answers 401; the hash comes back so the
// caller can keep or delete this very session.
func (a *Server) session(w http.ResponseWriter, r *http.Request) (identity, []byte, bool) {
	secret, found := strings.CutPrefix(r.Header.Get("Authorization"), "Bearer ")
	hash, ok := secretHash(strings.TrimSpace(secret))
	if !found || !ok {
		writeError(w, http.StatusUnauthorized, "session")
		return identity{}, nil, false
	}
	who, err := a.store.resolveSession(r.Context(), hash, a.now())
	if errors.Is(err, errUnknownSecret) {
		// The one answer a client may take to mean its session is gone for
		// good and start over; a bearer that is missing or malformed is a
		// client's or a proxy's fault, not a forgotten session.
		writeError(w, http.StatusUnauthorized, "unknown_session")
		return identity{}, nil, false
	}
	if err != nil {
		a.fail(w, err)
		return identity{}, nil, false
	}
	return who, hash, true
}

// hashing takes a bcrypt slot or answers 503; the caller releases it.
func (a *Server) hashing(w http.ResponseWriter) (release func(), ok bool) {
	timer := time.NewTimer(hashingWait)
	defer timer.Stop()
	select {
	case a.hashes <- struct{}{}:
		return func() { <-a.hashes }, true
	case <-timer.C:
		writeError(w, http.StatusServiceUnavailable, "busy")
		return nil, false
	}
}

type credentials struct {
	Login    string `json:"login"`
	Password string `json:"password"`
}

// Register attaches a login to the session's player; the device keeps its
// session, so registering never logs anyone out.
func (a *Server) Register(w http.ResponseWriter, r *http.Request) {
	who, _, ok := a.session(w, r)
	if !ok {
		return
	}
	var c credentials
	if !a.readJSON(w, r, &c) {
		return
	}
	login, ok := normalizeLogin(c.Login)
	if !ok || !passwordOK(c.Password) {
		writeError(w, http.StatusBadRequest, "rules")
		return
	}
	release, ok := a.hashing(w)
	if !ok {
		return
	}
	hash, err := bcrypt.GenerateFromPassword([]byte(c.Password), bcrypt.DefaultCost)
	release()
	if err != nil {
		a.fail(w, err)
		return
	}
	err = a.store.register(r.Context(), who.id, login, string(hash))
	switch {
	case errors.Is(err, errLoginTaken):
		writeError(w, http.StatusConflict, "taken")
	case errors.Is(err, errRegistered):
		writeError(w, http.StatusConflict, "registered")
	case err != nil:
		a.fail(w, err)
	default:
		writeJSON(w, http.StatusOK, map[string]any{"login": login})
	}
}

// Login opens an account on this device with a fresh session. An unknown
// Login costs the same bcrypt as a wrong password and answers the same
// bytes, and every wrong answer counts against the login name before any
// lookup, so neither timing nor the counter tells logins apart; a busy
// server refuses before the attempt is spent. The new session is written
// only if the password it verified still stands, so a login racing a
// password change cannot outlive the revocation. A bearer sent along is
// the session this device leaves behind: it is deleted once the new one
// exists.
func (a *Server) Login(w http.ResponseWriter, r *http.Request) {
	var c credentials
	if !a.readJSON(w, r, &c) {
		return
	}
	login, ok := normalizeLogin(c.Login)
	if !ok || !passwordOK(c.Password) {
		writeError(w, http.StatusBadRequest, "rules")
		return
	}
	release, ok := a.hashing(w)
	if !ok {
		return
	}
	defer release()
	if !a.attempts.allow(login, a.now()) {
		writeError(w, http.StatusTooManyRequests, "attempts")
		return
	}
	who, err := a.store.findLogin(r.Context(), login)
	known := err == nil
	if errors.Is(err, errUnknownLogin) {
		who.passwordHash = a.dummyHash
	} else if err != nil {
		a.fail(w, err)
		return
	}
	matched := bcrypt.CompareHashAndPassword([]byte(who.passwordHash), []byte(c.Password)) == nil
	if !known || !matched {
		writeError(w, http.StatusForbidden, "wrong")
		return
	}
	secret, err := newSecret()
	if err != nil {
		a.fail(w, err)
		return
	}
	hash, _ := secretHash(secret)
	landed, err := a.store.newSession(r.Context(), who.id, hash, who.passwordHash)
	if err != nil {
		a.fail(w, err)
		return
	}
	if !landed {
		writeError(w, http.StatusForbidden, "wrong")
		return
	}
	a.attempts.forget(login)
	if old, found := strings.CutPrefix(r.Header.Get("Authorization"), "Bearer "); found {
		if oldHash, ok := secretHash(strings.TrimSpace(old)); ok {
			if err := a.store.deleteSession(r.Context(), oldHash); err != nil {
				a.fail(w, err)
				return
			}
		}
	}
	writeJSON(w, http.StatusOK, map[string]any{"player_id": who.id, "secret": secret, "login": who.login})
}

type passwordChange struct {
	Old string `json:"old"`
	New string `json:"new"`
}

// Password keeps this session and revokes every other one. A wrong
// old password counts as an attempt against this session, so a stolen
// session cannot guess, and a lockout of the login name (anyone can spend
// it) does not keep the owner from changing the password.
func (a *Server) Password(w http.ResponseWriter, r *http.Request) {
	who, keep, ok := a.session(w, r)
	if !ok {
		return
	}
	if who.login == "" {
		writeError(w, http.StatusConflict, "guest")
		return
	}
	var c passwordChange
	if !a.readJSON(w, r, &c) {
		return
	}
	if !passwordOK(c.New) {
		writeError(w, http.StatusBadRequest, "rules")
		return
	}
	release, ok := a.hashing(w)
	if !ok {
		return
	}
	defer release()
	attempts := "session:" + hex.EncodeToString(keep)
	if !a.attempts.allow(attempts, a.now()) {
		writeError(w, http.StatusTooManyRequests, "attempts")
		return
	}
	if bcrypt.CompareHashAndPassword([]byte(who.passwordHash), []byte(c.Old)) != nil {
		writeError(w, http.StatusForbidden, "wrong")
		return
	}
	hash, err := bcrypt.GenerateFromPassword([]byte(c.New), bcrypt.DefaultCost)
	if err != nil {
		a.fail(w, err)
		return
	}
	a.attempts.forget(attempts)
	if err := a.store.setPassword(r.Context(), who.id, string(hash), keep); err != nil {
		a.fail(w, err)
		return
	}
	w.WriteHeader(http.StatusNoContent)
}

// Logout deletes this device's session; the client forgets it either way.
func (a *Server) Logout(w http.ResponseWriter, r *http.Request) {
	_, hash, ok := a.session(w, r)
	if !ok {
		return
	}
	if err := a.store.deleteSession(r.Context(), hash); err != nil {
		a.fail(w, err)
		return
	}
	w.WriteHeader(http.StatusNoContent)
}
