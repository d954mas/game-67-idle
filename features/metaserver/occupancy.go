package metaserver

import (
	"context"
	"log/slog"
	"time"

	"github.com/jackc/pgx/v5"
)

func (s *store) sampleOccupancy(ctx context.Context, at time.Time) error {
	minute := at.UTC().Truncate(time.Minute)
	tx, err := s.pool.Begin(ctx)
	if err != nil {
		return err
	}
	defer tx.Rollback(ctx)
	var sampledAt time.Time
	err = tx.QueryRow(ctx, `INSERT INTO activity.occupancy_runs (minute_utc, sampled_at)
		VALUES ($1, clock_timestamp()) ON CONFLICT (minute_utc) DO NOTHING RETURNING sampled_at`, minute).Scan(&sampledAt)
	if err == pgx.ErrNoRows {
		return tx.Commit(ctx)
	}
	if err != nil {
		return err
	}
	_, err = tx.Exec(ctx, `INSERT INTO activity.occupancy_minutes
		(minute_utc, room_id, incarnation, seated, away, capacity, alive, room_seen_at)
		SELECT $1, id, incarnation, seated, away, capacity,
			seen_at > $2::timestamptz - (period_ms::bigint * $3::bigint) * interval '1 millisecond',
			seen_at
		FROM public.rooms WHERE seen_at > $2::timestamptz - interval '1 hour'`, minute, sampledAt, roomStalePeriods)
	if err != nil {
		return err
	}
	_, err = tx.Exec(ctx, `UPDATE activity.occupancy_runs AS run SET
		rooms_seen = (SELECT count(*) FROM public.rooms),
		live_rooms = (SELECT count(*) FROM activity.occupancy_minutes WHERE minute_utc = $1 AND alive)
		WHERE run.minute_utc = $1`, minute)
	if err != nil {
		return err
	}
	return tx.Commit(ctx)
}

func (s *store) pruneOccupancy(ctx context.Context, at time.Time) (int64, error) {
	cutoff := at.UTC().Add(-90 * 24 * time.Hour).Truncate(time.Minute)
	tag, err := s.pool.Exec(ctx, `WITH expired AS (
		SELECT minute_utc FROM activity.occupancy_runs
		WHERE minute_utc < $1 ORDER BY minute_utc LIMIT 500
	)
	DELETE FROM activity.occupancy_runs WHERE minute_utc IN
		(SELECT minute_utc FROM expired)`, cutoff)
	return tag.RowsAffected(), err
}

func (s *store) runOccupancySampler(ctx context.Context, log *slog.Logger) {
	for {
		wait := time.Until(time.Now().UTC().Truncate(time.Minute).Add(time.Minute + 2*time.Second))
		timer := time.NewTimer(wait)
		select {
		case <-ctx.Done():
			timer.Stop()
			return
		case <-timer.C:
		}
		sampleCtx, cancel := context.WithTimeout(ctx, 2*time.Second)
		err := s.sampleOccupancy(sampleCtx, time.Now())
		cancel()
		if err != nil && ctx.Err() == nil {
			log.Warn("occupancy sample failed", "err", err)
		}
	}
}

// runRetention drops expired minutes in batches: hourly, at once again while
// a full batch came back, and after a minute when a pass failed.
func runRetention(ctx context.Context, log *slog.Logger, what string, prune func(context.Context, time.Time) (int64, error)) {
	for {
		pruneCtx, cancel := context.WithTimeout(ctx, 5*time.Second)
		removed, err := prune(pruneCtx, time.Now())
		cancel()
		if err != nil && ctx.Err() == nil {
			log.Warn(what+" retention failed", "err", err)
		}
		wait := time.Hour
		if err != nil {
			wait = time.Minute
		} else if removed == 500 {
			wait = 100 * time.Millisecond
		}
		timer := time.NewTimer(wait)
		select {
		case <-ctx.Done():
			timer.Stop()
			return
		case <-timer.C:
		}
	}
}
