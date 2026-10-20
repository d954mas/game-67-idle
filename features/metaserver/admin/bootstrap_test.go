package admin

import (
	"encoding/base64"
	"fmt"
	"testing"
	"time"

	"golang.org/x/crypto/argon2"
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

func TestRecoveryRevokesLoginThatReadTheOldPassword(t *testing.T) {
	store := freshAdminStore(t)
	ctx := t.Context()
	if err := bootstrapOwner(ctx, store.pool, "owner", "1234567890"); err != nil {
		t.Fatal(err)
	}
	// The slowest hash verify accepts keeps login in verify long enough for recovery to commit in between.
	salt := make([]byte, 16)
	slow := argon2.IDKey([]byte("1234567890"), salt, 10, 256*1024, 1, 32)
	if _, err := store.pool.Exec(ctx, `UPDATE ops.admin_users SET password_hash=$1 WHERE username='owner'`,
		fmt.Sprintf("$argon2id$v=19$m=%d,t=10,p=1$%s$%s", 256*1024,
			base64.RawStdEncoding.EncodeToString(salt), base64.RawStdEncoding.EncodeToString(slow))); err != nil {
		t.Fatal(err)
	}
	holder, err := store.pool.Acquire(ctx)
	if err != nil {
		t.Fatal(err)
	}
	defer holder.Release()
	if _, err := holder.Exec(ctx, `SELECT pg_advisory_lock($1)`, int64(ownerLockKey)); err != nil {
		t.Fatal(err)
	}
	waitFor := func(query string) {
		t.Helper()
		for deadline := time.Now().Add(10 * time.Second); time.Now().Before(deadline); time.Sleep(time.Millisecond) {
			var ok bool
			if err := store.pool.QueryRow(ctx, query).Scan(&ok); err != nil {
				t.Fatal(err)
			}
			if ok {
				return
			}
		}
		t.Fatalf("timed out waiting for %s", query)
	}
	recovered := make(chan error, 1)
	go func() { recovered <- recoverOwner(ctx, store.pool, "owner", "abcdefghij") }()
	waitFor(`SELECT count(*) = 1 FROM pg_locks WHERE locktype='advisory' AND NOT granted`)
	loggedIn := make(chan error, 1)
	go func() {
		_, _, err := store.login(ctx, "owner", "1234567890", time.Now())
		loggedIn <- err
	}()
	// Login is either queued behind recovery on the lock or already holds the old hash.
	waitFor(`SELECT (SELECT count(*) = 2 FROM pg_locks WHERE locktype='advisory' AND NOT granted)
		OR EXISTS (SELECT 1 FROM pg_stat_activity WHERE pid <> pg_backend_pid()
			AND state = 'idle in transaction' AND query LIKE '%password_hash%')`)
	if _, err := holder.Exec(ctx, `SELECT pg_advisory_unlock($1)`, int64(ownerLockKey)); err != nil {
		t.Fatal(err)
	}
	if err := <-recovered; err != nil {
		t.Fatal(err)
	}
	loginErr := <-loggedIn
	var live int
	if err := store.pool.QueryRow(ctx, `SELECT count(*) FROM ops.admin_sessions WHERE revoked_at IS NULL`).Scan(&live); err != nil || live != 0 {
		t.Fatalf("old-password session survived recovery: live=%d login=%v err=%v", live, loginErr, err)
	}
}
