package admin

import (
	"context"
	"sort"
	"time"
)

// A machine's plan (metaserver room_machines): it runs rooms
// RoomIDBase + 1 .. + Rooms. Live and Missing are counted against it.
type roomPlanView struct {
	Host       string    `json:"host"`
	Rooms      int       `json:"rooms"`
	RoomIDBase int64     `json:"room_id_base"`
	ReportedAt time.Time `json:"reported_at"`
	Live       int       `json:"live"`
	Missing    int       `json:"missing"`
}

// A room of no plan that is silent this long is taken for switched off: the
// registry keeps such rows a day (metaserver plans.go) and machines that
// predate plans report none.
const unplannedSilence = time.Hour

// The rows added for planned rooms the registry never heard; past it the
// plans' Missing still counts them.
const neverReportedMax = 500

func (s *adminStore) roomPlans(ctx context.Context) ([]roomPlanView, error) {
	rows, err := s.pool.Query(ctx, `SELECT host, rooms, room_id_base, reported_at FROM ops.v_room_machines ORDER BY host LIMIT 1024`)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	plans := []roomPlanView{}
	for rows.Next() {
		var p roomPlanView
		if err := rows.Scan(&p.Host, &p.Rooms, &p.RoomIDBase, &p.ReportedAt); err != nil {
			return nil, err
		}
		p.ReportedAt = p.ReportedAt.UTC()
		plans = append(plans, p)
	}
	return plans, rows.Err()
}

// applyPlans keeps what an operator must see: every planned room, alive or
// not (a planned room that never reported is added as such), and a room
// outside any plan only while it answers or went silent within the hour.
// The rest is counted as retired.
func applyPlans(rooms []metaRoom, plans []roomPlanView) ([]metaRoom, []roomPlanView, int) {
	byHost := map[string]*roomPlanView{}
	for i := range plans {
		byHost[plans[i].Host] = &plans[i]
	}
	kept := make([]metaRoom, 0, len(rooms))
	seen := map[int64]bool{}
	retired := 0
	for _, room := range rooms {
		plan := byHost[room.Host]
		planned := plan != nil && room.ID > plan.RoomIDBase && room.ID <= plan.RoomIDBase+int64(plan.Rooms)
		room.Planned = planned
		room.HasPlan = plan != nil
		switch {
		case planned:
			if room.Alive {
				plan.Live++
			}
		case room.Alive:
		case plan == nil && room.AgeS < unplannedSilence.Seconds():
		default:
			retired++
			continue
		}
		seen[room.ID] = true
		kept = append(kept, room)
	}
	added := 0
	for i := range plans {
		plan := &plans[i]
		for id := plan.RoomIDBase + 1; id <= plan.RoomIDBase+int64(plan.Rooms) && added < neverReportedMax; id++ {
			if !seen[id] {
				seen[id] = true
				added++
				kept = append(kept, metaRoom{ID: id, Host: plan.Host, Planned: true, HasPlan: true, NeverReported: true, AgeS: -1})
			}
		}
		plan.Missing = plan.Rooms - plan.Live
	}
	sort.Slice(kept, func(i, j int) bool { return kept[i].ID < kept[j].ID })
	return kept, plans, retired
}
