package admin

import (
	"bytes"
	"crypto/sha256"
	"net/http/httptest"
	"testing"
)

func TestPasswordHashRejectsWrongAndMalformedPasswords(t *testing.T) {
	hash, err := hashPassword("long owner password")
	if err != nil {
		t.Fatal(err)
	}
	if !verifyPassword(hash, "long owner password") || verifyPassword(hash, "wrong password") || verifyPassword("invalid", "long owner password") {
		t.Fatal("password verifier accepted the wrong value or rejected the right one")
	}
}

func TestSessionSecretAndCSRFAreBoundToSameCookie(t *testing.T) {
	one, err := newSessionSecret()
	if err != nil {
		t.Fatal(err)
	}
	two, err := newSessionSecret()
	if err != nil {
		t.Fatal(err)
	}
	if bytes.Equal(sessionHash(one), sessionHash(two)) || bytes.Equal(sessionHash(one), sha256.New().Sum(nil)) {
		t.Fatal("session secret was not random")
	}
	brand := Brand{Name: "Example Game"}
	if csrfToken(brand, one) == csrfToken(brand, two) || !validCSRF(brand, one, csrfToken(brand, one)) ||
		validCSRF(brand, two, csrfToken(brand, one)) {
		t.Fatal("CSRF token did not bind to its session")
	}
}

func TestBrandsOnOneHostnameKeepSeparateSessions(t *testing.T) {
	one, two := Brand{Name: "Example Game", Mark: "EG"}, Brand{Name: "Other Game", Mark: "EG"}
	if one.sessionCookie() == two.sessionCookie() || one.sessionCookie() != "__Host-example-game-admin" {
		t.Fatalf("session cookies: %q %q", one.sessionCookie(), two.sessionCookie())
	}
	secret, err := newSessionSecret()
	if err != nil {
		t.Fatal(err)
	}
	if validCSRF(two, secret, csrfToken(one, secret)) {
		t.Fatal("CSRF token crossed brands")
	}
}

func TestOriginCheckRequiresConfiguredAdminOrigin(t *testing.T) {
	r := httptest.NewRequest("POST", "https://admin.example.test/login", nil)
	if validOrigin(r, "https://admin.example.test") {
		t.Fatal("missing Origin accepted")
	}
	r.Header.Set("Origin", "https://game.example.test")
	if validOrigin(r, "https://admin.example.test") {
		t.Fatal("foreign Origin accepted")
	}
	r.Header.Set("Origin", "https://admin.example.test")
	if !validOrigin(r, "https://admin.example.test") {
		t.Fatal("configured Origin rejected")
	}
}
