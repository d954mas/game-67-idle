package metaserver

import (
	"errors"
	"fmt"
	"net/netip"
	"os"
	"strconv"
	"strings"
	"time"
)

// Config comes from META_* variables. The defaults are the local stand
// (compose.yaml's database), so `go run .` needs nothing but a key file;
// production sets the real values. Rooms are not configured: they
// report themselves, and META_ROOM_URL says how players reach one
// (`ws://127.0.0.1:{port}/` locally, `wss://host/room/{id}/` behind a
// proxy, `ws://{host}:{port}/` when rooms are dialled by the name they
// report).
type Config struct {
	Addr                 string        // listen address
	DatabaseURL          string        // pgx connection string
	DatabasePasswordFile string        // restricted runtime credential
	TicketKeyFile        string        // 64 hex digits, shared with the rooms
	RoomURL              string        // what players open: `{id}` and `{port}` are filled in
	TicketTTL            time.Duration // validity window of an entry ticket
	CallsPerHour         int           // API calls one address may make
	// Addresses the per-address call and entry budgets do not apply to: the
	// operator's load generators. Session budgets and login attempts still do.
	UnlimitedAddrs []netip.Prefix
	BehindProxy    bool // the peer is our reverse proxy; the player is the last X-Forwarded-For hop
	// The internal views the operator console reads have their own listener,
	// never the player port; empty = none. Only a socket peer inside
	// AdminNets is served, whatever any header says, only a request naming
	// this host by address, as localhost or by one of AdminHosts, and only
	// with AdminServiceToken; without a token every path is 404.
	AdminAddr         string
	AdminNets         []netip.Prefix
	AdminHosts        []string
	AdminServiceToken string
	// An outside watcher's ping address (healthchecks.io); empty = none.
	// The room machines' monthly traffic allowance in GB, for the traffic
	// alert (alerts.go); 0 = none.
	TrafficMonthGB float64
	HeartbeatURL   string
	HeartbeatEvery time.Duration
	// The origin players use, for the heartbeat's probe to enter through
	// the proxy as they do; empty = the probe asks the router directly.
	PublicURL string
	// Entry opens an empty room once every room with company has more
	// than this share of its seats taken.
	RoomComfortPercent int
	// The largest profile document a room may store, in bytes.
	ProfileMaxBytes int
}

// The bounds of META_PROFILE_MAX_BYTES: a sealed document is a few
// kilobytes, and one save is one request body held in memory.
const (
	defaultProfileMaxBytes = 64 << 10
	profileMaxBytesFloor   = 1 << 10
	profileMaxBytesCeiling = 1 << 20
)

