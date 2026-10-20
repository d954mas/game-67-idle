package metaserver

import (
	"context"
	"net/url"
	"os"
	"path/filepath"
	"testing"
	"time"
)

func TestMigrationIsRepeatableAndRejectsDrift(t *testing.T) {
	st, _ := freshStore(t)
	ctx := context.Background()
	if err := st.migrate(ctx); err != nil {
		t.Fatalf("repeat migration: %v", err)
	}
	var count int
	if err := st.pool.QueryRow(ctx, `SELECT count(*) FROM schema_migrations`).Scan(&count); err != nil || count != len(migrations) {
		t.Fatalf("migration history: count=%d err=%v", count, err)
	}
	if _, err := st.pool.Exec(ctx, `UPDATE schema_migrations SET checksum = 'tampered' WHERE version = 1`); err != nil {
		t.Fatal(err)
	}
	if err := st.migrate(ctx); err == nil {
		t.Fatal("a changed migration checksum was accepted")
	}
}

func TestRuntimeAndBackupRolesCannotChangeOperatorData(t *testing.T) {
	st, _ := freshStore(t)
	ctx := t.Context()
	var gameInsert, gameSequence, gameOps, gameMigration, backupRead, backupWrite bool
	err := st.pool.QueryRow(ctx, `SELECT
		has_table_privilege('meta_runtime', 'public.players', 'INSERT'),
		has_sequence_privilege('meta_runtime', 'public.players_id_seq', 'USAGE'),
		has_table_privilege('meta_runtime', 'ops.admin_users', 'SELECT, INSERT, UPDATE, DELETE'),
		has_table_privilege('meta_runtime', 'public.schema_migrations', 'INSERT, UPDATE, DELETE'),
		has_table_privilege('meta_backup', 'ops.admin_users', 'SELECT'),
		has_table_privilege('meta_backup', 'ops.admin_users', 'INSERT, UPDATE, DELETE')`).Scan(
		&gameInsert, &gameSequence, &gameOps, &gameMigration, &backupRead, &backupWrite)
	if err != nil {
		t.Fatal(err)
	}
	if !gameInsert || !gameSequence || gameOps || gameMigration || !backupRead || backupWrite {
		t.Fatalf("role privileges: game_insert=%v sequence=%v ops=%v migration=%v backup_read=%v backup_write=%v",
			gameInsert, gameSequence, gameOps, gameMigration, backupRead, backupWrite)
	}
	if _, err := st.pool.Exec(ctx, `CREATE TABLE public.future_operator_data (secret text)`); err != nil {
		t.Fatal(err)
	}
	var futureRead bool
	if err := st.pool.QueryRow(ctx, `SELECT has_table_privilege('meta_runtime', 'public.future_operator_data', 'SELECT')`).Scan(&futureRead); err != nil {
		t.Fatal(err)
	}
	if futureRead {
		t.Fatal("runtime inherited read access to a future table")
	}
}

