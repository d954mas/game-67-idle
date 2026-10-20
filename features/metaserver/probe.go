package metaserver

import (
	"context"
	"encoding/base64"
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"

	"github.com/coder/websocket"
)

// The room's wire, as far as the probe speaks it: network-core's HELLO.
const (
	helloMagic   = 0x5357544E // "NTWS"
	msgHello     = 0
	closeVersion = 4002
	probeLogin   = "#probe" // no player can register it: logins are [a-z0-9_]
	probeTimeout = 5 * time.Second
)

// probe walks the player's path once per beat: entry through the proxy
// (when the public origin is known, else the router directly), the room's
// wss port with its certificate and a HELLO. The registry's counters
// cannot see a room whose certificate a browser refuses or a room that
// does not answer; this can.
type probe struct {
	cfg    Config
	store  *store
	key    ticketKey
	api    *Server
	client *http.Client
	player int64
	secret string // the probe's own session, minted at start
}

type entryAnswer struct {
	URL    string `json:"url"`
	Ticket string `json:"ticket"`
}

func newProbe(ctx context.Context, cfg Config, st *store, key ticketKey, a *Server) (*probe, error) {
	p := &probe{cfg: cfg, store: st, key: key, api: a, client: &http.Client{Timeout: probeTimeout}}
	var err error
	if p.player, err = st.ensureProbePlayer(ctx, probeLogin); err != nil {
		return nil, fmt.Errorf("probe player: %w", err)
	}
	if cfg.PublicURL == "" {
		return p, nil
	}
	if p.secret, err = newSecret(); err != nil {
		return nil, err
	}
	hash, _ := secretHash(p.secret)
	if err := st.replaceSessions(ctx, p.player, hash); err != nil {
		return nil, fmt.Errorf("probe session: %w", err)
	}
	return p, nil
}

// run is one walk; the report is for the heartbeat's line.
func (p *probe) run(ctx context.Context) (string, error) {
	ctx, cancel := context.WithTimeout(ctx, probeTimeout)
	defer cancel()
	start := time.Now()
	url, ticket, err := p.enter(ctx)
	if err != nil {
		return "", err
	}
	entered := time.Since(start)
	if err := walk(ctx, url, ticket); err != nil {
		return "", fmt.Errorf("room %s: %w", url, err)
	}
	return fmt.Sprintf("probe entry_ms=%d room_ms=%d", entered.Milliseconds(),
		(time.Since(start) - entered).Milliseconds()), nil
}

// enter is what the client does: through the proxy with the probe's
// session when the public origin is configured, else the router itself.
func (p *probe) enter(ctx context.Context) (string, []byte, error) {
	if p.cfg.PublicURL == "" {
		room, err := p.store.pick(ctx, p.player)
		if err != nil {
			return "", nil, fmt.Errorf("pick: %w", err)
		}
		expiresAt := uint32(time.Now().Add(p.cfg.TicketTTL).Unix())
		ticket := encodeTicket(p.key, ticketKindPlayer, uint64(p.player), room.id, expiresAt)
		return p.api.roomURL(room), ticket[:], nil
	}
	req, err := http.NewRequestWithContext(ctx, http.MethodPost, p.cfg.PublicURL+PathEnter, strings.NewReader("{}"))
	if err != nil {
		return "", nil, err
	}
	req.Header.Set("Authorization", "Bearer "+p.secret)
	req.Header.Set("Content-Type", "application/json")
	res, err := p.client.Do(req)
	if err != nil {
		return "", nil, fmt.Errorf("enter: %w", err)
	}
	defer res.Body.Close()
	body, _ := io.ReadAll(io.LimitReader(res.Body, 4096))
	if res.StatusCode != http.StatusOK {
		return "", nil, fmt.Errorf("enter: status %d %s", res.StatusCode, strings.TrimSpace(string(body)))
	}
	var answer entryAnswer
	if err := json.Unmarshal(body, &answer); err != nil {
		return "", nil, fmt.Errorf("enter: %w", err)
	}
	ticket, err := base64.RawURLEncoding.DecodeString(answer.Ticket)
	if err != nil || answer.URL == "" {
		return "", nil, errors.New("enter: answer without a room or a ticket")
	}
	return answer.URL, ticket, nil
}

// walk is the room's part, as far as a probe that does not speak the
// game's wire can go: the wss port with its certificate, and a HELLO
// with this ticket and a protocol version no build has, which the room's
// socket layer refuses with 4002 before it reads the ticket. That proves
// the room listens, speaks TLS the players accept and answers HELLO; the
// seat, the profile and the wire stay the game client's own tests.
func walk(ctx context.Context, url string, ticket []byte) error {
	conn, _, err := websocket.Dial(ctx, url, nil)
	if err != nil {
		return fmt.Errorf("dial: %w", err)
	}
	defer conn.CloseNow()
	conn.SetReadLimit(1 << 16)
	hello := make([]byte, 9, 9+len(ticket))
	hello[0] = msgHello
	binary.LittleEndian.PutUint32(hello[1:], helloMagic)
	binary.LittleEndian.PutUint32(hello[5:], 0)
	if err := conn.Write(ctx, websocket.MessageBinary, append(hello, ticket...)); err != nil {
		return fmt.Errorf("hello: %w", err)
	}
	for {
		_, _, err := conn.Read(ctx)
		if err == nil {
			continue
		}
		if websocket.CloseStatus(err) == closeVersion {
			return nil
		}
		if errors.Is(err, context.DeadlineExceeded) {
			return errors.New("HELLO: no answer in time")
		}
		return fmt.Errorf("HELLO: %w, not the version refusal", err)
	}
}

// ---- the store's part ----

// ensureProbePlayer is the probe's own account: a login no player can
// take, no password, no way in but the session this service mints.
func (s *store) ensureProbePlayer(ctx context.Context, login string) (int64, error) {
	var id int64
	err := s.pool.QueryRow(ctx,
		`INSERT INTO players (login) VALUES ($1)
		 ON CONFLICT (login) DO UPDATE SET login = EXCLUDED.login
		 RETURNING id`, login).Scan(&id)
	return id, err
}

// replaceSessions leaves the player exactly one session: this one.
func (s *store) replaceSessions(ctx context.Context, player int64, hash []byte) error {
	_, err := s.pool.Exec(ctx,
		`WITH gone AS (DELETE FROM sessions WHERE player_id = $2)
		 INSERT INTO sessions (secret_hash, player_id) VALUES ($1, $2)`, hash, player)
	return err
}
