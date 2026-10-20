package admin

import (
	"context"
	"errors"
	"strconv"
	"strings"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"
)

var (
	errInvalidCredentials = errors.New("invalid credentials")
	errNoSession          = errors.New("admin session unavailable")
	errNoPlayer           = errors.New("player not found")
)

type adminStore struct {
	pool      *pgxpool.Pool
	dummyHash string
}

type adminUser struct {
	ID       int64  `json:"id"`
	Username string `json:"username"`
	Role     string `json:"role"`
}

type playerProjection struct {
	ID             int64      `json:"id"`
	Login          *string    `json:"login"`
	CreatedAt      time.Time  `json:"created_at"`
	ProfileRev     *int64     `json:"profile_rev"`
	ProfileSavedAt *time.Time `json:"profile_saved_at"`
}

type occupancyHour struct {
	HourStart      time.Time `json:"hour_start"`
	Samples        int       `json:"samples"`
	PartialSamples int       `json:"partial_samples"`
	SampleStatus   string    `json:"sample_status"`
	PeakSeated     int       `json:"peak_seated"`
	PeakAway       int       `json:"peak_away"`
	LastSampleAt   time.Time `json:"last_sample_at"`
}

func (s *adminStore) knownRooms(ctx context.Context, now time.Time) ([]metaRoom, error) {
	rows, err := s.pool.Query(ctx, `SELECT id, incarnation, host, port, free, seated, away,
		capacity, period_ms, seen_at FROM ops.v_rooms ORDER BY id LIMIT 501`)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	rooms := []metaRoom{}
	for rows.Next() {
		var room metaRoom
		var seenAt time.Time
		var periodMs int
		if err := rows.Scan(&room.ID, &room.Incarnation, &room.Host, &room.Port,
			&room.Free, &room.Seated, &room.Away, &room.Capacity, &periodMs, &seenAt); err != nil {
			return nil, err
		}
		room.AgeS = now.Sub(seenAt).Seconds()
		room.Alive = room.AgeS >= 0 && room.AgeS < float64(periodMs*3)/1000
		rooms = append(rooms, room)
		if len(rooms) > 500 {
			return nil, errors.New("known room registry exceeds 500 rows")
		}
	}
	return rooms, rows.Err()
}

func (s *adminStore) occupancyHistory(ctx context.Context, now time.Time) ([]occupancyHour, error) {
	start := now.UTC().Truncate(time.Hour).Add(-23 * time.Hour)
	rows, err := s.pool.Query(ctx, `WITH minute AS (
		SELECT r.minute_utc, r.sampled_at, r.rooms_seen, r.live_rooms,
			COALESCE(sum(m.seated) FILTER (WHERE m.alive), 0)::integer AS seated,
			COALESCE(sum(m.away) FILTER (WHERE m.alive), 0)::integer AS away
		FROM activity.v_occupancy_runs AS r
		LEFT JOIN activity.v_occupancy_minutes AS m ON m.minute_utc = r.minute_utc
		WHERE r.minute_utc >= $1 AND r.minute_utc <= $2
		GROUP BY r.minute_utc, r.sampled_at, r.rooms_seen, r.live_rooms
	)
	SELECT date_trunc('hour', minute_utc), count(*)::integer,
		count(*) FILTER (WHERE rooms_seen = 0 OR live_rooms < rooms_seen)::integer,
		max(seated), max(away), max(sampled_at)
	FROM minute GROUP BY 1 ORDER BY 1`, start, now)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	hours := []occupancyHour{}
	for rows.Next() {
		var hour occupancyHour
		if err := rows.Scan(&hour.HourStart, &hour.Samples, &hour.PartialSamples, &hour.PeakSeated,
			&hour.PeakAway, &hour.LastSampleAt); err != nil {
			return nil, err
		}
		hour.SampleStatus = "observed"
		if hour.PartialSamples > 0 {
			hour.SampleStatus = "partial"
		}
		hours = append(hours, hour)
	}
	return hours, rows.Err()
}

func (s *adminStore) login(ctx context.Context, username, password string, now time.Time) ([]byte, adminUser, error) {
	var user adminUser
	tx, err := s.pool.Begin(ctx)
	if err != nil {
		return nil, user, err
	}
	defer tx.Rollback(ctx)
	var passwordHash string
	var disabled bool
	err = tx.QueryRow(ctx, `SELECT id, username, role, password_hash, disabled
		FROM ops.admin_users WHERE username = $1`, username).Scan(
		&user.ID, &user.Username, &user.Role, &passwordHash, &disabled)
	if errors.Is(err, pgx.ErrNoRows) {
		verifyPassword(s.dummyHash, password)
		return nil, user, auditDenied(ctx, tx, username)
	}
	if err != nil {
		return nil, user, err
	}
	if disabled || !verifyPassword(passwordHash, password) {
		return nil, adminUser{}, auditDenied(ctx, tx, username)
	}
	cookie, err := newSessionSecret()
	if err != nil {
		return nil, adminUser{}, err
	}
	if _, err := tx.Exec(ctx, `INSERT INTO ops.admin_sessions
		(secret_hash, user_id, created_at, last_seen_at, idle_expires_at, absolute_expires_at)
		VALUES ($1, $2, $3, $3, $4, $5)`, sessionHash(cookie), user.ID, now,
		now.Add(30*time.Minute), now.Add(12*time.Hour)); err != nil {
		return nil, adminUser{}, err
	}
	if _, err := tx.Exec(ctx, `INSERT INTO ops.audit_actions (user_id, action, target, result)
		VALUES ($1, 'login', $2, 'allowed')`, user.ID, username); err != nil {
		return nil, adminUser{}, err
	}
	if err := tx.Commit(ctx); err != nil {
		return nil, adminUser{}, err
	}
	return cookie, user, nil
}