func TestRestrictedRuntimeCanServeGameAndSampleRooms(t *testing.T) {
	owner, ownerURL := freshStore(t)
	ctx := t.Context()
	password := "runtime-test-password-1234567890"
	if err := owner.provisionRole(ctx, "meta_runtime", password); err != nil {
		t.Fatal(err)
	}
	passwordFile := filepath.Join(t.TempDir(), "db.password")
	if err := os.WriteFile(passwordFile, []byte(password), 0600); err != nil {
		t.Fatal(err)
	}
	parsed, err := url.Parse(ownerURL)
	if err != nil {
		t.Fatal(err)
	}
	parsed.User = url.User("meta_runtime")
	runtime, err := openStoreWithPassword(ctx, parsed.String(), passwordFile, "meta_runtime")
	if err != nil {
		t.Fatal(err)
	}
	defer runtime.close()
	if err := runtime.checkSchema(ctx); err != nil {
		t.Fatal(err)
	}
	if _, err := runtime.createGuest(ctx, []byte("test guest secret hash")); err != nil {
		t.Fatalf("create guest: %v", err)
	}
	status := &roomStatus{V: statusVersion, Port: 7001, Host: "room.test", Free: 3,
		Seated: 1, Capacity: 4, PeriodMs: 1000}
	if accepted, err := runtime.reportRoom(ctx, 9, 1, status); err != nil || !accepted {
		t.Fatalf("room status: accepted=%v err=%v", accepted, err)
	}
	if err := runtime.sampleOccupancy(ctx, time.Now()); err != nil {
		t.Fatalf("occupancy sample: %v", err)
	}
	if _, err := runtime.pruneOccupancy(ctx, time.Now()); err != nil {
		t.Fatalf("occupancy prune: %v", err)
	}
	if err := runtime.sampleCapacity(ctx, testCapacityMinute(time.Now())); err != nil {
		t.Fatalf("capacity sample: %v", err)
	}
	if err := runtime.sampleCapacity(ctx, testCapacityMinute(time.Now().Add(-91*24*time.Hour))); err != nil {
		t.Fatalf("old capacity sample: %v", err)
	}
	if err := runtime.reportPlan(ctx, "room.test", perfPlan{Rooms: 8, RoomIDBase: 0}); err != nil {
		t.Fatalf("machine plan: %v", err)
	}
	if _, err := runtime.pruneRegistry(ctx, time.Now()); err != nil {
		t.Fatalf("registry prune: %v", err)
	}
	if removed, err := runtime.pruneCapacity(ctx, time.Now()); err != nil || removed != 1 {
		t.Fatalf("capacity prune with its cascade: removed=%d err=%v", removed, err)
	}
	var count int
	if err := runtime.pool.QueryRow(ctx, `SELECT count(*) FROM ops.admin_users`).Scan(&count); err == nil {
		t.Fatal("runtime read operator accounts")
	}
	if _, err := runtime.pool.Exec(ctx, `UPDATE schema_migrations SET checksum='x' WHERE version=1`); err == nil {
		t.Fatal("runtime changed migration history")
	}
	if _, err := openStoreWithPassword(ctx, ownerURL, passwordFile, "meta_runtime"); err == nil {
		t.Fatal("runtime accepted owner database role")
	}
}

func TestOccupancySamplerKeepsOneFleetSnapshotPerMinute(t *testing.T) {
	st, _ := freshStore(t)
	ctx := context.Background()
	status := &roomStatus{V: statusVersion, Port: 7001, Host: "room.test", Free: 3,
		Seated: 1, Away: 0, Capacity: 4, PeriodMs: 1000}
	if accepted, err := st.reportRoom(ctx, 9, 1, status); err != nil || !accepted {
		t.Fatalf("room status: %v %v", accepted, err)
	}
	minute := time.Now().UTC().Truncate(time.Minute)
	if err := st.sampleOccupancy(ctx, minute); err != nil {
		t.Fatal(err)
	}
	status.Seated, status.Free = 2, 2
	if accepted, err := st.reportRoom(ctx, 9, 1, status); err != nil || !accepted {
		t.Fatalf("updated status: %v %v", accepted, err)
	}
	if err := st.sampleOccupancy(ctx, minute); err != nil {
		t.Fatal(err)
	}
	var seated, rows int
	if err := st.pool.QueryRow(ctx, `SELECT seated FROM activity.occupancy_minutes WHERE minute_utc=$1 AND room_id=9`, minute).Scan(&seated); err != nil {
		t.Fatal(err)
	}
	if err := st.pool.QueryRow(ctx, `SELECT rooms_seen FROM activity.occupancy_runs WHERE minute_utc=$1`, minute).Scan(&rows); err != nil {
		t.Fatal(err)
	}
	if seated != 1 || rows != 1 {
		t.Fatalf("minute changed after retry: seated=%d rooms=%d", seated, rows)
	}
}

