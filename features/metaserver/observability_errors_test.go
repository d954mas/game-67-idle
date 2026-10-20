package metaserver

import (
	"fmt"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"
)

func TestObservationGroupsOutliveEventCap(t *testing.T) {
	o := newObservations()
	now := time.Now()
	for i := 0; i < observationEventLimit+20; i++ {
		o.eventLocked(observationEvent{At: now, Level: "ERROR", Message: "save profile", Detail: fmt.Sprintf("failure %d", i)})
	}
	s := o.snapshot(now)
	if len(s.Events) != observationEventLimit || len(s.Groups) != 1 || s.Groups[0].Count != observationEventLimit+20 {
		t.Fatalf("group count lost: events=%d groups=%+v", len(s.Events), s.Groups)
	}
	if s.Groups[0].Detail != fmt.Sprintf("failure %d", observationEventLimit+19) {
		t.Fatal("latest detail was not retained")
	}
	if len(o.snapshot(now.Add(observationRetention)).Groups) != 0 {
		t.Fatal("expired group remains")
	}
}

func TestObservationGroupBoundsAndExpiry(t *testing.T) {
	o := newObservations()
	now := time.Now()
	for i := 0; i < 160; i++ {
		o.eventLocked(observationEvent{At: now, Level: "ERROR", Message: fmt.Sprintf("type %d", i)})
	}
	s := o.snapshot(now)
	var count int64
	for _, group := range s.Groups {
		count += group.Count
	}
	if len(s.Groups) > 128 || count != 160 {
		t.Fatalf("unbounded or lost groups: groups=%d count=%d", len(s.Groups), count)
	}
	later := now.Add(25 * time.Hour)
	o.eventLocked(observationEvent{At: later, Level: "ERROR", Message: "fresh type"})
	if s := o.snapshot(later); len(s.Groups) != 1 || s.Groups[0].Message != "fresh type" {
		t.Fatalf("expired groups consumed type budget: %+v", s.Groups)
	}
}

func TestObservationErrorDetailsAreSelectedAndRedacted(t *testing.T) {
	o := newObservations()
	log := slog.New(&observationHandler{next: slog.NewJSONHandler(io.Discard, nil), observations: o})
	log.With("password", "must-not-appear", "err", "dial postgres://user:secret@host/db").WithGroup("db").
		Warn("request failed", "error", `Bearer verysecret password="two words" token=abc session: xyz https://private/path?q=secret`,
			"reason", "connection refused", "request", "private-body")
	s := o.snapshot(time.Now())
	if len(s.Events) != 1 || len(s.Groups) != 1 {
		t.Fatal("missing grouped event")
	}
	detail := s.Events[0].Detail
	for _, secret := range []string{"must-not-appear", "verysecret", "two words", "abc", "xyz", "postgres://", "https://", "private-body"} {
		if strings.Contains(detail, secret) {
			t.Fatalf("secret %q leaked: %s", secret, detail)
		}
	}
	if !strings.Contains(detail, "connection refused") || s.Groups[0].Detail != detail {
		t.Fatalf("safe diagnostic detail lost: %q", detail)
	}
}

func TestObservationDetailLimitAndNestedAttrs(t *testing.T) {
	o := newObservations()
	log := slog.New(&observationHandler{next: slog.NewJSONHandler(io.Discard, nil), observations: o})
	log.With(slog.Group("db", slog.String("last_reason", "timeout"))).WithGroup("request").
		Warn("request failed", "err", "private request context")
	log.Warn("large error", "err", strings.Repeat("я", 3000))
	s := o.snapshot(time.Now())
	if s.Events[1].Detail != "last_reason: timeout" {
		t.Fatalf("group attr privacy lost: %q", s.Events[1].Detail)
	}
	if len([]rune(s.Events[0].Detail)) > 2048 {
		t.Fatal("unbounded detail")
	}
}

func TestObservationDetailQuotedAssignments(t *testing.T) {
	for _, value := range []string{
		`password="first \"secret\" last"`,
		`token='first \'secret\' last'`,
		`"session": "first secret last"`,
		`password="first secret unfinished`,
	} {
		got := observationDetail(value)
		for _, word := range []string{"first", "secret", "last", "unfinished"} {
			if strings.Contains(got, word) {
				t.Fatalf("assignment leaked %s: %s", word, got)
			}
		}
	}
}

func TestObservationHTTPErrorDetail(t *testing.T) {
	for _, tc := range []struct {
		status     int
		body, want string
	}{
		{503, `{"error":"no room has a free seat"}`, "no room has a free seat"},
		{500, `{"error":"failed token=private"}`, "failed token=[redacted]"},
		{500, `private raw body`, "Internal Server Error"},
		{500, `{"error":"` + strings.Repeat("private", 1000) + `"}`, "Internal Server Error"},
		{200, `{"error":"private successful body"}`, ""},
	} {
		o := newObservations()
		var response *observedResponse
		h := o.handler(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			response = w.(*observedResponse)
			w.WriteHeader(tc.status)
			w.Write([]byte(tc.body))
		}))
		h.ServeHTTP(httptest.NewRecorder(), httptest.NewRequest("POST", "/api/enter", nil))
		if len(response.errorBody) > 4096 || (tc.status == 200 && len(response.errorBody) != 0) {
			t.Fatal("response capture exceeded its scope")
		}
		s := o.snapshot(time.Now())
		if tc.status == 200 {
			if len(s.Events) != 0 {
				t.Fatal("successful response captured")
			}
		} else if len(s.Events) != 1 || s.Events[0].Detail != tc.want {
			t.Fatalf("wrong HTTP failure detail: %+v", s.Events)
		}
	}
}