func auditDenied(ctx context.Context, tx pgx.Tx, username string) error {
	if _, err := tx.Exec(ctx, `INSERT INTO ops.audit_actions (action, target, result)
		VALUES ('login', $1, 'denied')`, username); err != nil {
		return err
	}
	if err := tx.Commit(ctx); err != nil {
		return err
	}
	return errInvalidCredentials
}

func (s *adminStore) session(ctx context.Context, cookie []byte, now time.Time) (adminUser, error) {
	var user adminUser
	var lastSeen time.Time
	err := s.pool.QueryRow(ctx, `SELECT u.id, u.username, u.role, s.last_seen_at
		FROM ops.admin_sessions AS s JOIN ops.admin_users AS u ON u.id = s.user_id
		WHERE s.secret_hash = $1 AND s.revoked_at IS NULL AND NOT u.disabled
		AND s.idle_expires_at > $2 AND s.absolute_expires_at > $2`, sessionHash(cookie), now).Scan(
		&user.ID, &user.Username, &user.Role, &lastSeen)
	if errors.Is(err, pgx.ErrNoRows) {
		return user, errNoSession
	}
	if err != nil {
		return user, err
	}
	if now.Sub(lastSeen) >= time.Minute {
		if _, err := s.pool.Exec(ctx, `UPDATE ops.admin_sessions
			SET last_seen_at = $2, idle_expires_at = LEAST(absolute_expires_at, $2::timestamptz + interval '30 minutes')
			WHERE secret_hash = $1 AND revoked_at IS NULL`, sessionHash(cookie), now); err != nil {
			return adminUser{}, err
		}
	}
	return user, nil
}

func (s *adminStore) logout(ctx context.Context, cookie []byte, now time.Time) error {
	tx, err := s.pool.Begin(ctx)
	if err != nil {
		return err
	}
	defer tx.Rollback(ctx)
	var userID int64
	err = tx.QueryRow(ctx, `UPDATE ops.admin_sessions SET revoked_at = $2
		WHERE secret_hash = $1 AND revoked_at IS NULL RETURNING user_id`, sessionHash(cookie), now).Scan(&userID)
	if errors.Is(err, pgx.ErrNoRows) {
		return errNoSession
	}
	if err != nil {
		return err
	}
	if _, err := tx.Exec(ctx, `INSERT INTO ops.audit_actions (user_id, action, result)
		VALUES ($1, 'logout', 'applied')`, userID); err != nil {
		return err
	}
	return tx.Commit(ctx)
}

func (s *adminStore) players(ctx context.Context, query string, after int64, limit int) ([]playerProjection, error) {
	if limit < 1 || limit > 50 {
		return nil, errors.New("player page limit must be 1..50")
	}
	query = strings.TrimSpace(query)
	if id, err := strconv.ParseInt(query, 10, 64); err == nil && id > 0 {
		if id <= after {
			return []playerProjection{}, nil
		}
		player, err := s.player(ctx, id)
		if errors.Is(err, errNoPlayer) {
			return []playerProjection{}, nil
		}
		if err != nil {
			return nil, err
		}
		return []playerProjection{player}, nil
	}
	pattern := strings.NewReplacer(`\`, `\\`, `%`, `\%`, `_`, `\_`).Replace(strings.ToLower(query))
	rows, err := s.pool.Query(ctx, `SELECT id, login, created_at, profile_rev, profile_saved_at
		FROM ops.v_players WHERE id > $1 AND ($2 = '' OR lower(login) LIKE $2 || '%' ESCAPE '\')
		ORDER BY id LIMIT $3`, after, pattern, limit)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	items := make([]playerProjection, 0, limit)
	for rows.Next() {
		var item playerProjection
		if err := rows.Scan(&item.ID, &item.Login, &item.CreatedAt, &item.ProfileRev, &item.ProfileSavedAt); err != nil {
			return nil, err
		}
		items = append(items, item)
	}
	return items, rows.Err()
}

func (s *adminStore) player(ctx context.Context, id int64) (playerProjection, error) {
	var item playerProjection
	err := s.pool.QueryRow(ctx, `SELECT id, login, created_at, profile_rev, profile_saved_at
		FROM ops.v_players WHERE id = $1`, id).Scan(
		&item.ID, &item.Login, &item.CreatedAt, &item.ProfileRev, &item.ProfileSavedAt)
	if errors.Is(err, pgx.ErrNoRows) {
		return item, errNoPlayer
	}
	return item, err
}

// The saved profile, or nil when the player has none yet.
func (s *adminStore) playerProfile(ctx context.Context, id int64) (*playerProfile, error) {
	var rev int64
	var savedAt time.Time
	var doc []byte
	err := s.pool.QueryRow(ctx, `SELECT rev, updated_at, doc FROM ops.v_player_profiles WHERE account = $1`, id).Scan(&rev, &savedAt, &doc)
	if errors.Is(err, pgx.ErrNoRows) || (err == nil && rev == 0) {
		return nil, nil
	}
	if err != nil {
		return nil, err
	}
	format, systems, err := parseProfile(doc)
	if err != nil {
		return nil, err
	}
	return &playerProfile{Rev: rev, SavedAt: savedAt, Format: format, Systems: systems}, nil
}

func (s *adminStore) auditPlayerRead(ctx context.Context, userID, playerID int64) error {
	_, err := s.pool.Exec(ctx, `INSERT INTO ops.audit_actions (user_id, action, target, result)
		VALUES ($1, 'player_read', $2, 'allowed')`, userID, strconv.FormatInt(playerID, 10))
	return err
}
