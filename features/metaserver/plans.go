package metaserver

import (
	"context"
	"sync"
	"time"
)

// Every room of a machine reports the machine's plan with each window; one
// write a minute per machine, or at once when the plan changes, keeps it fresh
// without a database write per status.
const planRefresh = time.Minute

type planCache struct {
	mu    sync.Mutex
	hosts map[string]planSeen
}

type planSeen struct {
	plan perfPlan
	at   time.Time
}

// due notes the plan and says whether the store should hear it now.
func (c *planCache) due(host string, plan perfPlan, now time.Time) bool {
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.hosts == nil {
		c.hosts = map[string]planSeen{}
	}
	seen, ok := c.hosts[host]
	if ok && seen.plan == plan && now.Sub(seen.at) < planRefresh {
		return false
	}
	if len(c.hosts) >= perfHostsMax && !ok {
		return false
	}
	c.hosts[host] = planSeen{plan: plan, at: now}
	return true
}

// forget makes the next report write again after a failed one.
func (c *planCache) forget(host string) {
	c.mu.Lock()
	defer c.mu.Unlock()
	delete(c.hosts, host)
}

func (s *store) reportPlan(ctx context.Context, host string, plan perfPlan) error {
	_, err := s.pool.Exec(ctx, `INSERT INTO room_machines (host, rooms, room_id_base, reported_at)
		VALUES ($1, $2, $3, now())
		ON CONFLICT (host) DO UPDATE SET rooms = EXCLUDED.rooms, room_id_base = EXCLUDED.room_id_base,
			reported_at = EXCLUDED.reported_at`, host, int32(plan.Rooms), int64(plan.RoomIDBase))
	return err
}

// The registry keeps a row per room id it ever heard. A row silent for a day
// and outside its machine's plan (or of a machine with no plan heard for a
// week) is a room switched off; a planned room keeps its row, so its silence
// stays an alarm while the machine's plan stands. A pruned row takes its
// incarnation fence with it: a life of that id seen again starts a new one.
const (
	registryKeep = 24 * time.Hour
	planKeep     = 7 * 24 * time.Hour
)

func (s *store) pruneRegistry(ctx context.Context, at time.Time) (int64, error) {
	tag, err := s.pool.Exec(ctx, `DELETE FROM room_machines WHERE reported_at < $1`, at.Add(-planKeep))
	if err != nil {
		return 0, err
	}
	tag, err = s.pool.Exec(ctx, `WITH stale AS (
		SELECT r.id FROM rooms AS r LEFT JOIN room_machines AS m ON m.host = r.host
		WHERE r.seen_at < $1 AND (m.host IS NULL OR r.id <= m.room_id_base OR r.id > m.room_id_base + m.rooms)
		ORDER BY r.id LIMIT 500
	)
	DELETE FROM rooms WHERE id IN (SELECT id FROM stale) AND seen_at < $1`, at.Add(-registryKeep))
	if err != nil {
		return 0, err
	}
	// Rooms only: runRetention repeats at once while a full batch comes back.
	return tag.RowsAffected(), nil
}
