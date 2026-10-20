package metaserver

import (
	"bytes"
	"context"
	"crypto/rand"
	"crypto/sha256"
	_ "embed"
	"encoding/base64"
	"errors"
	"fmt"
	"os"
	"strings"
	"time"

	"github.com/jackc/pgerrcode"
	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgconn"
	"github.com/jackc/pgx/v5/pgxpool"
)

//go:embed schema.sql
var schemaSQL string

// SchemaVersion is the newest migration; the server refuses a database
// at any other version.
const SchemaVersion = 1

var migrations = []struct {
	version int
	sql     string
}{{1, schemaSQL}}

// store is the only game API code that talks to PostgreSQL: plain SQL over a small pool.
type store struct {
	pool   *pgxpool.Pool
	health *dbHealth
	// The share of a room's seats, in percent, past which entry prefers
	// to open an empty room: players meet, and no room is crammed.
	comfort int
}

func openStore(ctx context.Context, url string) (*store, error) {
	return openStoreWithPassword(ctx, url, "", "")
}

func openStoreWithPassword(ctx context.Context, url, passwordFile, expectedRole string) (*store, error) {
	cfg, err := pgxpool.ParseConfig(url)
	if err != nil {
		return nil, fmt.Errorf("database url: %w", err)
	}
	if expectedRole != "" && cfg.ConnConfig.User != expectedRole {
		return nil, fmt.Errorf("database role %q is not %s", cfg.ConnConfig.User, expectedRole)
	}
	if passwordFile != "" {
		if cfg.ConnConfig.Password != "" {
			return nil, errors.New("database URL must not contain a password when a password file is set")
		}
		cfg.ConnConfig.Password, err = readDBPassword(passwordFile)
		if err != nil {
			return nil, err
		}
	}
	// Enough for a burst of saves from a few dozen rooms without a queue;
	// the database side is small and local.
	cfg.MaxConns = 32
	cfg.MaxConnLifetime = 30 * time.Minute
	cfg.MaxConnIdleTime = 5 * time.Minute
	health := &dbHealth{}
	cfg.ConnConfig.Tracer = health
	pool, err := pgxpool.NewWithConfig(ctx, cfg)
	if err != nil {
		return nil, err
	}
	if err := pool.Ping(ctx); err != nil {
		pool.Close()
		return nil, fmt.Errorf("database ping: %w", err)
	}
	return &store{pool: pool, health: health, comfort: defaultComfortPercent}, nil
}

func readDBPassword(path string) (string, error) {
	secret, err := os.ReadFile(path)
	if err != nil {
		return "", err
	}
	password := strings.TrimSpace(string(secret))
	if len(password) < 32 || len(password) > 256 {
		return "", errors.New("database password file must contain a 32..256 character password")
	}
	return password, nil
}

func (s *store) close() { s.pool.Close() }

func (s *store) migrate(ctx context.Context) error {
	tx, err := s.pool.Begin(ctx)
	if err != nil {
		return err
	}
	defer tx.Rollback(ctx)
	if _, err := tx.Exec(ctx, `SELECT pg_advisory_xact_lock(7454133)`); err != nil {
		return err
	}
	if _, err := tx.Exec(ctx, `CREATE TABLE IF NOT EXISTS schema_migrations (
		version integer PRIMARY KEY,
		checksum text NOT NULL,
		applied_at timestamptz NOT NULL DEFAULT now()
	)`); err != nil {
		return err
	}
	for _, migration := range migrations {
		checksum := fmt.Sprintf("%x", sha256.Sum256([]byte(migration.sql)))
		var applied string
		err = tx.QueryRow(ctx, `SELECT checksum FROM schema_migrations WHERE version = $1`, migration.version).Scan(&applied)
		switch {
		case errors.Is(err, pgx.ErrNoRows):
			if _, err := tx.Exec(ctx, migration.sql); err != nil {
				return fmt.Errorf("schema migration %d: %w", migration.version, err)
			}
			if _, err := tx.Exec(ctx, `INSERT INTO schema_migrations (version, checksum) VALUES ($1, $2)`, migration.version, checksum); err != nil {
				return err
			}
		case err != nil:
			return err
		case applied != checksum:
			return fmt.Errorf("schema migration %d checksum mismatch", migration.version)
		}
	}
	return tx.Commit(ctx)
}

