package admin

import (
	"context"
	"errors"
	"sort"
	"strconv"
	"time"
)

// capacityRange is a chartable period: its length and the bucket one point covers.
type capacityRange struct {
	name   string
	length time.Duration
	bucket time.Duration
}

// The default buckets give about 300 points, one per two pixels of a typical
// chart: denser points draw minute noise as a solid sawtooth.
var capacityRanges = map[string]capacityRange{
	"24h": {"24h", 24 * time.Hour, 5 * time.Minute},
	"7d":  {"7d", 7 * 24 * time.Hour, 30 * time.Minute},
	"30d": {"30d", 30 * 24 * time.Hour, 3 * time.Hour},
}

// capacitySteps divide a UTC day, so buckets stay on the same wall-clock
// marks whatever step the chart width picks.
var capacitySteps = map[time.Duration]bool{time.Minute: true, 2 * time.Minute: true, 5 * time.Minute: true,
	10 * time.Minute: true, 15 * time.Minute: true, 30 * time.Minute: true, time.Hour: true,
	2 * time.Hour: true, 3 * time.Hour: true, 6 * time.Hour: true}

const capacityPointsMax = 1500

// Norms of design/admin-console.md («Нормы мощности»).
const (
	cpuNormPct  = 80
	cpuAlarmPct = 90
)

// capacityPoint is one bucket of the fleet: sums over machines per minute,
// then the bucket's mean or peak of those minutes. Minutes counts sampled
// minutes; a bucket without any has nil figures, never zeros.
type capacityPoint struct {
	At       time.Time `json:"t"`
	Minutes  int       `json:"minutes"`
	Expected int       `json:"expected"`
	Status   string    `json:"status"` // ok, partial, missing
	// Seats with a socket in live rooms, and live rooms' seats in all.
	SeatedAvg *float64 `json:"seated_avg"`
	SeatedMax *int     `json:"seated_max"`
	Seats     *int     `json:"seats"`
	// CPU of machines that sent load: their mean, and the busiest one.
	CPUAvg *float64 `json:"cpu_avg"`
	CPUMax *float64 `json:"cpu_max"`
	// Every machine's interfaces together, Mbit/s averaged over the bucket.
	RxMbitS *float64 `json:"rx_mbit_s"`
	TxMbitS *float64 `json:"tx_mbit_s"`
	// The bucket's busiest minute.
	RxPeak *float64 `json:"rx_mbit_s_peak"`
	TxPeak *float64 `json:"tx_mbit_s_peak"`
	// API requests by every route, summed over the bucket.
	Requests     *int64 `json:"requests"`
	ClientErrors *int64 `json:"client_errors"`
	ServerErrors *int64 `json:"server_errors"`
	RateLimited  *int64 `json:"rate_limited"`
	// The rooms' ticks, fleet only: CPU per player-second weighted by players
	// (idle rooms do not inflate it), the bucket's worst p99 of each stage,
	// and ticks started late or skipped, summed.
	CPUMsPerPlayerS *float64 `json:"cpu_ms_per_player_s"`
	StepP99Us       *int64   `json:"step_p99_us"`
	ReplicateP99Us  *int64   `json:"replicate_p99_us"`
	LateP99Us       *int64   `json:"late_p99_us"`
	LateTicks       *int64   `json:"late_ticks"`
	DroppedTicks    *int64   `json:"dropped_ticks"`
}

// machineCapacity is one machine over the range and in its newest minute.
type machineCapacity struct {
	Host      string    `json:"host"`
	LastAt    time.Time `json:"last_at"`
	Rooms     int       `json:"rooms"`
	LiveRooms int       `json:"live_rooms"`
	Seated    int       `json:"seated"`
	Seats     int       `json:"seats"`
	CPU       *float64  `json:"cpu_pct"`
	RxMbitS   *float64  `json:"rx_mbit_s"`
	TxMbitS   *float64  `json:"tx_mbit_s"`
	State     string    `json:"state"` // ok, warn, alarm, unknown: CPU of the newest minute against the norms
	// Over the range.
	Minutes   int      `json:"minutes"`
	SeatedMax int      `json:"seated_max"`
	CPUAvg    *float64 `json:"cpu_avg"`
	CPUP95    *float64 `json:"cpu_p95"`
	CPUMax    *float64 `json:"cpu_max"`
	TxAvg     *float64 `json:"tx_mbit_s_avg"`
}

