package admin

import (
	"bytes"
	"crypto/sha256"
	"net/http/httptest"
	"testing"
	"time"
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

func TestTOTPUsesSixDigitRFC6238CodeAndWindow(t *testing.T) {
	secret := []byte("12345678901234567890")
	if got := totpCode(secret, 1); got != "287082" {
		t.Fatalf("RFC 6238 code: %q", got)
	}
	step, ok := verifyTOTP(secret, "287082", time.Unix(59, 0), -1)
	if !ok || step != 1 {
		t.Fatalf("valid code: step=%d accepted=%v", step, ok)
	}
	if _, ok := verifyTOTP(secret, "287082", time.Unix(59, 0), step); ok {
		t.Fatal("a used TOTP step was accepted again")
	}
	if _, ok := verifyTOTP(secret, "000000", time.Unix(59, 0), -1); ok {
		t.Fatal("wrong code accepted")
	}
}

func TestTOTPSecretEncryptionRejectsWrongKey(t *testing.T) {
	key := bytes.Repeat([]byte{0x31}, 32)
	secret := []byte("12345678901234567890")
	encrypted, err := encryptSecret(key, secret)
	if err != nil {
		t.Fatal(err)
	}
	got, err := decryptSecret(key, encrypted)
	if err != nil || !bytes.Equal(got, secret) {
		t.Fatalf("decrypt: %x %v", got, err)
	}
	wrong := bytes.Repeat([]byte{0x32}, 32)
	if _, err := decryptSecret(wrong, encrypted); err == nil {
		t.Fatal("wrong encryption key accepted")
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