func (s *store) checkSchema(ctx context.Context) error {
	// Equal, not at least: an older image on a newer schema must stop, not
	// write rows that schema no longer means; that is what refuses a rollback.
	var newest int
	if err := s.pool.QueryRow(ctx, `SELECT coalesce(max(version), 0) FROM schema_migrations`).Scan(&newest); err != nil {
		return err
	}
	if newest != SchemaVersion {
		return fmt.Errorf("database schema is %d, this server runs schema %d", newest, SchemaVersion)
	}
	return nil
}

func (s *store) provisionRole(ctx context.Context, role, password string) error {
	if role != RoleAdminService && role != RoleRuntime && role != RoleBackup {
		return errors.New("unsupported database role")
	}
	var statement string
	if err := s.pool.QueryRow(ctx, `SELECT format('ALTER ROLE %I LOGIN PASSWORD %L', $1::text, $2::text)`, role, password).Scan(&statement); err != nil {
		return err
	}
	_, err := s.pool.Exec(ctx, statement)
	return err
}

// A guest secret is 32 random bytes; the client keeps the base64url form
// and the database keeps only SHA-256 of the bytes, so a leaked table logs
// nobody in.
const secretBytes = 32

func newSecret() (string, error) {
	var raw [secretBytes]byte
	if _, err := rand.Read(raw[:]); err != nil {
		return "", err
	}
	return base64.RawURLEncoding.EncodeToString(raw[:]), nil
}

func secretHash(secret string) ([]byte, bool) {
	raw, err := base64.RawURLEncoding.DecodeString(secret)
	if err != nil || len(raw) != secretBytes {
		return nil, false
	}
	sum := sha256.Sum256(raw)
	return sum[:], true
}

var errUnknownSecret = errors.New("unknown secret")

// createGuest makes a player and the session that opens it, in one
// statement: nobody ever holds an id without a way in.
func (s *store) createGuest(ctx context.Context, hash []byte) (int64, error) {
	var id int64
	err := s.pool.QueryRow(ctx,
		`WITH p AS (INSERT INTO players DEFAULT VALUES RETURNING id)
		 INSERT INTO sessions (secret_hash, player_id) SELECT $1, id FROM p RETURNING player_id`, hash).Scan(&id)
	return id, err
}

// A session resolved to its player: the login and password hash are empty
// for a guest.
type identity struct {
	id           int64
	login        string
	passwordHash string
}

// resolveSession is the one lookup behind every bearer call; it records
// the use.
func (s *store) resolveSession(ctx context.Context, hash []byte, now time.Time) (identity, error) {
	var who identity
	var login, passwordHash *string
	// The last use is a coarse fact (a session unused for months): writing
	// it on every entry would make every entry a disk write, so it moves
	// once a minute at most.
	err := s.pool.QueryRow(ctx,
		`UPDATE sessions SET last_used_at = $2::timestamptz FROM players
		 WHERE sessions.secret_hash = $1 AND players.id = sessions.player_id
		   AND (sessions.last_used_at IS NULL OR sessions.last_used_at < $2::timestamptz - interval '1 minute')
		 RETURNING players.id, players.login, players.password_hash`, hash, now).Scan(&who.id, &login, &passwordHash)
	if errors.Is(err, pgx.ErrNoRows) {
		err = s.pool.QueryRow(ctx,
			`SELECT players.id, players.login, players.password_hash FROM sessions JOIN players
			 ON players.id = sessions.player_id WHERE sessions.secret_hash = $1`, hash).Scan(&who.id, &login, &passwordHash)
	}
	if errors.Is(err, pgx.ErrNoRows) {
		return who, errUnknownSecret
	}
	if login != nil {
		who.login = *login
	}
	if passwordHash != nil {
		who.passwordHash = *passwordHash
	}
	return who, err
}

// newSession opens the player for a password that still stands: a login
// racing a password change lands only if the hash it verified is the one
// on record at the insert (false).
func (s *store) newSession(ctx context.Context, player int64, hash []byte, passwordHash string) (bool, error) {
	tag, err := s.pool.Exec(ctx,
		`INSERT INTO sessions (secret_hash, player_id)
		 SELECT $1, id FROM players WHERE id = $2 AND password_hash = $3`, hash, player, passwordHash)
	return tag.RowsAffected() == 1, err
}

