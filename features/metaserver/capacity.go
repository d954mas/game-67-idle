package metaserver

import (
	"context"
	"log/slog"
	"sort"
	"time"

	"github.com/jackc/pgx/v5"
)

// capacityMinute is what one ended minute leaves for the console's history:
// the fleet's tick figures (nil without windows), each machine heard in it and
// the API's requests by route.
type capacityMinute struct {
	at       time.Time
	fleet    *minuteView
	machines []machineView
	api      map[string]endpointObservation
}

type machineSeats struct {
	rooms, live, seated, away, capacity int
}

// sampleCapacity writes one minute once; a retry of a written minute changes nothing.
func (s *store) sampleCapacity(ctx context.Context, m capacityMinute) error {
	minute := m.at.UTC().Truncate(time.Minute)
	tx, err := s.pool.Begin(ctx)
	if err != nil {
		return err
	}
	defer tx.Rollback(ctx)
	var sampledAt time.Time
	err = tx.QueryRow(ctx, `INSERT INTO activity.capacity_runs (minute_utc, sampled_at)
		VALUES ($1, clock_timestamp()) ON CONFLICT (minute_utc) DO NOTHING RETURNING sampled_at`, minute).Scan(&sampledAt)
	if err == pgx.ErrNoRows {
		return tx.Commit(ctx)
	}
	if err != nil {
		return err
	}
	// Seats as read just after the minute ended (the sampler wakes ~2 s late),
	// by the same liveness rule as the occupancy sample.
	rows, err := tx.Query(ctx, `SELECT host, count(*)::integer,
			count(*) FILTER (WHERE live)::integer,
			coalesce(sum(seated) FILTER (WHERE live), 0)::integer,
			coalesce(sum(away) FILTER (WHERE live), 0)::integer,
			coalesce(sum(capacity) FILTER (WHERE live), 0)::integer
		FROM (SELECT host, seated, away, capacity,
				seen_at > $1::timestamptz - (period_ms::bigint * $2::bigint) * interval '1 millisecond' AS live
			FROM public.rooms WHERE seen_at > $1::timestamptz - interval '1 hour') AS r
		GROUP BY host`, sampledAt, roomStalePeriods)
	if err != nil {
		return err
	}
	seats := map[string]machineSeats{}
	for rows.Next() {
		var host string
		var v machineSeats
		if err := rows.Scan(&host, &v.rooms, &v.live, &v.seated, &v.away, &v.capacity); err != nil {
			rows.Close()
			return err
		}
		seats[host] = v
	}
	rows.Close()
	if err := rows.Err(); err != nil {
		return err
	}
	load := map[string]*machineView{}
	hosts := make([]string, 0, len(seats)+len(m.machines))
	for i := range m.machines {
		load[m.machines[i].Host] = &m.machines[i]
		hosts = append(hosts, m.machines[i].Host)
	}
	for host := range seats {
		if load[host] == nil {
			hosts = append(hosts, host)
		}
	}
	sort.Strings(hosts)
	batch := &pgx.Batch{}
	for _, host := range hosts {
		v, l := seats[host], load[host]
		var cpu, user, system, softirq, steal, iowait, rxMbit, txMbit, rxPps, txPps *float64
		if l != nil {
			cpu, user, system, softirq, steal, iowait = &l.CPUPct, &l.UserPct, &l.SystemPct, &l.SoftirqPct, &l.StealPct, &l.IowaitPct
			rxMbit, txMbit, rxPps, txPps = &l.RxMbitS, &l.TxMbitS, &l.RxPps, &l.TxPps
		}
		batch.Queue(`INSERT INTO activity.machine_minutes (minute_utc, host, rooms, live_rooms, seated, away, capacity,
			cpu_pct, user_pct, system_pct, softirq_pct, steal_pct, iowait_pct, rx_mbit_s, tx_mbit_s, rx_pps, tx_pps)
			VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17)`,
			minute, host, v.rooms, v.live, v.seated, v.away, v.capacity,
			cpu, user, system, softirq, steal, iowait, rxMbit, txMbit, rxPps, txPps)
	}
	if f := m.fleet; f != nil {
		batch.Queue(`INSERT INTO activity.fleet_minutes (minute_utc, windows, avg_seated, cpu_ms_per_player_s,
			step_p99_us, replicate_p99_us, late_p99_us, late_ticks, dropped_ticks, rx_msg_per_s, tx_writes_per_s, tx_mbit_s)
			VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12)`,
			minute, f.Windows, f.AvgSeated, f.CPUMsPerPlayerS, int64(f.StepP99Us), int64(f.ReplicateP99Us), int64(f.LateP99Us),
			int64(f.LateTicks), int64(f.DroppedTicks), f.RxMsgPerS, f.TxWritesPerS, f.TxMbitS)
	}
	for name, e := range m.api {
		batch.Queue(`INSERT INTO activity.api_minutes (minute_utc, endpoint, requests, client_errors, server_errors,
			rate_limited, avg_ms, max_ms) VALUES ($1, $2, $3, $4, $5, $6, $7, $8)`,
			minute, name, e.Requests, e.ClientErrors, e.ServerErrors, e.RateLimited, e.AvgMs, e.MaxMs)
	}
	if err := tx.SendBatch(ctx, batch).Close(); err != nil {
		return err
	}
	return tx.Commit(ctx)
}

func (s *store) pruneCapacity(ctx context.Context, at time.Time) (int64, error) {
	cutoff := at.UTC().Add(-90 * 24 * time.Hour).Truncate(time.Minute)
	tag, err := s.pool.Exec(ctx, `WITH expired AS (
		SELECT minute_utc FROM activity.capacity_runs
		WHERE minute_utc < $1 ORDER BY minute_utc LIMIT 500
	)
	DELETE FROM activity.capacity_runs WHERE minute_utc IN
		(SELECT minute_utc FROM expired)`, cutoff)
	return tag.RowsAffected(), err
}

// runCapacitySampler writes each minute shortly after it ends. A minute the
// process was down for, or saw only part of, stays missing rather than low.
func (s *store) runCapacitySampler(ctx context.Context, log *slog.Logger, perf *perfBoard, obs *observations) {
	started := time.Now().UTC()
	for {
		wait := time.Until(time.Now().UTC().Truncate(time.Minute).Add(time.Minute + 2*time.Second))
		timer := time.NewTimer(wait)
		select {
		case <-ctx.Done():
			timer.Stop()
			return
		case <-timer.C:
		}
		ended := time.Now().UTC().Truncate(time.Minute).Add(-time.Minute)
		if ended.Before(started) {
			continue
		}
		m := capacityMinute{at: ended, api: obs.minuteEndpoints(ended.Unix() / 60)}
		m.fleet, m.machines = perf.minuteHistory(ended.Unix() / 60)
		sampleCtx, cancel := context.WithTimeout(ctx, 2*time.Second)
		err := s.sampleCapacity(sampleCtx, m)
		cancel()
		if err != nil && ctx.Err() == nil {
			log.Warn("capacity sample failed", "err", err)
		}
	}
}
