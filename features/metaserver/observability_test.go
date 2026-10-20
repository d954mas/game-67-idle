package metaserver

import (
	"context"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync"
	"testing"
	"time"
)

func TestObservationsWindowAndStatus(t *testing.T) {
	o := newObservations()
	now := time.Unix(1800000000, 0)
	o.record(now.Add(-24*time.Hour), "enter", 500, time.Second)
	o.record(now, "enter", 200, 2*time.Millisecond)
	o.record(now, "enter", 429, 4*time.Millisecond)
	o.record(now, "enter", 503, 6*time.Millisecond)
	s := o.snapshot(now)
	e := s.Endpoints["enter"]
	if e.Requests != 3 || e.ClientErrors != 1 || e.ServerErrors != 1 || e.RateLimited != 1 || e.AvgMs != 4 || e.MaxMs != 6 {
		t.Fatalf("wrong window/status accounting: %+v", e)
	}
	if len(o.snapshot(now.Add(23*time.Hour)).Endpoints) == 0 {
		t.Fatal("day history expired early")
	}
	if len(o.snapshot(now.Add(24*time.Hour)).Endpoints) != 0 {
		t.Fatal("expired data remains")
	}
}

func TestObservationsHTTPAndPrivacy(t *testing.T) {
	o := newObservations()
	h := o.handler(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(503)
		w.WriteHeader(201)
		w.Write([]byte("secret body"))
	}))
	r := httptest.NewRequest("POST", "/rooms/profiles/123/save?token=secret", nil)
	w := httptest.NewRecorder()
	h.ServeHTTP(w, r)
	s := o.snapshot(time.Now())
	if w.Code != 503 || s.Endpoints["other"].ServerErrors != 1 {
		t.Fatalf("response lost: %+v", s)
	}
	if len(s.Events) != 1 || strings.Contains(s.Events[0].Message, "secret") || strings.Contains(s.Events[0].Message, "123") {
		t.Fatalf("private request data: %+v", s.Events)
	}
}

func TestObservationsLogAndConcurrency(t *testing.T) {
	o := newObservations()
	log := slog.New(&observationHandler{next: slog.NewJSONHandler(io.Discard, nil), observations: o})
	log.With("password", "secret").WithGroup("db").Warn("database unavailable", "err", "secret connection string")
	if got := o.snapshot(time.Now()).Events; len(got) != 1 || got[0].Message != "database unavailable" {
		t.Fatalf("log not recorded: %+v", got)
	}
	var wg sync.WaitGroup
	for i := 0; i < 8; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for j := 0; j < 100; j++ {
				o.record(time.Now(), "enter", 503, time.Millisecond)
				o.snapshot(time.Now())
				log.InfoContext(context.Background(), "ignored")
			}
		}()
	}
	wg.Wait()
	s := o.snapshot(time.Now())
	if s.Endpoints["enter"].Requests != 800 || len(s.Events) > observationEventLimit {
		t.Fatalf("unbounded or lost: %+v", s)
	}
}

func TestObservationsImplicitResponseAndPanic(t *testing.T) {
	for _, tc := range []struct {
		name    string
		handler http.HandlerFunc
		status  int64
	}{
		{"implicit", func(w http.ResponseWriter, r *http.Request) { w.Write([]byte("ok")) }, 0},
		{"empty", func(w http.ResponseWriter, r *http.Request) {}, 0},
		{"panic", func(w http.ResponseWriter, r *http.Request) { panic("private failure") }, 1},
	} {
		t.Run(tc.name, func(t *testing.T) {
			o := newObservations()
			func() {
				defer func() {
					recovered := recover()
					if tc.name == "panic" && recovered != "private failure" {
						t.Fatal("panic was swallowed")
					}
				}()
				o.handler(tc.handler).ServeHTTP(httptest.NewRecorder(), httptest.NewRequest("GET", "/unknown/private", nil))
			}()
			e := o.snapshot(time.Now()).Endpoints["other"]
			if e.Requests != 1 || e.ServerErrors != tc.status {
				t.Fatalf("wrong observation: %+v", e)
			}
		})
	}
}