func (s *store) deleteSession(ctx context.Context, hash []byte) error {
	_, err := s.pool.Exec(ctx, `DELETE FROM sessions WHERE secret_hash = $1`, hash)
	return err
}

var (
	errLoginTaken = errors.New("login taken")
	errRegistered = errors.New("already registered")
)

// register attaches a login to a player that has none. The UNIQUE index is
// the guard between two registrations of one login: the second one loses.
func (s *store) register(ctx context.Context, player int64, login, passwordHash string) error {
	tag, err := s.pool.Exec(ctx,
		`UPDATE players SET login = $2, password_hash = $3 WHERE id = $1 AND login IS NULL`, player, login, passwordHash)
	var pgErr *pgconn.PgError
	if errors.As(err, &pgErr) && pgErr.Code == pgerrcode.UniqueViolation {
		return errLoginTaken
	}
	if err == nil && tag.RowsAffected() == 0 {
		return errRegistered
	}
	return err
}

var errUnknownLogin = errors.New("unknown login")

func (s *store) findLogin(ctx context.Context, login string) (identity, error) {
	who := identity{login: login}
	err := s.pool.QueryRow(ctx, `SELECT id, password_hash FROM players WHERE login = $1`, login).Scan(&who.id, &who.passwordHash)
	if errors.Is(err, pgx.ErrNoRows) {
		return who, errUnknownLogin
	}
	return who, err
}

// setPassword lands the new hash and revokes every other session of the
// player in one statement, so no session outlives the old password.
func (s *store) setPassword(ctx context.Context, player int64, passwordHash string, keep []byte) error {
	_, err := s.pool.Exec(ctx,
		`WITH p AS (UPDATE players SET password_hash = $2 WHERE id = $1)
		 DELETE FROM sessions WHERE player_id = $1 AND secret_hash <> $3`, player, passwordHash, keep)
	return err
}

// reportRoom records a room's status and is the fence: a life below the
// one on record changes nothing and is told so (false), so a room that
// outlived its own restart cannot take the id back. A life has one
// address: a status of the same life from another host or port is
// refused the same way, so a ticket caught on the wire cannot move the
// players of a room elsewhere for as long as it lives.
func (s *store) reportRoom(ctx context.Context, id uint32, incarnation uint64, st *roomStatus) (bool, error) {
	accounts := st.Accounts
	if accounts == nil {
		accounts = []int64{}
	}
	// One statement, so the status and the departures it implies land
	// together or not at all. The previous accounts come from the
	// statement's snapshot, not a FOR UPDATE: a row this same statement
	// updates reads as nothing under a lock. The upsert's own row lock
	// orders two statuses of one room; a departure written twice is the
	// same row. An accepted status records who is gone since (pick's
	// return window), and only a departure pays for purging old rows.
	var stored int
	err := s.pool.QueryRow(ctx,
		`WITH prev AS (
		     SELECT accounts FROM rooms WHERE id = $1),
		 up AS (
		     INSERT INTO rooms (id, incarnation, port, free, accounts, seen_at,
		         seated, away, passes, pass_avg_us, pass_max_us, round_trip_ms, capacity, period_ms, host)
		     VALUES ($1, $2, $3, $4, $5, now(), $6, $7, $8, $9, $10, $11, $12, $13, $14)
		     ON CONFLICT (id) DO UPDATE SET incarnation = EXCLUDED.incarnation, port = EXCLUDED.port,
		         free = EXCLUDED.free, accounts = EXCLUDED.accounts, seen_at = EXCLUDED.seen_at,
		         seated = EXCLUDED.seated, away = EXCLUDED.away, passes = EXCLUDED.passes,
		         pass_avg_us = EXCLUDED.pass_avg_us, pass_max_us = EXCLUDED.pass_max_us,
		         round_trip_ms = EXCLUDED.round_trip_ms, capacity = EXCLUDED.capacity,
		         period_ms = EXCLUDED.period_ms, host = EXCLUDED.host
		     WHERE rooms.incarnation < EXCLUDED.incarnation
		        OR (rooms.incarnation = EXCLUDED.incarnation
		            AND rooms.host = EXCLUDED.host AND rooms.port = EXCLUDED.port)
		     RETURNING 1),
		 gone AS (
		     INSERT INTO last_rooms (account, room, left_at)
		     SELECT account, $1, now() FROM prev, unnest(prev.accounts) AS account
		     WHERE EXISTS (SELECT 1 FROM up) AND NOT (account = ANY($5::bigint[]))
		     ON CONFLICT (account) DO UPDATE SET room = EXCLUDED.room, left_at = EXCLUDED.left_at
		     RETURNING 1),
		 purged AS (
		     DELETE FROM last_rooms
		     WHERE EXISTS (SELECT 1 FROM gone) AND left_at < now() - $15::int * interval '1 second')
		 SELECT count(*) FROM up`,
		int64(id), int64(incarnation), int32(st.Port), st.Free, accounts,
		st.Seated, st.Away, st.Passes, st.PassAvgUs, st.PassMaxUs, st.RoundTripMs,
		st.Capacity, st.PeriodMs, st.Host, returnWindowSeconds).Scan(&stored)
	return err == nil && stored == 1, err
}

