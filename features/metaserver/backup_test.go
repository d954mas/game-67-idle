package metaserver

import (
	"bytes"
	"context"
	"fmt"
	"net/url"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"testing"

	"github.com/jackc/pgx/v5"
)

// Slow tier: builds backup/ and runs it in Docker against the local stand's
// network, so the core `go test ./...` skips it.
const backupGate = "META_BACKUP_DOCKER"

func TestBackupDumpRestoresIntoAnotherDatabase(t *testing.T) {
	if os.Getenv(backupGate) != "1" {
		t.Skip(backupGate + "=1 runs the backup image in Docker against the local stand")
	}
	admin, err := url.Parse(envOr("META_TEST_DATABASE_URL", defaultDatabaseURL))
	if err != nil || (admin.Hostname() != "127.0.0.1" && admin.Hostname() != "localhost") {
		t.Fatal("the backup test runs only against a local stand database")
	}
	network := envOr("META_BACKUP_DOCKER_NETWORK", "metaserver-stand_default")
	ctx := t.Context()

	const password = "backup-test-password-12345678901"
	src, srcURL := emptyStore(t)
	if err := src.migrate(ctx, roleLogin{RoleBackup, password}); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	login := "backup-seed-" + strconv.FormatInt(int64(os.Getpid()), 36)
	for _, seed := range []string{
		`INSERT INTO players (login, password_hash) VALUES (@login, 'hash')`,
		`INSERT INTO ops.admin_users (username, password_hash, role) VALUES (@login, 'hash', 'viewer')`,
		`INSERT INTO activity.occupancy_runs (minute_utc, sampled_at) VALUES (date_trunc('minute', now()), now())`,
	} {
		if _, err := src.pool.Exec(ctx, seed, pgx.NamedArgs{"login": login}); err != nil {
			t.Fatalf("seed: %v", err)
		}
	}

	docker(t, "build", "-q", "-t", "metaserver-backup:test", "backup")
	work := t.TempDir()
	backups := filepath.Join(work, "backups")
	secret := filepath.Join(work, "password")
	if err := os.Mkdir(backups, 0o700); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(secret, []byte(password+"\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	run := func(env []string, args ...string) (string, string, error) {
		cmd := []string{"run", "--rm", "--network", network, "-e", "BACKUP_NAME=game",
			"-v", backups + ":/backups", "-v", secret + ":/run/secrets/password:ro"}
		for _, e := range env {
			cmd = append(cmd, "-e", e)
		}
		var stdout, stderr bytes.Buffer
		c := exec.CommandContext(ctx, "docker", append(cmd, args...)...)
		c.Stdout, c.Stderr = &stdout, &stderr
		err := c.Run()
		return stdout.String(), stderr.String(), err
	}

	out, logs, err := run([]string{"PGHOST=db", "PGUSER=" + RoleBackup, "PGDATABASE=" + dbName(t, srcURL),
		"PGPASSWORD_FILE=/run/secrets/password"}, "metaserver-backup:test", "once")
	if err != nil {
		t.Fatalf("once: %v\n%s", err, logs)
	}
	if strings.Contains(out+logs, login) {
		t.Fatal("once printed row data")
	}
	match := regexp.MustCompile(`^(\d+) (game-\d{8}-\d{6}-[0-9a-f]{6}\.dump)\n$`).FindStringSubmatch(out)
	if match == nil || match[1] != strconv.Itoa(SchemaVersion) {
		t.Fatalf("once stdout %q, want \"%d game-<stamp>.dump\"", out, SchemaVersion)
	}
	dump := match[2]
	if parts, _ := filepath.Glob(filepath.Join(backups, "*.part")); len(parts) > 0 {
		t.Fatalf("left behind: %v", parts)
	}

	dst, dstURL := emptyStore(t)
	ownerPassword, _ := admin.User.Password()
	if _, logs, err := run([]string{"PGHOST=db", "PGUSER=" + admin.User.Username(), "PGPASSWORD=" + ownerPassword},
		"--entrypoint", "pg_restore", "metaserver-backup:test",
		"-d", dbName(t, dstURL), "--exit-on-error", "--single-transaction", "--no-owner", "--no-acl", "/backups/"+dump); err != nil {
		t.Fatalf("restore into the second database: %v\n%s", err, logs)
	}
	if want, got := tableCounts(t, src), tableCounts(t, dst); want != got {
		t.Fatalf("restored counts differ:\nsource   %s\nrestored %s", want, got)
	}
	var restored string
	if err := dst.pool.QueryRow(ctx, `SELECT p.login FROM players p JOIN ops.admin_users a ON a.username = p.login`).Scan(&restored); err != nil || restored != login {
		t.Fatalf("seeded row after restore: %q, %v", restored, err)
	}

	whole, err := os.ReadFile(filepath.Join(backups, dump))
	if err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(backups, "game-corrupt.dump"), whole[:len(whole)*2/3], 0o600); err != nil {
		t.Fatal(err)
	}
	out, logs, err = run(nil, "metaserver-backup:test", "check", "/backups/game-corrupt.dump")
	if err == nil || out != "" {
		t.Fatalf("a truncated dump passed the check: %q", out)
	}
	if strings.Contains(out+logs, login) || !strings.Contains(logs, "did not restore") {
		t.Fatalf("truncated dump check output: %s", logs)
	}
}

func docker(t *testing.T, args ...string) {
	t.Helper()
	if out, err := exec.CommandContext(t.Context(), "docker", args...).CombinedOutput(); err != nil {
		t.Fatalf("docker %s: %v\n%s", args[0], err, out)
	}
}

func dbName(t *testing.T, databaseURL string) string {
	t.Helper()
	parsed, err := url.Parse(databaseURL)
	if err != nil {
		t.Fatal(err)
	}
	return strings.TrimPrefix(parsed.Path, "/")
}

// "schema.table=count ..." over every user table.
func tableCounts(t *testing.T, st *store) string {
	t.Helper()
	ctx := context.Background()
	rows, err := st.pool.Query(ctx, `SELECT format('%I.%I', n.nspname, c.relname)
		FROM pg_class c JOIN pg_namespace n ON n.oid = c.relnamespace
		WHERE c.relkind IN ('r', 'p') AND NOT c.relispartition
		  AND n.nspname NOT IN ('pg_catalog', 'information_schema') AND n.nspname NOT LIKE 'pg\_toast%'
		ORDER BY 1`)
	if err != nil {
		t.Fatal(err)
	}
	var tables []string
	for rows.Next() {
		var table string
		if err := rows.Scan(&table); err != nil {
			t.Fatal(err)
		}
		tables = append(tables, table)
	}
	rows.Close()
	var out strings.Builder
	for _, table := range tables {
		var n int64
		if err := st.pool.QueryRow(ctx, "SELECT count(*) FROM "+table).Scan(&n); err != nil {
			t.Fatal(err)
		}
		fmt.Fprintf(&out, "%s=%d ", table, n)
	}
	return out.String()
}
