package metaserver

import (
	"encoding/json"
	"errors"
	"fmt"
	"log/slog"
	"os"
	"path/filepath"
	"time"
)

type savedObservationEndpoint struct {
	Metrics endpointObservation `json:"metrics"`
	TotalMs float64             `json:"total_ms"`
}

type savedObservationMinute struct {
	Minute    int64                               `json:"minute"`
	Endpoints map[string]savedObservationEndpoint `json:"endpoints,omitempty"`
}

type observationArchiveState struct {
	Version  int                                        `json:"version"`
	SavedAt  time.Time                                  `json:"saved_at"`
	Minutes  [observationMinutes]savedObservationMinute `json:"minutes"`
	Events   []observationEvent                         `json:"events"`
	Groups   map[string]observationErrorGroupState      `json:"groups"`
	Database dbHealthState                              `json:"database"`
	// Room machines' traffic by day (traffic.go); absent in older checkpoints.
	Traffic map[string]trafficHostState `json:"traffic,omitempty"`
}

type observationArchive struct {
	path         string
	observations *observations
	database     *dbHealth
	traffic      *trafficLedger // nil: none kept
}

func newObservationArchive(path string, observed *observations, database *dbHealth, traffic *trafficLedger) *observationArchive {
	observed.mu.Lock()
	observed.history.Persistent = path != ""
	observed.mu.Unlock()
	return &observationArchive{path, observed, database, traffic}
}

func (a *observationArchive) result(at time.Time, err error) error {
	a.observations.mu.Lock()
	defer a.observations.mu.Unlock()
	if err != nil {
		a.observations.history.Error = "History checkpoint unavailable; see metaserver logs"
	} else {
		a.observations.history.LastSaved = at
		a.observations.history.Error = ""
	}
	return err
}

func (a *observationArchive) load() error {
	if a.path == "" {
		return nil
	}
	file, err := os.Open(a.path)
	if errors.Is(err, os.ErrNotExist) {
		return nil
	}
	if err != nil {
		return a.result(time.Time{}, err)
	}
	defer file.Close()
	info, err := file.Stat()
	if err != nil {
		return a.result(time.Time{}, err)
	}
	if info.Size() > 16<<20 {
		return a.result(time.Time{}, errors.New("history checkpoint exceeds limit"))
	}
	var state observationArchiveState
	if err := json.NewDecoder(file).Decode(&state); err != nil {
		return a.result(time.Time{}, err)
	}
	if state.Version != 1 || len(state.Events) > observationEventLimit || len(state.Groups) > observationGroupLimit {
		return a.result(time.Time{}, errors.New("unsupported history checkpoint"))
	}
	for _, minute := range state.Minutes {
		if len(minute.Endpoints) > 32 {
			return a.result(time.Time{}, errors.New("history endpoint limit exceeded"))
		}
	}
	o := a.observations
	o.mu.Lock()
	for i, minute := range state.Minutes {
		o.minutes[i] = observationMinute{minute: minute.Minute, endpoints: map[string]endpointObservation{}}
		for name, endpoint := range minute.Endpoints {
			e := endpoint.Metrics
			e.totalMs = endpoint.TotalMs
			o.minutes[i].endpoints[name] = e
		}
	}
	o.events = state.Events
	o.groups = make(map[string]*observationErrorGroupState, len(state.Groups))
	for key, group := range state.Groups {
		o.groups[key] = &group
	}
	o.mu.Unlock()
	a.database.restore(state.Database)
	a.traffic.restore(state.Traffic)
	return a.result(state.SavedAt, nil)
}

func (a *observationArchive) save() error {
	if a.path == "" {
		return nil
	}
	state := observationArchiveState{Version: 1, SavedAt: time.Now(), Database: a.database.checkpoint(), Groups: map[string]observationErrorGroupState{},
		Traffic: a.traffic.checkpoint()}
	o := a.observations
	o.mu.Lock()
	for i, minute := range o.minutes {
		if minute.minute <= state.SavedAt.Unix()/60-observationMinutes || minute.minute > state.SavedAt.Unix()/60 {
			continue
		}
		state.Minutes[i] = savedObservationMinute{Minute: minute.minute, Endpoints: map[string]savedObservationEndpoint{}}
		for name, endpoint := range minute.endpoints {
			state.Minutes[i].Endpoints[name] = savedObservationEndpoint{endpoint, endpoint.totalMs}
		}
	}
	for _, event := range o.events {
		if event.At.After(state.SavedAt.Add(-observationRetention)) && !event.At.After(state.SavedAt) {
			state.Events = append(state.Events, event)
		}
	}
	for key, group := range o.groups {
		if !group.LastAt.After(state.SavedAt.Add(-observationRetention)) || group.LastAt.After(state.SavedAt) {
			continue
		}
		copy := *group
		for i, first := range copy.MinuteFirst {
			if first/60 <= state.SavedAt.Unix()/60-observationMinutes || first/60 > state.SavedAt.Unix()/60 {
				copy.MinuteFirst[i], copy.Counts[i] = 0, 0
			}
		}
		state.Groups[key] = copy
	}
	o.mu.Unlock()
	data, err := json.Marshal(state)
	if err != nil {
		return a.result(time.Time{}, err)
	}
	if err := os.MkdirAll(filepath.Dir(a.path), 0700); err != nil {
		return a.result(time.Time{}, err)
	}
	file, err := os.CreateTemp(filepath.Dir(a.path), ".history-*")
	if err != nil {
		return a.result(time.Time{}, err)
	}
	defer os.Remove(file.Name())
	if _, err = file.Write(data); err == nil {
		err = file.Sync()
	}
	closeErr := file.Close()
	if err == nil {
		err = closeErr
	}
	if err == nil {
		err = os.Rename(file.Name(), a.path)
	}
	return a.result(state.SavedAt, err)
}

// One writer owns the archive; the final save follows HTTP shutdown.
func (a *observationArchive) start(log *slog.Logger) func() {
	if a.path == "" {
		return func() {}
	}
	stop, done := make(chan struct{}), make(chan struct{})
	save := func() {
		if err := a.save(); err != nil {
			log.Warn("history checkpoint failed", "err", fmt.Sprint(err))
		}
	}
	go func() {
		defer close(done)
		ticker := time.NewTicker(30 * time.Second)
		defer ticker.Stop()
		for {
			select {
			case <-ticker.C:
				save()
			case <-stop:
				return
			}
		}
	}()
	return func() { close(stop); <-done; save() }
}
