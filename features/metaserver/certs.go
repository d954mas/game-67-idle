package metaserver

import (
	"context"
	"crypto/tls"
	"crypto/x509"
	"fmt"
	"log/slog"
	"net"
	"strconv"
	"strings"
	"sync"
	"time"
)

// certWatch reads, once an hour, the certificate each room host presents
// to players, on a stand where the rooms present their own (a `{host}`
// room URL). The renewer on a room machine can stop quietly (a revoked
// DNS token, a wedged container) and nothing else here notices until the
// day the wildcard runs out and no player can connect: the rooms keep
// reporting, the heartbeat keeps saying all is well. So the heartbeat
// carries this finding while there is time to act, and the admin console
// shows the dates. Behind a proxy the certificate is the proxy's own
// business, and there is no watch.
type certWatch struct {
	store *store
	log   *slog.Logger
	port  uint16         // dialed instead of the room's when the room URL names none
	roots *x509.CertPool // nil: the system's
	addr  func(host string, port uint16) string
	mu    sync.Mutex
	hosts []certState
}

type certState struct {
	Host    string    `json:"host"`
	Checked time.Time `json:"checked"`
	Expires time.Time `json:"expires"`
	// The rooms answered, but no player would accept what they showed.
	Invalid string `json:"invalid,omitempty"`
	// Nothing answered; from where players sit the rooms may be fine.
	Unreachable string `json:"unreachable,omitempty"`
}

const (
	certFirstCheck = time.Minute
	certEvery      = time.Hour
	certWarn       = 14 * 24 * time.Hour
	certDialTime   = 10 * time.Second
)

func newCertWatch(cfg Config, st *store, log *slog.Logger) *certWatch {
	if !strings.HasPrefix(cfg.RoomURL, "wss://") || !strings.Contains(cfg.RoomURL, "{host}") {
		return nil
	}
	w := &certWatch{store: st, log: log,
		addr: func(host string, port uint16) string { return net.JoinHostPort(host, strconv.Itoa(int(port))) }}
	if !strings.Contains(cfg.RoomURL, "{port}") {
		w.port = 443
	}
	return w
}

// run checks a minute after start, when the rooms have reported, then hourly.
func (w *certWatch) run(ctx context.Context) {
	timer := time.NewTimer(certFirstCheck)
	defer timer.Stop()
	for {
		select {
		case <-ctx.Done():
			return
		case <-timer.C:
			w.check(ctx)
			timer.Reset(certEvery)
		}
	}
}

// check dials one room of every host alive rooms report and keeps what
// it found; hosts no room reports any more are forgotten with it.
func (w *certWatch) check(ctx context.Context) {
	listCtx, cancel := context.WithTimeout(adminQueries(ctx), storeTimeout)
	hosts, err := w.store.roomHosts(listCtx)
	cancel()
	if err != nil {
		w.log.Error("room certificates: registry", "err", err)
		return
	}
	states := make([]certState, 0, len(hosts))
	for _, room := range hosts {
		port := room.port
		if w.port != 0 {
			port = w.port
		}
		state := certState{Host: room.host, Checked: time.Now()}
		leaf, chain, err := w.fetch(ctx, room.host, port)
		switch {
		case err != nil:
			state.Unreachable = err.Error()
			w.log.Warn("room certificate unreachable", "host", room.host, "port", port, "err", err)
		default:
			state.Expires = leaf.NotAfter
			if err := verifyChain(leaf, chain, room.host, w.roots); err != nil {
				state.Invalid = err.Error()
				w.log.Warn("room certificate invalid", "host", room.host, "err", err)
			} else {
				w.log.Info("room certificate", "host", room.host, "expires", leaf.NotAfter.Format(time.DateOnly))
			}
		}
		states = append(states, state)
	}
	w.mu.Lock()
	w.hosts = states
	w.mu.Unlock()
}

// fetch takes the chain a host presents without judging it: a player's
// verdict is verifyChain's, and a chain that fails it is still a date
// worth showing.
func (w *certWatch) fetch(ctx context.Context, host string, port uint16) (*x509.Certificate, []*x509.Certificate, error) {
	ctx, cancel := context.WithTimeout(ctx, certDialTime)
	defer cancel()
	dialer := tls.Dialer{Config: &tls.Config{ServerName: host, InsecureSkipVerify: true, MinVersion: tls.VersionTLS12}}
	conn, err := dialer.DialContext(ctx, "tcp", w.addr(host, port))
	if err != nil {
		return nil, nil, err
	}
	defer conn.Close()
	chain := conn.(*tls.Conn).ConnectionState().PeerCertificates
	if len(chain) == 0 {
		return nil, nil, fmt.Errorf("no certificate presented")
	}
	return chain[0], chain[1:], nil
}

func verifyChain(leaf *x509.Certificate, intermediates []*x509.Certificate, host string, roots *x509.CertPool) error {
	opts := x509.VerifyOptions{DNSName: host, Roots: roots, Intermediates: x509.NewCertPool()}
	for _, cert := range intermediates {
		opts.Intermediates.AddCert(cert)
	}
	_, err := leaf.Verify(opts)
	return err
}

// alarm is the heartbeat's reason when a room certificate needs the
// operator: an invalid one keeps players out now, one under two weeks
// from its end is a renewal that has stopped. An unreachable host is not
// an alarm: the rooms answer for their own liveness.
func (w *certWatch) alarm(now time.Time) (string, bool) {
	if w == nil {
		return "", false
	}
	w.mu.Lock()
	defer w.mu.Unlock()
	for _, state := range w.hosts {
		switch {
		case state.Invalid != "":
			return "room certificate " + state.Host + ": " + state.Invalid, true
		case state.Unreachable != "":
		case state.Expires.Sub(now) < certWarn:
			return fmt.Sprintf("room certificate %s expires in %d days", state.Host, int(state.Expires.Sub(now).Hours()/24)), true
		}
	}
	return "", false
}

// snapshot is nil without a watch, empty before the first check.
func (w *certWatch) snapshot() []certState {
	if w == nil {
		return nil
	}
	w.mu.Lock()
	defer w.mu.Unlock()
	out := make([]certState, len(w.hosts))
	copy(out, w.hosts)
	return out
}