// returnWindowSeconds is how long after it left a room an account is
// still sent back there: a PvP lock (60 s) is kept in the room's memory,
// not the save, and a rejoin elsewhere would shed it.
const returnWindowSeconds = 70

var errNoRoom = errors.New("no room has a free seat")

// A room silent for this many of its own periods is not routed to. A
// status that times out is followed by the next one at once, so this
// is that many statuses lost in a row: two missed and a slow answer.
// Short, because a room that hangs is routed to until then, and every
// player sent there waits out a whole connect before asking again.
const roomStalePeriods = 3

// roomAlive is the SQL test of that, on the row's own period.
var roomAlive = fmt.Sprintf("seen_at > now() - (period_ms * %d) * interval '1 millisecond'", roomStalePeriods)

// roomAddress is where a player connects: the port the room reported and
// the host the proxy dials for it.
type roomAddress struct {
	id   uint32
	port uint16
	host string
}

const defaultComfortPercent = 70

// pick sends the player to the room already holding their account (a
// grace seat, a second tab, even in a full room), then to the room that
// held it within the return window, if it is alive with a free seat, so a
// lock that room remembers cannot be shed elsewhere. Otherwise players go
// where there is company but no crowd: the emptiest room among those
// with someone in them and no more than `comfort` percent of the seats
// taken; when every such room is past that share, an empty one; when
// there is none, the emptiest of the crowded. It books the seat, so the
// registry's count drops at once and a burst of entries spreads by these
// rules instead of every entry of one second landing in the same room and
// most of them coming back FULL. The room's next status restores the
// truth. Selection locks and rechecks the row before booking so contending
// picks cannot issue tickets against the same free-seat snapshot.
// A room that stopped reporting is skipped.
func (s *store) pick(ctx context.Context, player int64) (roomAddress, error) {
	var roomID int64
	var roomPort int32
	var host string
	err := s.pool.QueryRow(ctx,
		`WITH chosen AS (
		     SELECT id, ($1::bigint = ANY(accounts)) AS seated FROM rooms
		     WHERE `+roomAlive+` AND ($1::bigint = ANY(accounts) OR free > 0)
		     ORDER BY ($1::bigint = ANY(accounts)) DESC,
		         (id = (SELECT room FROM last_rooms WHERE account = $1
		                 AND left_at > now() - $3::int * interval '1 second')) IS TRUE DESC,
		         CASE WHEN free = capacity THEN 1
		              WHEN (capacity - free) * 100 <= capacity * $2::int THEN 0
		              ELSE 2 END,
		         free DESC, id
		     LIMIT 1 FOR UPDATE)
		 UPDATE rooms SET free = CASE WHEN chosen.seated THEN rooms.free ELSE greatest(rooms.free - 1, 0) END
		 FROM chosen WHERE rooms.id = chosen.id
		 RETURNING rooms.id, rooms.port, rooms.host`, player, s.comfort, returnWindowSeconds).Scan(&roomID, &roomPort, &host)
	if errors.Is(err, pgx.ErrNoRows) {
		return roomAddress{}, errNoRoom
	}
	return roomAddress{id: uint32(roomID), port: uint16(roomPort), host: host}, err
}

// roomHosts is every host alive rooms report, each with one of its ports.
func (s *store) roomHosts(ctx context.Context) ([]roomAddress, error) {
	rows, err := s.pool.Query(ctx, `SELECT host, min(port) FROM rooms WHERE `+roomAlive+` GROUP BY host ORDER BY host`)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var hosts []roomAddress
	for rows.Next() {
		var host string
		var port int32
		if err := rows.Scan(&host, &port); err != nil {
			return nil, err
		}
		hosts = append(hosts, roomAddress{port: uint16(port), host: host})
	}
	return hosts, rows.Err()
}