func TestOccupancySamplerKeepsSilentRoomInFleetCoverage(t *testing.T) {
	st, _ := freshStore(t)
	ctx := context.Background()
	status := &roomStatus{V: statusVersion, Port: 7001, Host: "room.test", Free: 3,
		Seated: 1, Away: 0, Capacity: 4, PeriodMs: 1000}
	if accepted, err := st.reportRoom(ctx, 9, 1, status); err != nil || !accepted {
		t.Fatalf("room status: %v %v", accepted, err)
	}
	if _, err := st.pool.Exec(ctx, `UPDATE public.rooms SET seen_at = now() - interval '2 hours' WHERE id=9`); err != nil {
		t.Fatal(err)
	}
	minute := time.Now().UTC().Truncate(time.Minute)
	if err := st.sampleOccupancy(ctx, minute); err != nil {
		t.Fatal(err)
	}
	var seen, live, details int
	if err := st.pool.QueryRow(ctx, `SELECT rooms_seen, live_rooms FROM activity.occupancy_runs
		WHERE minute_utc=$1`, minute).Scan(&seen, &live); err != nil {
		t.Fatal(err)
	}
	if err := st.pool.QueryRow(ctx, `SELECT count(*) FROM activity.occupancy_minutes WHERE minute_utc=$1`, minute).Scan(&details); err != nil {
		t.Fatal(err)
	}
	if seen != 1 || live != 0 || details != 0 {
		t.Fatalf("silent room coverage: known=%d live=%d detail=%d", seen, live, details)
	}
}

func TestOccupancyRetentionPrunesOldRunsAndTheirRoomRows(t *testing.T) {
	st, _ := freshStore(t)
	ctx := context.Background()
	now := time.Now().UTC().Truncate(time.Minute)
	old := now.Add(-91 * 24 * time.Hour)
	if _, err := st.pool.Exec(ctx, `INSERT INTO activity.occupancy_runs (minute_utc, sampled_at)
		SELECT $1::timestamptz + minute * interval '1 minute', $1::timestamptz + minute * interval '1 minute'
		FROM generate_series(0, 500) AS minute
		UNION ALL SELECT $2, $2`, old, now); err != nil {
		t.Fatal(err)
	}
	if _, err := st.pool.Exec(ctx, `INSERT INTO activity.occupancy_minutes
		(minute_utc, room_id, incarnation, seated, away, capacity, alive, room_seen_at)
		VALUES ($1, 1, 1, 1, 0, 4, true, $1)`, old); err != nil {
		t.Fatal(err)
	}
	count, err := st.pruneOccupancy(ctx, now)
	if err != nil || count != 500 {
		t.Fatalf("prune count=%d err=%v", count, err)
	}
	count, err = st.pruneOccupancy(ctx, now)
	if err != nil || count != 1 {
		t.Fatalf("prune remaining count=%d err=%v", count, err)
	}
	var runs, rooms int
	if err := st.pool.QueryRow(ctx, `SELECT count(*) FROM activity.occupancy_runs`).Scan(&runs); err != nil {
		t.Fatal(err)
	}
	if err := st.pool.QueryRow(ctx, `SELECT count(*) FROM activity.occupancy_minutes`).Scan(&rooms); err != nil {
		t.Fatal(err)
	}
	if runs != 1 || rooms != 0 {
		t.Fatalf("retention left runs=%d room_rows=%d", runs, rooms)
	}
}

func TestAdminRolesReadOnlyViewsAndAppendOnlyAudit(t *testing.T) {
	st, _ := freshStore(t)
	ctx := context.Background()
	var viewRead, profileRead, profileTableRead, tableRead, auditInsert, auditDelete, credentialUpdate, totpFenceUpdate bool
	err := st.pool.QueryRow(ctx, `SELECT
		has_table_privilege('meta_admin_read', 'ops.v_players', 'SELECT'),
		has_table_privilege('meta_admin_read', 'ops.v_player_profiles', 'SELECT'),
		has_table_privilege('meta_admin_read', 'public.profile_docs', 'SELECT'),
		has_table_privilege('meta_admin_read', 'public.players', 'SELECT'),
		has_table_privilege('meta_audit_writer', 'ops.audit_actions', 'INSERT'),
		has_table_privilege('meta_audit_writer', 'ops.audit_actions', 'DELETE'),
		has_column_privilege('meta_admin_service', 'ops.admin_users', 'role', 'UPDATE'),
		has_column_privilege('meta_admin_service', 'ops.admin_users', 'last_totp_step', 'UPDATE')`).Scan(
		&viewRead, &profileRead, &profileTableRead, &tableRead, &auditInsert, &auditDelete, &credentialUpdate, &totpFenceUpdate)
	if err != nil {
		t.Fatal(err)
	}
	if !profileRead || profileTableRead {
		t.Fatalf("admin profile read: view=%v table=%v", profileRead, profileTableRead)
	}
	if !viewRead || tableRead || !auditInsert || auditDelete || credentialUpdate || !totpFenceUpdate {
		t.Fatalf("admin privileges: view=%v table=%v audit_insert=%v audit_delete=%v credential_update=%v totp_update=%v",
			viewRead, tableRead, auditInsert, auditDelete, credentialUpdate, totpFenceUpdate)
	}
}

