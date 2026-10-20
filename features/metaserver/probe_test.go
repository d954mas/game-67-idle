package metaserver

import (
	"bytes"
	"context"
	"encoding/binary"
	"net/http"
	"net/http/httptest"
	"strconv"
	"strings"
	"testing"
	"time"

	"github.com/coder/websocket"
)

// A room made of one WebSocket handler: it takes the HELLO and then does
// what the test says a room would.
func fakeRoom(t *testing.T, behave func(ctx context.Context, conn *websocket.Conn, hello []byte)) (*httptest.Server, string) {
	t.Helper()
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		conn, err := websocket.Accept(w, r, nil)
		if err != nil {
			t.Error(err)
			return
		}
		defer conn.CloseNow()
		_, hello, err := conn.Read(r.Context())
		if err != nil {
			return
		}
		behave(r.Context(), conn, hello)
	}))
	return server, "ws" + strings.TrimPrefix(server.URL, "http")
}

func TestProbeGivesUpOnASilentRoom(t *testing.T) {
	room, url := fakeRoom(t, func(ctx context.Context, conn *websocket.Conn, hello []byte) {
		<-ctx.Done()
	})
	defer room.Close()
	ctx, cancel := context.WithTimeout(context.Background(), 300*time.Millisecond)
	defer cancel()
	err := walk(ctx, url, []byte("t"))
	if err == nil || !strings.Contains(err.Error(), "no answer in time") {
		t.Fatalf("a silent room reads %v", err)
	}
}

func portOf(t *testing.T, url string) uint16 {
	t.Helper()
	port, err := strconv.Atoi(url[strings.LastIndex(url, ":")+1:])
	if err != nil {
		t.Fatal(err)
	}
	return uint16(port)
}

// A room is walked as far as its socket layer: a HELLO with this ticket and
// a version no build has, refused with 4002.
func TestProbeWalksARoomToItsVersionRefusal(t *testing.T) {
	ticket := []byte("a ticket, opaque to the probe")
	var seen []byte
	room, url := fakeRoom(t, func(ctx context.Context, conn *websocket.Conn, hello []byte) {
		seen = append([]byte(nil), hello...)
		_ = conn.Close(websocket.StatusCode(closeVersion), "version")
	})
	defer room.Close()
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	if err := walk(ctx, url, ticket); err != nil {
		t.Fatal(err)
	}
	if seen[0] != msgHello || binary.LittleEndian.Uint32(seen[1:]) != helloMagic || binary.LittleEndian.Uint32(seen[5:]) != 0 ||
		!bytes.Equal(seen[9:], ticket) {
		t.Fatalf("HELLO %x", seen)
	}
	other, otherURL := fakeRoom(t, func(ctx context.Context, conn *websocket.Conn, hello []byte) {
		_ = conn.Close(websocket.StatusCode(4100), "reenter")
	})
	defer other.Close()
	if err := walk(ctx, otherURL, ticket); err == nil || !strings.Contains(err.Error(), "not the version refusal") {
		t.Fatalf("a close other than 4002 reads %v", err)
	}
}

// The probe enters as a player would and walks the room it was sent to.
func TestProbeWalksTheRoomItEnters(t *testing.T) {
	st, _ := freshStore(t)
	a := newTestAPI(t, st)
	ctx := context.Background()
	room, url := fakeRoom(t, func(ctx context.Context, conn *websocket.Conn, hello []byte) {
		if binary.LittleEndian.Uint32(hello[5:]) == 0 && len(hello) == 9+ticketSize {
			_ = conn.Close(websocket.StatusCode(closeVersion), "version")
		}
	})
	defer room.Close()
	// The fixture's room 9, in a new life, listens where the fake one does.
	if _, err := st.reportRoom(ctx, 9, 2, &roomStatus{V: statusVersion, Port: portOf(t, url), Free: 4, Capacity: 4,
		PeriodMs: 1000, Host: "room.test"}); err != nil {
		t.Fatal(err)
	}
	walker, err := newProbe(ctx, a.cfg, st, testKey, a)
	if err != nil {
		t.Fatal(err)
	}
	if report, err := walker.run(ctx); err != nil || !strings.HasPrefix(report, "probe entry_ms=") {
		t.Fatalf("walk: %q %v", report, err)
	}
}