// ---- profile documents ----

var (
	errOlderLife = errors.New("an older life of the owning room")
	errNotOwner  = errors.New("another room or a newer life owns the profile")
	errStaleRev  = errors.New("the stored rev is not the one the save read")
	errNoAccount = errors.New("no such account")
	errTooLarge  = errors.New("the stored document is over the limit")
)

// loadDoc hands the room an account's document with its rev and
// makes this life of the room its owner: a second login seated in another
// room takes the profile over, and the first room's saves are refused from
// then on. A life older than the newest the registry knows for its room id
// cannot take it (errOlderLife), whichever room owns it now, nor can an
// older life of the owning room. A document over `maxBytes` is refused
// before anything is claimed (errTooLarge). A first visit is rev 0 and an
// empty document. The load writes no rev: ownership is not a change of the
// document. The registry row is read FOR SHARE, so a status of a newer life
// of the room cannot commit between the check and the claim.
func (s *store) loadDoc(ctx context.Context, account, room, incarnation int64, maxBytes int) (rev int64, doc []byte, err error) {
	tx, err := s.pool.Begin(ctx)
	if err != nil {
		return 0, nil, err
	}
	defer tx.Rollback(ctx)
	_, err = tx.Exec(ctx,
		`INSERT INTO profile_docs (account, owner_room, owner_incarnation) VALUES ($1, $2, $3)
		 ON CONFLICT (account) DO NOTHING`, account, room, incarnation)
	var pgErr *pgconn.PgError
	if errors.As(err, &pgErr) && pgErr.Code == pgerrcode.ForeignKeyViolation {
		return 0, nil, errNoAccount
	}
	if err != nil {
		return 0, nil, err
	}
	var ownerRoom, ownerIncarnation, size int64
	if err := tx.QueryRow(ctx,
		`SELECT rev, owner_room, owner_incarnation, octet_length(doc) FROM profile_docs WHERE account = $1 FOR UPDATE`,
		account).Scan(&rev, &ownerRoom, &ownerIncarnation, &size); err != nil {
		return 0, nil, err
	}
	var newest int64
	err = tx.QueryRow(ctx, `SELECT incarnation FROM rooms WHERE id = $1 FOR SHARE`, room).Scan(&newest)
	if err != nil && !errors.Is(err, pgx.ErrNoRows) {
		return 0, nil, err
	}
	switch {
	case newest > incarnation, ownerRoom == room && ownerIncarnation > incarnation:
		return 0, nil, errOlderLife
	case size > int64(maxBytes):
		return 0, nil, errTooLarge
	}
	if err := tx.QueryRow(ctx,
		`UPDATE profile_docs SET owner_room = $2, owner_incarnation = $3 WHERE account = $1 RETURNING doc`,
		account, room, incarnation).Scan(&doc); err != nil {
		return 0, nil, err
	}
	return rev, doc, tx.Commit(ctx)
}

// docWriter names the save that produced a stored rev: the room and the
// life it came from (the bearer's) and the save id the room gave it.
// All zero for rev 0.
type docWriter struct {
	room        int64
	incarnation int64
	saveID      int64
}

// docSaveEntry is one profile's save: the account, the rev the room read, the
// save id the room gave it and the document.
type docSaveEntry struct {
	account int64
	ifRev   int64
	saveID  int64
	doc     []byte
}

// saveResult is one entry's verdict: err nil with the rev it landed as, or
// errNotOwner, or errStaleRev with the rev stored now and its writer, or a
// database error (the outcome is unknown).
type saveResult struct {
	rev    int64
	stored docWriter
	err    error
}

