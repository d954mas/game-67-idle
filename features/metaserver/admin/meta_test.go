package admin

import (
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
)

func TestMetaClientReadsBoundedSnapshotWithServiceToken(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/internal/admin/v1/snapshot" || r.Header.Get("Authorization") != "Bearer test-service-token" {
			http.Error(w, "wrong request", http.StatusUnauthorized)
			return
		}
		w.Header().Set("Content-Type", "application/json")
		w.Write([]byte(`{"now":"2026-09-29T00:00:00Z","rooms":[{"id":1,"seated":2,"away":1,"capacity":4,"alive":true}],"players":{"guests":1,"accounts":1}}`))
	}))
	defer server.Close()
	client := metaClient{baseURL: server.URL, token: "test-service-token", http: server.Client()}
	snapshot, err := client.snapshot(t.Context())
	if err != nil || len(snapshot.Rooms) != 1 || snapshot.Rooms[0].Seated != 2 || snapshot.Players.Accounts != 1 {
		t.Fatalf("snapshot: %+v %v", snapshot, err)
	}
}

func TestMetaClientRejectsOversizedBody(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, _ *http.Request) {
		w.Write([]byte(strings.Repeat("x", 2<<20)))
	}))
	defer server.Close()
	client := metaClient{baseURL: server.URL, token: "test", http: server.Client()}
	if _, err := client.snapshot(t.Context()); err == nil {
		t.Fatal("oversized snapshot accepted")
	}
}