func loadConfig() (Config, error) {
	cfg := Config{
		Addr:                 envOr("META_ADDR", "127.0.0.1:8080"),
		DatabaseURL:          envOr("META_DATABASE_URL", defaultDatabaseURL),
		DatabasePasswordFile: os.Getenv("META_DATABASE_PASSWORD_FILE"),
		TicketKeyFile:        envOr("META_TICKET_KEY_FILE", "ticket.key"),
		RoomURL:              envOr("META_ROOM_URL", "ws://127.0.0.1:{port}/"),
		BehindProxy:          envOr("META_BEHIND_PROXY", "") == "1",
		AdminAddr:            os.Getenv("META_ADMIN_ADDR"),
	}
	if _, set := os.LookupEnv("META_ADMIN_ADDR"); !set {
		cfg.AdminAddr = "127.0.0.1:8081"
	}
	var err error
	if cfg.AdminNets, err = parseNets(envOr("META_ADMIN_NETS", "127.0.0.0/8,::1/128")); err != nil {
		return cfg, fmt.Errorf("META_ADMIN_NETS: %w", err)
	}
	for _, host := range strings.Split(os.Getenv("META_ADMIN_HOSTS"), ",") {
		if host = strings.ToLower(strings.TrimSpace(host)); host != "" {
			cfg.AdminHosts = append(cfg.AdminHosts, host)
		}
	}
	if text := strings.TrimSpace(envOr("META_TRAFFIC_MONTH_GB", "0")); text != "" {
		if cfg.TrafficMonthGB, err = strconv.ParseFloat(text, 64); err != nil || cfg.TrafficMonthGB < 0 {
			return cfg, errors.New("META_TRAFFIC_MONTH_GB must be a number of GB, 0 for none")
		}
	}
	cfg.HeartbeatURL = strings.TrimRight(strings.TrimSpace(os.Getenv("META_HEARTBEAT_URL")), "/")
	if cfg.HeartbeatEvery, err = time.ParseDuration(envOr("META_HEARTBEAT_EVERY", "60s")); err != nil {
		return cfg, fmt.Errorf("META_HEARTBEAT_EVERY: %w", err)
	}
	if cfg.HeartbeatURL != "" && !strings.HasPrefix(cfg.HeartbeatURL, "https://") {
		return cfg, errors.New("META_HEARTBEAT_URL must be an https address")
	}
	if cfg.HeartbeatEvery < 10*time.Second || cfg.HeartbeatEvery > time.Hour {
		return cfg, errors.New("META_HEARTBEAT_EVERY must be between 10s and 1h")
	}
	cfg.PublicURL = strings.TrimRight(strings.TrimSpace(os.Getenv("META_PUBLIC_URL")), "/")
	if cfg.PublicURL != "" && !strings.HasPrefix(cfg.PublicURL, "https://") {
		return cfg, errors.New("META_PUBLIC_URL must be an https origin")
	}
	if file := os.Getenv("META_ADMIN_SERVICE_TOKEN_FILE"); file != "" {
		value, err := os.ReadFile(file)
		if err != nil {
			return cfg, fmt.Errorf("META_ADMIN_SERVICE_TOKEN_FILE: %w", err)
		}
		cfg.AdminServiceToken = strings.TrimSpace(string(value))
		if len(cfg.AdminServiceToken) < 32 {
			return cfg, errors.New("META_ADMIN_SERVICE_TOKEN_FILE: token is under 32 characters")
		}
	}
	if cfg.TicketTTL, err = time.ParseDuration(envOr("META_TICKET_TTL", "30s")); err != nil {
		return cfg, fmt.Errorf("META_TICKET_TTL: %w", err)
	}
	if cfg.CallsPerHour, err = strconv.Atoi(envOr("META_CALLS_PER_HOUR", "600")); err != nil {
		return cfg, fmt.Errorf("META_CALLS_PER_HOUR: %w", err)
	}
	if list := envOr("META_UNLIMITED_ADDRS", ""); strings.TrimSpace(list) != "" {
		if cfg.UnlimitedAddrs, err = parseAddrs(list); err != nil {
			return cfg, fmt.Errorf("META_UNLIMITED_ADDRS: %w", err)
		}
	}
	if cfg.RoomComfortPercent, err = strconv.Atoi(envOr("META_ROOM_COMFORT_PERCENT", strconv.Itoa(defaultComfortPercent))); err != nil ||
		cfg.RoomComfortPercent < 1 || cfg.RoomComfortPercent > 100 {
		return cfg, errors.New("META_ROOM_COMFORT_PERCENT must be 1..100")
	}
	if cfg.ProfileMaxBytes, err = strconv.Atoi(envOr("META_PROFILE_MAX_BYTES", strconv.Itoa(defaultProfileMaxBytes))); err != nil ||
		cfg.ProfileMaxBytes < profileMaxBytesFloor || cfg.ProfileMaxBytes > profileMaxBytesCeiling {
		return cfg, fmt.Errorf("META_PROFILE_MAX_BYTES must be %d..%d", profileMaxBytesFloor, profileMaxBytesCeiling)
	}
	switch {
	case !strings.Contains(cfg.RoomURL, "{id}") && !strings.Contains(cfg.RoomURL, "{port}") &&
		!strings.Contains(cfg.RoomURL, "{host}"):
		return cfg, errors.New("META_ROOM_URL must contain {id}, {port} or {host}")
	case cfg.TicketTTL < 5*time.Second || cfg.TicketTTL > 2*time.Minute:
		// Short enough that a leaked ticket is worthless soon, long enough
		// for a slow first connect.
		return cfg, errors.New("META_TICKET_TTL must be between 5s and 2m")
	case cfg.CallsPerHour <= 0:
		return cfg, errors.New("META_CALLS_PER_HOUR must be positive")
	}
	return cfg, nil
}

// parseAddrs reads addresses and networks: a bare address is its own /32 or /128.
func parseAddrs(list string) ([]netip.Prefix, error) {
	var nets []netip.Prefix
	for _, item := range strings.Split(list, ",") {
		item = strings.TrimSpace(item)
		if item == "" {
			continue
		}
		if addr, err := netip.ParseAddr(item); err == nil {
			nets = append(nets, netip.PrefixFrom(addr.Unmap(), addr.Unmap().BitLen()))
			continue
		}
		prefix, err := netip.ParsePrefix(item)
		if err != nil {
			return nil, err
		}
		nets = append(nets, prefix.Masked())
	}
	return nets, nil
}

func parseNets(list string) ([]netip.Prefix, error) {
	var nets []netip.Prefix
	for _, item := range strings.Split(list, ",") {
		item = strings.TrimSpace(item)
		if item == "" {
			continue
		}
		prefix, err := netip.ParsePrefix(item)
		if err != nil {
			return nil, err
		}
		nets = append(nets, prefix.Masked())
	}
	if len(nets) == 0 {
		return nil, errors.New("no networks")
	}
	return nets, nil
}

func envOr(name, fallback string) string {
	if value := os.Getenv(name); value != "" {
		return value
	}
	return fallback
}

// defaultDatabaseURL is compose.yaml's local stand.
const defaultDatabaseURL = "postgres://metaserver:metaserver@127.0.0.1:5432/metaserver"