// saveDocs is the compare-and-swap of every entry, each on its own: an
// entry lands only for the owning life of the owning room and only on the
// rev it names, which it then increments. An entry that is the stored rev's
// writer with the same document already landed (a retry after a lost
// answer) and gets that rev again, whoever owns the profile now; the same
// save id with another document is not a retry and is refused as stale.
// Otherwise nothing of it is written and its result says why: errNotOwner
// (the room lets the seat go), or errStaleRev with the rev stored now and its
// writer (the room reads it again). An account twice in one batch is saved
// as if its entries were sent one after the other. The error is a
// statement's own: the outcome of every entry is then unknown.
func (s *store) saveDocs(ctx context.Context, room, incarnation int64, entries []docSaveEntry) ([]saveResult, error) {
	var first, later []int
	seen := make(map[int64]bool, len(entries))
	for i, e := range entries {
		if seen[e.account] {
			later = append(later, i)
			continue
		}
		seen[e.account] = true
		first = append(first, i)
	}
	results := make([]saveResult, len(entries))
	if err := s.saveOnce(ctx, docWriter{room: room, incarnation: incarnation}, entries, first, results); err != nil {
		return nil, err
	}
	if len(later) == 0 {
		return results, nil
	}
	rest := make([]docSaveEntry, len(later))
	for j, i := range later {
		rest[j] = entries[i]
	}
	again, err := s.saveDocs(ctx, room, incarnation, rest)
	if err != nil {
		return nil, err
	}
	for j, i := range later {
		results[i] = again[j]
	}
	return results, nil
}

// saveOnce saves entries[at...], one per account. One statement locks their
// rows in account order, so two batches never deadlock, and writes every
// entry whose rev still matches; one more reads the rest under a lock to say
// why they were not written.
func (s *store) saveOnce(ctx context.Context, w docWriter, entries []docSaveEntry, at []int, results []saveResult) error {
	accounts := make([]int64, len(at))
	ifRevs := make([]int64, len(at))
	saveIDs := make([]int64, len(at))
	docs := make([][]byte, len(at))
	hashes := make([][]byte, len(at))
	for j, i := range at {
		e := entries[i]
		hash := sha256.Sum256(e.doc)
		accounts[j], ifRevs[j], saveIDs[j], docs[j], hashes[j] = e.account, e.ifRev, e.saveID, e.doc, hash[:]
	}
	rows, err := s.pool.Query(ctx,
		`WITH e AS (
		     SELECT * FROM unnest($3::bigint[], $4::bigint[], $5::bigint[], $6::bytea[], $7::bytea[])
		         AS e(account, if_rev, save_id, doc, hash)
		 ), locked AS (
		     SELECT account FROM profile_docs WHERE account IN (SELECT account FROM e) ORDER BY account FOR UPDATE
		 )
		 UPDATE profile_docs p SET rev = p.rev + 1, doc = e.doc, updated_at = now(),
		     writer_room = $1, writer_incarnation = $2, writer_save_id = e.save_id, writer_doc_hash = e.hash
		 FROM e JOIN locked USING (account)
		 WHERE p.account = e.account AND p.owner_room = $1 AND p.owner_incarnation = $2 AND p.rev = e.if_rev
		   AND (p.writer_room, p.writer_incarnation, p.writer_save_id) <> ($1, $2, e.save_id)
		 RETURNING p.account, p.rev`, w.room, w.incarnation, accounts, ifRevs, saveIDs, docs, hashes)
	if err != nil {
		return err
	}
	landed := make(map[int64]int64, len(at))
	for rows.Next() {
		var account, rev int64
		if err := rows.Scan(&account, &rev); err != nil {
			rows.Close()
			return err
		}
		landed[account] = rev
	}
	if err := rows.Err(); err != nil {
		return err
	}
	var missed []int64
	for _, i := range at {
		e := entries[i]
		if rev, ok := landed[e.account]; ok {
			results[i] = saveResult{rev: rev, stored: docWriter{room: w.room, incarnation: w.incarnation, saveID: e.saveID}}
		} else {
			missed = append(missed, e.account)
		}
	}
	if len(missed) == 0 {
		return nil
	}
	stored, err := s.docRows(ctx, missed)
	if err != nil {
		return err
	}
	for j, i := range at {
		e := entries[i]
		if _, ok := landed[e.account]; !ok {
			self := docWriter{room: w.room, incarnation: w.incarnation, saveID: e.saveID}
			results[i].rev, results[i].stored, results[i].err = judgeSave(stored[e.account], self, e.ifRev, hashes[j])
		}
	}
	return nil
}

// docRow is what a refused save is judged by.
type docRow struct {
	found  bool
	rev    int64
	owner  docWriter // room and incarnation only
	stored docWriter
	hash   []byte
}

