package admin

import (
	"context"
	"crypto/rand"
	"encoding/base32"
	"encoding/hex"
	"errors"
	"fmt"
	"io"
	"net/url"
	"os"
	"strings"
	"time"
	"unicode/utf8"

	"github.com/jackc/pgx/v5/pgxpool"
	"golang.org/x/term"
)

func validOwnerPassword(password string) bool {
	return utf8.ValidString(password) && utf8.RuneCountInString(password) >= 10 && len(password) <= 1024
}

func bootstrapOwner(ctx context.Context, pool *pgxpool.Pool, key []byte, username, password string,
	secret []byte, step int64) error {
	if len(username) < 1 || len(username) > 64 || !validOwnerPassword(password) {
		return errors.New("invalid owner credentials")
	}
	hash, err := hashPassword(password)
	if err != nil {
		return err
	}
	encrypted, err := encryptSecret(key, secret)
	if err != nil {
		return err
	}
	tx, err := pool.Begin(ctx)
	if err != nil {
		return err
	}
	defer tx.Rollback(ctx)
	if _, err := tx.Exec(ctx, `SELECT pg_advisory_xact_lock(7454134)`); err != nil {
		return err
	}
	var ownerExists bool
	if err := tx.QueryRow(ctx, `SELECT EXISTS (SELECT 1 FROM ops.admin_users WHERE role='owner')`).Scan(&ownerExists); err != nil {
		return err
	}
	if ownerExists {
		return errors.New("owner already exists; use recover-owner locally")
	}
	var id int64
	if err := tx.QueryRow(ctx, `INSERT INTO ops.admin_users
		(username, password_hash, totp_secret, last_totp_step, role)
		VALUES ($1, $2, $3, $4, 'owner') RETURNING id`, username, hash, encrypted, step).Scan(&id); err != nil {
		return err
	}
	if _, err := tx.Exec(ctx, `INSERT INTO ops.audit_actions (user_id, action, target, result)
		VALUES ($1, 'owner_bootstrap', $2, 'applied')`, id, username); err != nil {
		return err
	}
	return tx.Commit(ctx)
}

func recoverOwner(ctx context.Context, pool *pgxpool.Pool, key []byte, username, password string,
	secret []byte, step int64) error {
	if len(username) < 1 || len(username) > 64 || !validOwnerPassword(password) {
		return errors.New("invalid owner credentials")
	}
	hash, err := hashPassword(password)
	if err != nil {
		return err
	}
	encrypted, err := encryptSecret(key, secret)
	if err != nil {
		return err
	}
	tx, err := pool.Begin(ctx)
	if err != nil {
		return err
	}
	defer tx.Rollback(ctx)
	if _, err := tx.Exec(ctx, `SELECT pg_advisory_xact_lock(7454134)`); err != nil {
		return err
	}
	var id int64
	if err := tx.QueryRow(ctx, `UPDATE ops.admin_users SET password_hash=$2, totp_secret=$3,
		last_totp_step=$4, disabled=false, updated_at=now() WHERE username=$1 AND role='owner'
		RETURNING id`, username, hash, encrypted, step).Scan(&id); err != nil {
		return err
	}
	if _, err := tx.Exec(ctx, `UPDATE ops.admin_sessions SET revoked_at=now()
		WHERE user_id=$1 AND revoked_at IS NULL`, id); err != nil {
		return err
	}
	if _, err := tx.Exec(ctx, `INSERT INTO ops.audit_actions (user_id, action, target, result)
		VALUES ($1, 'owner_recovery', $2, 'applied')`, id, username); err != nil {
		return err
	}
	return tx.Commit(ctx)
}

func readHidden(prompt string) (string, error) {
	fmt.Fprint(os.Stderr, prompt)
	raw, err := term.ReadPassword(int(os.Stdin.Fd()))
	fmt.Fprintln(os.Stderr)
	return strings.TrimSpace(string(raw)), err
}

func ownerFactor(required bool, issuer, username string, readCode func(string) (string, error), output io.Writer) ([]byte, int64, error) {
	secret := make([]byte, 20)
	if _, err := rand.Read(secret); err != nil {
		return nil, -1, err
	}
	if !required {
		return secret, -1, nil
	}
	encoded := base32.StdEncoding.WithPadding(base32.NoPadding).EncodeToString(secret)
	query := url.Values{"secret": {encoded}, "issuer": {issuer}}
	fmt.Fprintln(output, "Add this TOTP account once:")
	fmt.Fprintln(output, "otpauth://totp/"+url.PathEscape(issuer)+":"+url.PathEscape(username)+"?"+query.Encode())
	code, err := readCode("Enter the current six-digit code: ")
	if err != nil {
		return nil, -1, err
	}
	step, valid := verifyTOTP(secret, code, time.Now(), -1)
	if !valid {
		return nil, -1, errors.New("TOTP enrollment code was invalid")
	}
	return secret, step, nil
}

