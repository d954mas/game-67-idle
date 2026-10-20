package metaserver

import (
	"context"
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/tls"
	"crypto/x509"
	"crypto/x509/pkix"
	"io"
	"log/slog"
	"math/big"
	"net"
	"net/http"
	"strings"
	"testing"
	"time"
)

// roomTLS listens as a room would, presenting a certificate for room.test
// that ends `left` from now, signed by a throwaway CA the watch may be
// told to trust.
func roomTLS(t *testing.T, left time.Duration) (addr string, roots *x509.CertPool) {
	t.Helper()
	caKey, _ := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	caTemplate := &x509.Certificate{SerialNumber: big.NewInt(1), Subject: pkix.Name{CommonName: "test CA"},
		NotBefore: time.Now().Add(-time.Hour), NotAfter: time.Now().Add(365 * 24 * time.Hour),
		IsCA: true, KeyUsage: x509.KeyUsageCertSign, BasicConstraintsValid: true}
	caDER, err := x509.CreateCertificate(rand.Reader, caTemplate, caTemplate, &caKey.PublicKey, caKey)
	if err != nil {
		t.Fatal(err)
	}
	ca, _ := x509.ParseCertificate(caDER)
	leafKey, _ := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	leafTemplate := &x509.Certificate{SerialNumber: big.NewInt(2), Subject: pkix.Name{CommonName: "room.test"},
		DNSNames: []string{"room.test"}, NotBefore: time.Now().Add(-time.Hour), NotAfter: time.Now().Add(left),
		KeyUsage: x509.KeyUsageDigitalSignature, ExtKeyUsage: []x509.ExtKeyUsage{x509.ExtKeyUsageServerAuth}}
	leafDER, err := x509.CreateCertificate(rand.Reader, leafTemplate, ca, &leafKey.PublicKey, caKey)
	if err != nil {
		t.Fatal(err)
	}
	cert := tls.Certificate{Certificate: [][]byte{leafDER, caDER}, PrivateKey: leafKey}
	ln, err := tls.Listen("tcp", "127.0.0.1:0", &tls.Config{Certificates: []tls.Certificate{cert}})
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { ln.Close() })
	go func() {
		for {
			conn, err := ln.Accept()
			if err != nil {
				return
			}
			go func() {
				conn.(*tls.Conn).Handshake()
				conn.Close()
			}()
		}
	}()
	roots = x509.NewCertPool()
	roots.AddCert(ca)
	return ln.Addr().String(), roots
}

// The watch exists only where the rooms present their own certificate;
// a short one or an untrusted one fails the heartbeat with the reason, a
// host that does not answer does not, and the admin console shows them all.
func TestRoomCertificatesFeedTheHeartbeat(t *testing.T) {
	st, _ := freshStore(t)
	a := newTestAPI(t, st) // room 9 reports from room.test
	log := slog.New(slog.NewTextHandler(io.Discard, nil))
	cfg := Config{RoomURL: "wss://{host}:{port}/", HeartbeatURL: "https://watcher.test/x", HeartbeatEvery: 20 * time.Second}
	if newCertWatch(Config{RoomURL: "wss://stand.test/rooms/{id}"}, st, log) != nil {
		t.Fatal("a watch behind a proxy")
	}
	w := newCertWatch(cfg, st, log)
	if w == nil {
		t.Fatal("no watch for rooms presenting their own certificate")
	}
	ctx := context.Background()
	var addr string
	w.addr = func(string, uint16) string { return addr }

	addr, w.roots = roomTLS(t, 5*24*time.Hour)
	w.check(ctx)
	if reason, bad := w.alarm(time.Now()); !bad || reason != "room certificate room.test expires in 4 days" {
		t.Fatalf("five days left: %v %q", bad, reason)
	}
	h := newHeartbeat(cfg, st, w, nil, nil, nil, log)
	if ok, report := h.check(ctx); ok || !strings.HasPrefix(report, "room certificate room.test expires") {
		t.Fatalf("heartbeat: %v %q", ok, report)
	}

	addr, w.roots = roomTLS(t, 90*24*time.Hour)
	w.check(ctx)
	if reason, bad := w.alarm(time.Now()); bad {
		t.Fatalf("ninety days left: %q", reason)
	}
	if ok, report := h.check(ctx); !ok || !strings.HasPrefix(report, "rooms alive=1") {
		t.Fatalf("heartbeat: %v %q", ok, report)
	}
	if snap := w.snapshot(); len(snap) != 1 || snap[0].Host != "room.test" || snap[0].Invalid != "" ||
		snap[0].Unreachable != "" || time.Until(snap[0].Expires) < 89*24*time.Hour {
		t.Fatalf("snapshot: %+v", snap)
	}

	closed, _ := net.Listen("tcp", "127.0.0.1:0")
	addr = closed.Addr().String()
	closed.Close()
	w.check(ctx)
	if snap := w.snapshot(); len(snap) != 1 || snap[0].Unreachable == "" || !snap[0].Expires.IsZero() {
		t.Fatalf("closed port: %+v", snap)
	}
	if reason, bad := w.alarm(time.Now()); bad {
		t.Fatalf("closed port alarmed: %q", reason)
	}

	addr, _ = roomTLS(t, 90*24*time.Hour)
	w.roots = x509.NewCertPool()
	w.check(ctx)
	if reason, bad := w.alarm(time.Now()); !bad || !strings.HasPrefix(reason, "room certificate room.test: ") {
		t.Fatalf("untrusted: %v %q", bad, reason)
	}
	if snap := w.snapshot(); len(snap) != 1 || snap[0].Invalid == "" || snap[0].Expires.IsZero() {
		t.Fatalf("untrusted snapshot: %+v", snap)
	}

	a.cfg.AdminNets, _ = parseNets("127.0.0.0/8")
	a.cfg.AdminServiceToken = "internal-service-token-with-enough-entropy"
	token := []string{"Authorization", "Bearer " + a.cfg.AdminServiceToken}
	path := "/internal/admin/v1/incidents"
	code, snap := adminGet(t, newAdmin(a, w).handler(), path, "127.0.0.1:5000", "127.0.0.1:8081", token...)
	certs, _ := snap["certificates"].([]any)
	if code != http.StatusOK || len(certs) != 1 || certs[0].(map[string]any)["host"] != "room.test" ||
		certs[0].(map[string]any)["invalid"] == "" {
		t.Fatalf("incidents: %d %v", code, snap["certificates"])
	}
	if _, without := adminGet(t, newAdmin(a, nil).handler(), path, "127.0.0.1:5000", "127.0.0.1:8081", token...); without["certificates"] != nil {
		t.Fatalf("behind a proxy: %v", without["certificates"])
	}
}
