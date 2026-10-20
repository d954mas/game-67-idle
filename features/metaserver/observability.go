package metaserver

import (
	"context"
	"encoding/json"
	"fmt"
	"log/slog"
	"net/http"
	"sync"
	"time"
)

type endpointObservation struct {
	Requests     int64   `json:"requests"`
	ClientErrors int64   `json:"client_errors"`
	ServerErrors int64   `json:"server_errors"`
	RateLimited  int64   `json:"rate_limited"`
	AvgMs        float64 `json:"avg_ms"`
	MaxMs        float64 `json:"max_ms"`
	totalMs      float64
}

type observationMinute struct {
	minute    int64
	endpoints map[string]endpointObservation
}

type observationEvent struct {
	At      time.Time `json:"at"`
	Level   string    `json:"level"`
	Message string    `json:"message"`
	Detail  string    `json:"detail,omitempty"`
}

type observationSnapshot struct {
	Endpoints map[string]endpointObservation `json:"endpoints"`
	Events    []observationEvent             `json:"events"`
	History   observationHistory             `json:"history"`
	Groups    []observationErrorGroup        `json:"groups"`
}

const (
	observationMinutes    = 24 * 60
	observationRetention  = 24 * time.Hour
	observationEventLimit = 1000
)

type observationHistory struct {
	RetentionHours int       `json:"retention_hours"`
	Persistent     bool      `json:"persistent"`
	LastSaved      time.Time `json:"last_saved"`
	Error          string    `json:"error,omitempty"`
}

// Minute buckets and a capped journal keep diagnostics bounded under failure storms.
type observations struct {
	mu      sync.Mutex
	minutes [observationMinutes]observationMinute
	events  []observationEvent
	history observationHistory
	groups  map[string]*observationErrorGroupState
}

func newObservations() *observations {
	return &observations{history: observationHistory{RetentionHours: 24}}
}

func (o *observations) historyState() observationHistory {
	o.mu.Lock()
	defer o.mu.Unlock()
	return o.history
}

func (o *observations) record(now time.Time, name string, status int, took time.Duration, detail ...string) {
	o.mu.Lock()
	defer o.mu.Unlock()
	minute := now.Unix() / 60
	b := &o.minutes[minute%int64(len(o.minutes))]
	if b.minute != minute || b.endpoints == nil {
		*b = observationMinute{minute: minute, endpoints: map[string]endpointObservation{}}
	}
	e := b.endpoints[name]
	e.Requests++
	if status >= 400 && status < 500 {
		e.ClientErrors++
	}
	if status >= 500 {
		e.ServerErrors++
	}
	if status == 429 {
		e.RateLimited++
	}
	ms := float64(took.Microseconds()) / 1000
	e.totalMs += ms
	e.MaxMs = max(e.MaxMs, ms)
	b.endpoints[name] = e
	if status >= 500 {
		event := observationEvent{At: now, Level: "ERROR", Message: fmt.Sprintf("%s HTTP %d", name, status)}
		if len(detail) > 0 {
			event.Detail = detail[0]
		}
		o.eventLocked(event)
	}
}

func (o *observations) eventLocked(e observationEvent) {
	e.Message = observationTextLimit(observationDetail(e.Message), 256)
	e.Detail = observationDetail(e.Detail)
	o.groupEventLocked(e)
	if len(o.events) == observationEventLimit {
		copy(o.events, o.events[1:])
		o.events = o.events[:observationEventLimit-1]
	}
	o.events = append(o.events, e)
}

// minuteEndpoints is one minute's requests by route (Unix minutes); empty when
// nothing was served in it or it has left the window.
func (o *observations) minuteEndpoints(minute int64) map[string]endpointObservation {
	o.mu.Lock()
	defer o.mu.Unlock()
	out := map[string]endpointObservation{}
	b := &o.minutes[minute%int64(len(o.minutes))]
	if b.minute != minute {
		return out
	}
	for name, e := range b.endpoints {
		e.AvgMs = per(e.totalMs, float64(e.Requests))
		out[name] = e
	}
	return out
}