type endpointTotals struct {
	Endpoint     string  `json:"endpoint"`
	Requests     int64   `json:"requests"`
	ClientErrors int64   `json:"client_errors"`
	ServerErrors int64   `json:"server_errors"`
	RateLimited  int64   `json:"rate_limited"`
	AvgMs        float64 `json:"avg_ms"`
	MaxMs        float64 `json:"max_ms"`
}

const capacityMachinesMax = 500

// capacityBudget covers the whole request: a 30-day range over many machines reads millions of minute rows.
const capacityBudget = 6 * time.Second

func cpuState(cpu *float64) string {
	switch {
	case cpu == nil:
		return "unknown"
	case *cpu > cpuAlarmPct:
		return "alarm"
	case *cpu > cpuNormPct:
		return "warn"
	}
	return "ok"
}

type capacityWindow struct {
	start, end time.Time // whole buckets
	last       time.Time // the newest minute the points expect
}

// windowFor ends after the bucket holding the last whole minute, so the newest
// point may still be filling. A sampler a moment behind (it writes ~2 s after
// the minute) moves the expected end back instead of showing a gap.
func windowFor(r capacityRange, now time.Time, sampled *time.Time) capacityWindow {
	last := now.UTC().Truncate(time.Minute).Add(-time.Minute)
	if sampled != nil && sampled.Before(last) && last.Sub(*sampled) <= 2*time.Minute {
		last = sampled.UTC()
	}
	end := last.Truncate(r.bucket).Add(r.bucket)
	return capacityWindow{start: end.Add(-r.length), end: end, last: last}
}

