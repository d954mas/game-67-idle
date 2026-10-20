package admin

import (
	"bytes"
	"errors"
	"testing"
	"time"
)

func TestOwnerSetupWithoutTOTPDoesNotAskForCode(t *testing.T) {
	var output bytes.Buffer
	secret, step, err := ownerFactor(false, "Example", "owner", func(string) (string, error) {
		return "", errors.New("unexpected code prompt")
	}, &output)
	if err != nil || len(secret) != 20 || step != -1 || output.Len() != 0 {
		t.Fatalf("password-only owner factor: secret=%d step=%d output=%q err=%v",
			len(secret), step, output.String(), err)
	}
}

func TestOwnerPasswordMinimumIsTenCharacters(t *testing.T) {
	for _, password := range []string{"123456789", "абвгдежзи"} {
		if validOwnerPassword(password) {
			t.Fatalf("accepted short password %q", password)
		}
	}
	for _, password := range []string{"1234567890", "абвгдежзий"} {
		if !validOwnerPassword(password) {
			t.Fatalf("rejected ten-character password %q", password)
		}
	}
}

func TestLocalOwnerBootstrapAndRecoveryRevokeSessions(t *testing.T) {
	store := freshAdminStore(t)
	now := time.Unix(1_780_000_000, 0).UTC()
	first := []byte("12345678901234567890")
	second := []byte("09876543210987654321")
	if err := bootstrapOwner(t.Context(), store.pool, store.totpKey, "owner", "1234567890",
		first, now.Unix()/30); err != nil {
		t.Fatal(err)
	}
	if err := bootstrapOwner(t.Context(), store.pool, store.totpKey, "other", "another password long",
		first, now.Unix()/30); err == nil {
		t.Fatal("second owner bootstrap succeeded")
	}
	loginTime := now.Add(time.Minute)
	cookie, _, err := store.login(t.Context(), "owner", "1234567890",
		totpCode(first, loginTime.Unix()/30), loginTime)
	if err != nil {
		t.Fatal(err)
	}
	if err := recoverOwner(t.Context(), store.pool, store.totpKey, "owner", "abcdefghij",
		second, loginTime.Unix()/30); err != nil {
		t.Fatal(err)
	}
	if _, err := store.session(t.Context(), cookie, loginTime.Add(time.Second)); err != errNoSession {
		t.Fatalf("old owner session survived recovery: %v", err)
	}
	recoveryLoginTime := loginTime.Add(time.Minute)
	if _, _, err := store.login(t.Context(), "owner", "1234567890",
		totpCode(first, recoveryLoginTime.Unix()/30), recoveryLoginTime); err != errInvalidCredentials {
		t.Fatalf("old owner password survived recovery: %v", err)
	}
	if _, _, err := store.login(t.Context(), "owner", "abcdefghij",
		totpCode(second, recoveryLoginTime.Unix()/30), recoveryLoginTime); err != nil {
		t.Fatal(err)
	}
	var actions int
	if err := store.pool.QueryRow(t.Context(), `SELECT count(*) FROM ops.audit_actions
		WHERE action IN ('owner_bootstrap', 'owner_recovery')`).Scan(&actions); err != nil || actions != 2 {
		t.Fatalf("owner audit actions=%d err=%v", actions, err)
	}
	var encrypted []byte
	if err := store.pool.QueryRow(t.Context(), `SELECT totp_secret FROM ops.admin_users WHERE username='owner'`).Scan(&encrypted); err != nil {
		t.Fatal(err)
	}
	if bytes.Contains(encrypted, second) {
		t.Fatal("TOTP secret persisted in clear text")
	}
}