func (o *observations) snapshot(now time.Time) observationSnapshot {
	o.mu.Lock()
	defer o.mu.Unlock()
	s := observationSnapshot{Endpoints: map[string]endpointObservation{}, Events: []observationEvent{}, History: o.history}
	s.Groups = o.groupsSnapshotLocked(now)
	minute := now.Unix() / 60
	for _, b := range o.minutes {
		if b.minute <= minute-observationMinutes || b.minute > minute {
			continue
		}
		for name, value := range b.endpoints {
			e := s.Endpoints[name]
			e.Requests += value.Requests
			e.ClientErrors += value.ClientErrors
			e.ServerErrors += value.ServerErrors
			e.RateLimited += value.RateLimited
			e.totalMs += value.totalMs
			e.MaxMs = max(e.MaxMs, value.MaxMs)
			e.AvgMs = e.totalMs / float64(e.Requests)
			s.Endpoints[name] = e
		}
	}
	for i := len(o.events) - 1; i >= 0; i-- {
		if o.events[i].At.After(now.Add(-observationRetention)) {
			s.Events = append(s.Events, o.events[i])
		}
	}
	return s
}

type labelSlot struct{}

// labeled names the route for the observation wrapper around it; a request
// no route labels is "other". Labels never contain account ids, paths,
// query strings or credentials.
func labeled(name string, h http.HandlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		if slot, ok := r.Context().Value(labelSlot{}).(*string); ok {
			*slot = name
		}
		h(w, r)
	}
}

type observedResponse struct {
	http.ResponseWriter
	status    int
	errorBody []byte
}

func (w *observedResponse) Unwrap() http.ResponseWriter { return w.ResponseWriter }
func (w *observedResponse) WriteHeader(status int) {
	if status < 200 {
		w.ResponseWriter.WriteHeader(status)
		return
	}
	if w.status != 0 {
		return
	}
	w.status = status
	w.ResponseWriter.WriteHeader(status)
}
func (w *observedResponse) Write(body []byte) (int, error) {
	if w.status == 0 {
		w.WriteHeader(http.StatusOK)
	}
	n, err := w.ResponseWriter.Write(body)
	if w.status >= 500 && len(w.errorBody) < 4096 {
		w.errorBody = append(w.errorBody, body[:min(n, 4096-len(w.errorBody))]...)
	}
	return n, err
}

func (w *observedResponse) errorDetail(status int) string {
	if status < 500 {
		return ""
	}
	var body struct {
		Error string `json:"error"`
	}
	if json.Unmarshal(w.errorBody, &body) == nil && body.Error != "" {
		return observationDetail(body.Error)
	}
	return http.StatusText(status)
}

func (o *observations) handler(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		start := time.Now()
		rec := &observedResponse{ResponseWriter: w}
		label := "other"
		r = r.WithContext(context.WithValue(r.Context(), labelSlot{}, &label))
		defer func() {
			failure := recover()
			status := rec.status
			if status == 0 {
				status = http.StatusOK
			}
			if failure != nil {
				status = http.StatusInternalServerError
			}
			o.record(time.Now(), label, status, time.Since(start), rec.errorDetail(status))
			if failure != nil {
				panic(failure)
			}
		}()
		next.ServeHTTP(rec, r)
	})
}

// Only named diagnostic attributes enter the journal; request context stays in Docker logs.
type observationHandler struct {
	next            slog.Handler
	observations    *observations
	detail          string
	suppressDetails bool
}

func (h *observationHandler) Enabled(ctx context.Context, level slog.Level) bool {
	return h.next.Enabled(ctx, level)
}
func (h *observationHandler) Handle(ctx context.Context, r slog.Record) error {
	if r.Level >= slog.LevelWarn {
		detail := h.detail
		if !h.suppressDetails {
			r.Attrs(func(attr slog.Attr) bool {
				detail = observationJoinDetail(detail, observationAttrDetail(attr, 0))
				return true
			})
		}
		h.observations.mu.Lock()
		h.observations.eventLocked(observationEvent{At: r.Time, Level: r.Level.String(), Message: r.Message, Detail: detail})
		h.observations.mu.Unlock()
	}
	return h.next.Handle(ctx, r)
}
func (h *observationHandler) WithAttrs(attrs []slog.Attr) slog.Handler {
	detail := h.detail
	if !h.suppressDetails {
		for _, attr := range attrs {
			detail = observationJoinDetail(detail, observationAttrDetail(attr, 0))
		}
	}
	return &observationHandler{next: h.next.WithAttrs(attrs), observations: h.observations, detail: detail, suppressDetails: h.suppressDetails}
}
func (h *observationHandler) WithGroup(name string) slog.Handler {
	return &observationHandler{next: h.next.WithGroup(name), observations: h.observations, detail: h.detail, suppressDetails: h.suppressDetails || observationPrivateGroup(name)}
}
