package metaserver

import (
	"log/slog"
	"regexp"
	"sort"
	"strings"
	"time"
)

const observationGroupLimit = 128

type observationErrorGroup struct {
	Level   string    `json:"level"`
	Message string    `json:"message"`
	Detail  string    `json:"detail,omitempty"`
	Count   int64     `json:"count"`
	FirstAt time.Time `json:"first_at"`
	LastAt  time.Time `json:"last_at"`
}

type observationErrorGroupState struct {
	Level       string                    `json:"level"`
	Message     string                    `json:"message"`
	Detail      string                    `json:"detail,omitempty"`
	LastAt      time.Time                 `json:"last_at"`
	MinuteFirst [observationMinutes]int64 `json:"minute_first"`
	Counts      [observationMinutes]int64 `json:"counts"`
}

func observationErrorKey(level, message string) string { return level + "\x00" + message }

func (o *observations) groupEventLocked(e observationEvent) {
	if o.groups == nil {
		o.groups = map[string]*observationErrorGroupState{}
	}
	key := observationErrorKey(e.Level, e.Message)
	group := o.groups[key]
	if group == nil {
		for k, previous := range o.groups {
			if !previous.LastAt.After(e.At.Add(-observationRetention)) {
				delete(o.groups, k)
			}
		}
		level, message := e.Level, e.Message
		if len(o.groups) >= observationGroupLimit-1 {
			level, message = "OTHER", "Additional error types"
			key = observationErrorKey(level, message)
		}
		group = o.groups[key]
		if group == nil {
			group = &observationErrorGroupState{Level: level, Message: message}
			o.groups[key] = group
		}
	}
	minute := e.At.Unix() / 60
	index := minute % observationMinutes
	if index < 0 {
		return
	}
	if group.MinuteFirst[index]/60 != minute || group.Counts[index] == 0 {
		group.MinuteFirst[index], group.Counts[index] = e.At.Unix(), 0
	}
	group.MinuteFirst[index] = min(group.MinuteFirst[index], e.At.Unix())
	group.Counts[index]++
	if !e.At.Before(group.LastAt) {
		group.LastAt, group.Detail = e.At, e.Detail
	}
}

func (o *observations) groupsSnapshotLocked(now time.Time) []observationErrorGroup {
	out := []observationErrorGroup{}
	minute := now.Unix() / 60
	for key, state := range o.groups {
		if !state.LastAt.After(now.Add(-observationRetention)) {
			delete(o.groups, key)
			continue
		}
		group := observationErrorGroup{Level: state.Level, Message: state.Message, Detail: state.Detail, LastAt: state.LastAt}
		for i, first := range state.MinuteFirst {
			if state.Counts[i] == 0 || first/60 <= minute-observationMinutes || first/60 > minute {
				continue
			}
			group.Count += state.Counts[i]
			at := time.Unix(first, 0)
			if group.FirstAt.IsZero() || at.Before(group.FirstAt) {
				group.FirstAt = at
			}
		}
		if group.Count > 0 {
			out = append(out, group)
		}
	}
	sort.Slice(out, func(i, j int) bool {
		if out[i].LastAt.Equal(out[j].LastAt) {
			return observationErrorKey(out[i].Level, out[i].Message) < observationErrorKey(out[j].Level, out[j].Message)
		}
		return out[i].LastAt.After(out[j].LastAt)
	})
	return out
}

var observationURLs = regexp.MustCompile(`[a-zA-Z][a-zA-Z0-9+.-]*://[^\s"'<>]+`)
var observationBearer = regexp.MustCompile(`(?i)\bbearer\s+[a-z0-9._~+/=-]+`)
var observationSecrets = regexp.MustCompile(`(?i)\b(password|passwd|pwd|token|access_token|refresh_token|session|session_id|session_secret)\b["']?\s*[:=]\s*("(?:\\.|[^"\\])*(?:"|\\?$)|'(?:\\.|[^'\\])*(?:'|\\?$)|[^\s,;]+)`)

func observationTextLimit(text string, limit int) string {
	for offset := range text {
		if limit == 0 {
			return text[:offset]
		}
		limit--
	}
	return text
}

func observationDetail(text string) string {
	text = observationTextLimit(text, 16384)
	text = observationURLs.ReplaceAllString(text, "[redacted URL]")
	text = observationBearer.ReplaceAllString(text, "Bearer [redacted]")
	text = observationSecrets.ReplaceAllString(text, "$1=[redacted]")
	return observationTextLimit(text, 2048)
}

func observationPrivateGroup(name string) bool {
	switch strings.ToLower(name) {
	case "request", "password", "token", "session":
		return true
	}
	return false
}

func observationAttrDetail(attr slog.Attr, depth int) string {
	if depth > 16 {
		return ""
	}
	value := attr.Value.Resolve()
	if value.Kind() == slog.KindGroup {
		if observationPrivateGroup(attr.Key) {
			return ""
		}
		detail := ""
		for _, child := range value.Group() {
			detail = observationJoinDetail(detail, observationAttrDetail(child, depth+1))
		}
		return detail
	}
	switch attr.Key {
	case "err", "error", "reason", "last_reason":
		return observationDetail(attr.Key + ": " + value.String())
	}
	return ""
}

func observationJoinDetail(first, next string) string {
	if first == "" {
		return observationDetail(next)
	}
	if next == "" {
		return first
	}
	return observationTextLimit(first+"; "+next, 2048)
}