// capacitySeries is the fleet, or one machine with host set: a minute the
// machine is absent from is missing for it, while a sampled minute with no
// machine at all is an idle fleet (no seats, no traffic, CPU unknown).
func (s *adminStore) capacitySeries(ctx context.Context, r capacityRange, w capacityWindow, host string) ([]capacityPoint, error) {
	start, end, last := w.start, w.end, w.last
	rows, err := s.pool.Query(ctx, `WITH minute AS (
		SELECT run.minute_utc,
			coalesce(sum(m.seated), 0)::integer AS seated, coalesce(sum(m.capacity), 0)::integer AS seats,
			avg(m.cpu_pct) AS cpu_avg, max(m.cpu_pct) AS cpu_max,
			CASE WHEN count(m.host) = 0 THEN 0 ELSE sum(m.rx_mbit_s) END AS rx,
			CASE WHEN count(m.host) = 0 THEN 0 ELSE sum(m.tx_mbit_s) END AS tx
		FROM activity.v_capacity_runs AS run
		LEFT JOIN activity.v_machine_minutes AS m ON m.minute_utc = run.minute_utc AND ($4 = '' OR m.host = $4)
		WHERE run.minute_utc >= $1 AND run.minute_utc < $2
		GROUP BY run.minute_utc
		HAVING $4 = '' OR count(m.host) > 0
	), api AS (
		SELECT minute_utc, sum(requests)::bigint AS requests, sum(client_errors)::bigint AS client_errors,
			sum(server_errors)::bigint AS server_errors, sum(rate_limited)::bigint AS rate_limited
		FROM activity.v_api_minutes WHERE $4 = '' AND minute_utc >= $1 AND minute_utc < $2
		GROUP BY minute_utc
	), tick AS (
		SELECT minute_utc, avg_seated, cpu_ms_per_player_s, step_p99_us, replicate_p99_us, late_p99_us, late_ticks, dropped_ticks
		FROM activity.v_fleet_minutes WHERE $4 = '' AND minute_utc >= $1 AND minute_utc < $2
	), filled AS (
		SELECT date_bin($3::interval, m.minute_utc, $1::timestamptz) AS t, count(*)::integer AS minutes,
			avg(m.seated)::float8 AS seated_avg, max(m.seated) AS seated_max, max(m.seats) AS seats,
			avg(m.cpu_avg)::float8 AS cpu_avg, max(m.cpu_max)::float8 AS cpu_max, avg(m.rx)::float8 AS rx, avg(m.tx)::float8 AS tx,
			max(m.rx)::float8 AS rx_peak, max(m.tx)::float8 AS tx_peak,
			CASE WHEN $4 = '' THEN coalesce(sum(a.requests), 0) END AS requests,
			CASE WHEN $4 = '' THEN coalesce(sum(a.client_errors), 0) END AS client_errors,
			CASE WHEN $4 = '' THEN coalesce(sum(a.server_errors), 0) END AS server_errors,
			CASE WHEN $4 = '' THEN coalesce(sum(a.rate_limited), 0) END AS rate_limited,
			(sum(k.cpu_ms_per_player_s * k.avg_seated) / NULLIF(sum(k.avg_seated) FILTER (WHERE k.cpu_ms_per_player_s IS NOT NULL), 0))::float8 AS cpu_ms,
			max(k.step_p99_us) AS step_p99, max(k.replicate_p99_us) AS replicate_p99, max(k.late_p99_us) AS late_p99,
			sum(k.late_ticks)::bigint AS late_ticks, sum(k.dropped_ticks)::bigint AS dropped_ticks
		FROM minute AS m LEFT JOIN api AS a ON a.minute_utc = m.minute_utc
		LEFT JOIN tick AS k ON k.minute_utc = m.minute_utc
		GROUP BY 1
	)
	SELECT b.t, coalesce(f.minutes, 0), f.seated_avg, f.seated_max, f.seats, f.cpu_avg, f.cpu_max, f.rx, f.tx, f.rx_peak, f.tx_peak,
		f.requests, f.client_errors, f.server_errors, f.rate_limited,
		f.cpu_ms, f.step_p99, f.replicate_p99, f.late_p99, f.late_ticks, f.dropped_ticks
	FROM generate_series($1::timestamptz, $2::timestamptz - $3::interval, $3::interval) AS b(t)
	LEFT JOIN filled AS f ON f.t = b.t
	ORDER BY b.t`, start, end, r.bucket, host)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	points := []capacityPoint{}
	perBucket := int(r.bucket / time.Minute)
	for rows.Next() {
		var p capacityPoint
		if err := rows.Scan(&p.At, &p.Minutes, &p.SeatedAvg, &p.SeatedMax, &p.Seats, &p.CPUAvg, &p.CPUMax,
			&p.RxMbitS, &p.TxMbitS, &p.RxPeak, &p.TxPeak, &p.Requests, &p.ClientErrors, &p.ServerErrors, &p.RateLimited,
			&p.CPUMsPerPlayerS, &p.StepP99Us, &p.ReplicateP99Us, &p.LateP99Us, &p.LateTicks, &p.DroppedTicks); err != nil {
			return nil, err
		}
		p.At = p.At.UTC()
		p.Expected = perBucket
		if elapsed := int(last.Sub(p.At)/time.Minute) + 1; elapsed < p.Expected {
			p.Expected = max(elapsed, 0)
		}
		switch {
		case p.Minutes == 0:
			p.Status = "missing"
		case p.Minutes < p.Expected:
			p.Status = "partial"
		default:
			p.Status = "ok"
		}
		points = append(points, p)
	}
	return points, rows.Err()
}