// docRows reads the accounts' rows under a lock, in account order: each is
// the latest committed state, not one a racing save is still writing.
func (s *store) docRows(ctx context.Context, accounts []int64) (map[int64]docRow, error) {
	rows, err := s.pool.Query(ctx,
		`SELECT account, rev, owner_room, owner_incarnation, writer_room, writer_incarnation, writer_save_id, writer_doc_hash
		 FROM profile_docs WHERE account = ANY($1) ORDER BY account FOR UPDATE`, accounts)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	out := make(map[int64]docRow, len(accounts))
	for rows.Next() {
		var account int64
		r := docRow{found: true}
		if err := rows.Scan(&account, &r.rev, &r.owner.room, &r.owner.incarnation,
			&r.stored.room, &r.stored.incarnation, &r.stored.saveID, &r.hash); err != nil {
			return nil, err
		}
		out[account] = r
	}
	return out, rows.Err()
}

// judgeSave says why a save was not written, or that it had landed before.
func judgeSave(r docRow, w docWriter, ifRev int64, hash []byte) (int64, docWriter, error) {
	switch {
	case !r.found:
		// Never loaded, so nobody holds the seat it would be saved for.
		return 0, docWriter{}, errNotOwner
	case r.stored == w && bytes.Equal(r.hash, hash):
		return r.rev, r.stored, nil
	case r.stored == w:
		return r.rev, r.stored, errStaleRev
	case r.owner.room != w.room || r.owner.incarnation != w.incarnation:
		return 0, docWriter{}, errNotOwner
	case r.rev != ifRev:
		return r.rev, r.stored, errStaleRev
	}
	return r.rev, r.stored, errStaleRev
}

// ---- the admin console's reads: nothing here is on a player's path ----

type roomRow struct {
	ID          int64   `json:"id"`
	Incarnation int64   `json:"incarnation"`
	Port        int     `json:"port"`
	URL         string  `json:"url"`
	Free        int     `json:"free"`
	Seated      int     `json:"seated"`
	Away        int     `json:"away"`
	Accounts    []int64 `json:"accounts"`
	AgeS        float64 `json:"age_s"` // since the last status
	Alive       bool    `json:"alive"` // reported recently enough to be routed to
	Passes      int     `json:"passes"`
	PassAvgUs   int     `json:"pass_avg_us"`
	PassMaxUs   int     `json:"pass_max_us"`
	RoundTripMs int     `json:"round_trip_ms"`
	Capacity    int     `json:"capacity"`
	PeriodMs    int     `json:"period_ms"`
	Host        string  `json:"host"`
}

// listRooms is the registry as the router sees it, plus the rooms that
// went quiet within the hour; older rows are the fence's business only.
func (s *store) listRooms(ctx context.Context) ([]roomRow, error) {
	rows, err := s.pool.Query(ctx,
		`SELECT id, incarnation, port, free, accounts, extract(epoch FROM now() - seen_at),
		        `+roomAlive+`, seated, away, passes, pass_avg_us, pass_max_us, round_trip_ms,
		        capacity, period_ms, host
		 FROM rooms WHERE seen_at > now() - interval '1 hour' ORDER BY id`)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	out := []roomRow{}
	for rows.Next() {
		var r roomRow
		if err := rows.Scan(&r.ID, &r.Incarnation, &r.Port, &r.Free, &r.Accounts, &r.AgeS, &r.Alive,
			&r.Seated, &r.Away, &r.Passes, &r.PassAvgUs, &r.PassMaxUs, &r.RoundTripMs,
			&r.Capacity, &r.PeriodMs, &r.Host); err != nil {
			return nil, err
		}
		out = append(out, r)
	}
	return out, rows.Err()
}

type playerCounts struct {
	Guests   int64 `json:"guests"`
	Accounts int64 `json:"accounts"`
	Sessions int64 `json:"sessions"`
	Profiles int64 `json:"profiles"`
}

func (s *store) countPlayers(ctx context.Context) (playerCounts, error) {
	var c playerCounts
	err := s.pool.QueryRow(ctx,
		`SELECT (SELECT count(*) FROM players WHERE login IS NULL),
		        (SELECT count(*) FROM players WHERE login IS NOT NULL),
		        (SELECT count(*) FROM sessions), (SELECT count(*) FROM profile_docs)`).
		Scan(&c.Guests, &c.Accounts, &c.Sessions, &c.Profiles)
	return c, err
}
