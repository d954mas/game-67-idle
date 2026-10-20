package admin

import (
	"testing"
	"time"
)

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
	if err := bootstrapOwner(t.Context(), store.pool, "owner", "1234567890"); err != nil {
		t.Fatal(err)
	}
	if err := bootstrapOwner(t.Context(), store.pool, "other", "another password long"); err == nil {
		t.Fatal("second owner bootstrap succeeded")
	}
	loginTime := now.Add(time.Minute)
	cookie, _, err := store.login(t.Context(), "owner", "1234567890", loginTime)
	if err != nil {
		t.Fatal(err)
	}
	if err := recoverOwner(t.Context(), store.pool, "owner", "abcdefghij"); err != nil {
		t.Fatal(err)
	}
	if _, err := store.session(t.Context(), cookie, loginTime.Add(time.Second)); err != errNoSession {
		t.Fatalf("old owner session survived recovery: %v", err)
	}
	recoveryLoginTime := loginTime.Add(time.Minute)
	if _, _, err := store.login(t.Context(), "owner", "1234567890", recoveryLoginTime); err != errInvalidCredentials {
		t.Fatalf("old owner password survived recovery: %v", err)
	}
	if _, _, err := store.login(t.Context(), "owner", "abcdefghij", recoveryLoginTime); err != nil {
		t.Fatal(err)
	}
	var actions int
	if err := store.pool.QueryRow(t.Context(), `SELECT count(*) FROM ops.audit_actions
		WHERE action IN ('owner_bootstrap', 'owner_recovery')`).Scan(&actions); err != nil || actions != 2 {
		t.Fatalf("owner audit actions=%d err=%v", actions, err)
	}
}