func TestAdminServiceRoleCanUseOnlyGrantedViewsAndAuditInsert(t *testing.T) {
	st, _ := freshStore(t)
	ctx := context.Background()
	tx, err := st.pool.Begin(ctx)
	if err != nil {
		t.Fatal(err)
	}
	defer tx.Rollback(ctx)
	if _, err := tx.Exec(ctx, `SET LOCAL ROLE meta_admin_service`); err != nil {
		t.Fatal(err)
	}
	for _, query := range []string{
		`SELECT count(*) FROM ops.v_players`,
		`SELECT count(*) FROM ops.v_rooms`,
		`SELECT count(*) FROM activity.v_occupancy_runs`,
		`SELECT count(*) FROM activity.v_capacity_runs`,
		`SELECT count(*) FROM activity.v_machine_minutes`,
		`SELECT count(*) FROM activity.v_fleet_minutes`,
		`SELECT count(*) FROM activity.v_api_minutes`,
		`INSERT INTO ops.audit_actions (action, result) VALUES ('test', 'allowed')`,
	} {
		if _, err := tx.Exec(ctx, query); err != nil {
			t.Fatalf("restricted role query %q: %v", query, err)
		}
	}
	var direct bool
	if err := tx.QueryRow(ctx, `SELECT has_table_privilege(current_user, 'public.players', 'SELECT')`).Scan(&direct); err != nil {
		t.Fatal(err)
	}
	if direct {
		t.Fatal("restricted role can read the game players table")
	}
}

func TestAuditActionsRejectChangesAfterInsert(t *testing.T) {
	st, _ := freshStore(t)
	ctx := context.Background()
	var id int64
	if err := st.pool.QueryRow(ctx, `INSERT INTO ops.audit_actions (action, result) VALUES ('login', 'denied') RETURNING id`).Scan(&id); err != nil {
		t.Fatal(err)
	}
	for _, query := range []string{
		`UPDATE ops.audit_actions SET result = 'allowed' WHERE id = $1`,
		`DELETE FROM ops.audit_actions WHERE id = $1`,
	} {
		if _, err := st.pool.Exec(ctx, query, id); err == nil {
			t.Fatalf("audit mutation succeeded: %s", query)
		}
	}
	if _, err := st.pool.Exec(ctx, `TRUNCATE ops.audit_actions`); err == nil {
		t.Fatal("audit truncate succeeded")
	}
}

// The schema is one version the server checks for, run by the database's
// owner, with the roles the deploy provisions.
func TestSchemaIsOneVersionWithItsRoles(t *testing.T) {
	st, _ := freshStore(t)
	ctx := t.Context()
	if migrations[len(migrations)-1].version != SchemaVersion {
		t.Fatalf("newest migration %d, SchemaVersion %d", migrations[len(migrations)-1].version, SchemaVersion)
	}
	if err := st.checkSchema(ctx); err != nil {
		t.Fatal(err)
	}
	if err := st.checkOwner(ctx); err != nil {
		t.Fatal(err)
	}
	roles := []string{RoleRuntime, RoleAdminService, RoleBackup, "meta_admin_read", "meta_admin_auth", "meta_audit_writer"}
	var found int
	if err := st.pool.QueryRow(ctx, `SELECT count(*) FROM pg_roles WHERE rolname = ANY($1)`, roles).Scan(&found); err != nil || found != len(roles) {
		t.Fatalf("roles found %d of %d: %v", found, len(roles), err)
	}
}