// capacityMachines lists every machine heard in the range: alarms first, then
// warnings, then by the newest minute's CPU; a machine silent for 5 minutes is unknown.
func (s *adminStore) capacityMachines(ctx context.Context, w capacityWindow, now time.Time) ([]machineCapacity, error) {
	start, end := w.start, w.end
	rows, err := s.pool.Query(ctx, `WITH span AS (
		SELECT host, count(*)::integer AS minutes, max(seated) AS seated_max,
			avg(cpu_pct)::float8 AS cpu_avg,
			(percentile_cont(0.95) WITHIN GROUP (ORDER BY cpu_pct))::float8 AS cpu_p95,
			max(cpu_pct)::float8 AS cpu_max, avg(tx_mbit_s)::float8 AS tx_avg
		FROM activity.v_machine_minutes WHERE minute_utc >= $1 AND minute_utc < $2
		GROUP BY host
	), last_seen AS (
		SELECT host, max(minute_utc) AS minute_utc FROM activity.v_machine_minutes
		WHERE minute_utc >= $1 AND minute_utc < $2 GROUP BY host
	), newest AS (
		SELECT DISTINCT ON (host) host, rooms, live_rooms, seated, capacity, cpu_pct, rx_mbit_s, tx_mbit_s
		FROM activity.v_machine_minutes
		WHERE minute_utc >= greatest($1, $4::timestamptz - interval '10 minutes') AND minute_utc < $2
		ORDER BY host, minute_utc DESC
	)
	SELECT s.host, l.minute_utc, coalesce(n.rooms, 0), coalesce(n.live_rooms, 0), coalesce(n.seated, 0),
		coalesce(n.capacity, 0), n.cpu_pct, n.rx_mbit_s, n.tx_mbit_s,
		s.minutes, s.seated_max, s.cpu_avg, s.cpu_p95, s.cpu_max, s.tx_avg
	FROM span AS s JOIN last_seen AS l USING (host) LEFT JOIN newest AS n USING (host)
	ORDER BY s.host
	LIMIT $3`, start, end, capacityMachinesMax, now)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	machines := []machineCapacity{}
	for rows.Next() {
		var m machineCapacity
		if err := rows.Scan(&m.Host, &m.LastAt, &m.Rooms, &m.LiveRooms, &m.Seated, &m.Seats, &m.CPU, &m.RxMbitS, &m.TxMbitS,
			&m.Minutes, &m.SeatedMax, &m.CPUAvg, &m.CPUP95, &m.CPUMax, &m.TxAvg); err != nil {
			return nil, err
		}
		m.LastAt = m.LastAt.UTC()
		m.State = cpuState(m.CPU)
		if now.Sub(m.LastAt) > 5*time.Minute {
			m.State, m.CPU, m.RxMbitS, m.TxMbitS = "unknown", nil, nil, nil
		}
		machines = append(machines, m)
	}
	if err := rows.Err(); err != nil {
		return nil, err
	}
	severity := map[string]int{"alarm": 0, "warn": 1, "ok": 2, "unknown": 3}
	cpu := func(m machineCapacity) float64 {
		if m.CPU == nil {
			return -1
		}
		return *m.CPU
	}
	sort.SliceStable(machines, func(i, j int) bool {
		if a, b := severity[machines[i].State], severity[machines[j].State]; a != b {
			return a < b
		}
		return cpu(machines[i]) > cpu(machines[j])
	})
	return machines, nil
}

func (s *adminStore) capacityEndpoints(ctx context.Context, w capacityWindow) ([]endpointTotals, error) {
	start, end := w.start, w.end
	rows, err := s.pool.Query(ctx, `SELECT endpoint, sum(requests)::bigint, sum(client_errors)::bigint,
			sum(server_errors)::bigint, sum(rate_limited)::bigint,
			(sum(avg_ms * requests) / NULLIF(sum(requests), 0))::float8, max(max_ms)::float8
		FROM activity.v_api_minutes WHERE minute_utc >= $1 AND minute_utc < $2
		GROUP BY endpoint ORDER BY sum(server_errors) DESC, sum(requests) DESC`, start, end)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	out := []endpointTotals{}
	for rows.Next() {
		var e endpointTotals
		var avg *float64
		if err := rows.Scan(&e.Endpoint, &e.Requests, &e.ClientErrors, &e.ServerErrors, &e.RateLimited, &avg, &e.MaxMs); err != nil {
			return nil, err
		}
		if avg != nil {
			e.AvgMs = *avg
		}
		out = append(out, e)
	}
	return out, rows.Err()
}

// capacityBounds are the first sampled minute (history before it does not
// exist) and the newest one.
func (s *adminStore) capacityBounds(ctx context.Context) (first, newest *time.Time, err error) {
	err = s.pool.QueryRow(ctx, `SELECT min(minute_utc), max(minute_utc) FROM activity.v_capacity_runs`).Scan(&first, &newest)
	return first, newest, err
}

var (
	errUnknownRange = errors.New("range must be 24h, 7d or 30d")
	errUnknownStep  = errors.New("step must be 60, 120, 300, 600, 900, 1800, 3600, 7200, 10800 or 21600 seconds, at most 1500 points")
)

// parseCapacityRange takes the period and, optionally, the seconds one point
// covers, which the chart picks from its width.
func parseCapacityRange(value, step string) (capacityRange, error) {
	if value == "" {
		value = "24h"
	}
	r, ok := capacityRanges[value]
	if !ok {
		return capacityRange{}, errUnknownRange
	}
	if step == "" {
		return r, nil
	}
	seconds, err := strconv.Atoi(step)
	bucket := time.Duration(seconds) * time.Second
	if err != nil || !capacitySteps[bucket] || r.length/bucket > capacityPointsMax {
		return capacityRange{}, errUnknownStep
	}
	r.bucket = bucket
	return r, nil
}