func runOwnerCeremony(mode string, brand Brand) error {
	if !term.IsTerminal(int(os.Stdin.Fd())) {
		return errors.New("owner ceremony requires an interactive terminal")
	}
	totpRequired, err := parseTOTPRequired()
	if err != nil {
		return err
	}
	dbURL := os.Getenv("ADMIN_BOOTSTRAP_DATABASE_URL")
	if dbURL == "" {
		return errors.New("ADMIN_BOOTSTRAP_DATABASE_URL is required for local owner ceremony")
	}
	passwordFile := os.Getenv("ADMIN_BOOTSTRAP_DATABASE_PASSWORD_FILE")
	if passwordFile == "" {
		return errors.New("ADMIN_BOOTSTRAP_DATABASE_PASSWORD_FILE is required")
	}
	passwordText, err := os.ReadFile(passwordFile)
	if err != nil {
		return err
	}
	dbPassword := strings.TrimSpace(string(passwordText))
	if len(dbPassword) < 32 || len(dbPassword) > 256 {
		return errors.New("ADMIN_BOOTSTRAP_DATABASE_PASSWORD_FILE must contain a 32..256 character password")
	}
	poolConfig, err := pgxpool.ParseConfig(dbURL)
	if err != nil {
		return err
	}
	if poolConfig.ConnConfig.Password != "" {
		return errors.New("owner ceremony requires a password-free database URL")
	}
	poolConfig.ConnConfig.Password = dbPassword
	keyFile := os.Getenv("ADMIN_TOTP_KEY_FILE")
	if keyFile == "" {
		return errors.New("ADMIN_TOTP_KEY_FILE is required")
	}
	keyText, err := os.ReadFile(keyFile)
	if err != nil {
		return err
	}
	key, err := hex.DecodeString(strings.TrimSpace(string(keyText)))
	if err != nil || len(key) != 32 {
		return errors.New("ADMIN_TOTP_KEY_FILE must contain 64 hex characters")
	}
	username := os.Getenv("ADMIN_OWNER_USER")
	if username == "" {
		username = "owner"
	}
	if len(username) > 64 || strings.TrimSpace(username) != username {
		return errors.New("ADMIN_OWNER_USER must be a trimmed name of at most 64 characters")
	}
	startupCtx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	pool, err := pgxpool.NewWithConfig(startupCtx, poolConfig)
	if err != nil {
		cancel()
		return err
	}
	defer pool.Close()
	var canWrite bool
	// The ceremony runs as the database owner, the role the migrate job uses.
	if err := pool.QueryRow(startupCtx, `SELECT
		(SELECT datdba FROM pg_database WHERE datname = current_database()) =
		(SELECT oid FROM pg_roles WHERE rolname = current_user) AND
		has_table_privilege(current_user, 'ops.admin_users', 'INSERT') AND
		has_table_privilege(current_user, 'ops.admin_users', 'UPDATE') AND
		has_table_privilege(current_user, 'ops.audit_actions', 'INSERT')`).Scan(&canWrite); err != nil {
		cancel()
		return err
	}
	if !canWrite {
		cancel()
		return errors.New("owner ceremony must run as the database owner")
	}
	var ready bool
	if mode == "recover-owner" {
		err = pool.QueryRow(startupCtx, `SELECT EXISTS (SELECT 1 FROM ops.admin_users
			WHERE username=$1 AND role='owner')`, username).Scan(&ready)
	} else {
		err = pool.QueryRow(startupCtx, `SELECT NOT EXISTS (SELECT 1 FROM ops.admin_users
			WHERE role='owner')`).Scan(&ready)
	}
	cancel()
	if err != nil {
		return err
	}
	if !ready {
		return errors.New("owner state does not permit this ceremony")
	}
	if mode == "recover-owner" {
		prompt := "Type RECOVER " + username + " to replace the owner password and revoke sessions: "
		if totpRequired {
			prompt = "Type RECOVER " + username + " to replace the owner factor and revoke sessions: "
		}
		confirmation, err := readHidden(prompt)
		if err != nil || confirmation != "RECOVER "+username {
			return errors.New("owner recovery cancelled")
		}
	}
	password, err := readHidden("New owner password (at least 10 characters): ")
	if err != nil {
		return err
	}
	confirmed, err := readHidden("Repeat password: ")
	if err != nil || password != confirmed || !validOwnerPassword(password) {
		return errors.New("owner passwords differ, have fewer than 10 characters, or exceed 1024 bytes")
	}
	secret, step, err := ownerFactor(totpRequired, brand.Name, username, readHidden, os.Stdout)
	if err != nil {
		return err
	}
	writeCtx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()
	if mode == "recover-owner" {
		err = recoverOwner(writeCtx, pool, key, username, password, secret, step)
	} else {
		err = bootstrapOwner(writeCtx, pool, key, username, password, secret, step)
	}
	if err == nil {
		if totpRequired {
			fmt.Fprintln(os.Stdout, "Owner ready. Wait for the next TOTP code before logging in.")
		} else {
			fmt.Fprintln(os.Stdout, "Owner ready. Sign in with the new password.")
		}
	}
	return err
}
