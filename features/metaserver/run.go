// Package metaserver is the shared online back end of a room-based game:
// guest identity, accounts, the room registry, entry tickets and profile
// storage. A game's own main calls Main with its routes; the migrate job
// applies the schema; the rooms verify the tickets with the key both read
// from the same file.
package metaserver

import (
	"context"
	"errors"
	"fmt"
	"log/slog"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"
)

// The login roles the migrate job provisions. The owner is whoever the
// migrate job's URL names: the game picks its database and owner.
const (
	RoleRuntime      = "meta_runtime"
	RoleAdminService = "meta_admin_service"
	RoleBackup       = "meta_backup"
)

// PathEnter is the player route the heartbeat's probe enters through, as a
// player does; the game mounts Enter on it.
const PathEnter = "/api/enter"

// Main runs the metaserver as the process: `migrate` applies the schema
// and provisions the login roles, no argument serves. `mount` registers
// the game's player and room routes; the probes and the internal admin
// listener are mounted here.
func Main(mount func(s *Server, mux *http.ServeMux)) {
	var err error
	switch {
	case len(os.Args) == 2 && os.Args[1] == "migrate":
		err = runMigration()
	case len(os.Args) == 1:
		err = run(mount)
	default:
		err = errors.New("usage: metaserver [migrate]")
	}
	if err != nil {
		fmt.Fprintln(os.Stderr, "metaserver:", err)
		os.Exit(1)
	}
}

func runMigration() error {
	ctx, cancel := context.WithTimeout(context.Background(), time.Minute)
	defer cancel()
	st, err := openStoreWithPassword(ctx, envOr("META_DATABASE_URL", defaultDatabaseURL), os.Getenv("META_DATABASE_PASSWORD_FILE"), "")
	if err != nil {
		return err
	}
	defer st.close()
	if err := st.checkOwner(ctx); err != nil {
		return err
	}
	var logins []roleLogin
	for _, item := range []struct{ role, variable string }{
		{RoleAdminService, "META_ADMIN_DB_PASSWORD_FILE"},
		{RoleRuntime, "META_RUNTIME_DB_PASSWORD_FILE"},
		{RoleBackup, "META_BACKUP_DB_PASSWORD_FILE"},
	} {
		if path := os.Getenv(item.variable); path != "" {
			password, err := readDBPassword(path)
			if err != nil {
				return fmt.Errorf("%s: %w", item.variable, err)
			}
			logins = append(logins, roleLogin{item.role, password})
		}
	}
	return st.migrate(ctx, logins...)
}

// checkOwner refuses a migration run by anyone but the database's owner:
// the schema's objects must belong to the one role the game named.
func (s *store) checkOwner(ctx context.Context) error {
	var owner bool
	if err := s.pool.QueryRow(ctx, `SELECT datdba = (SELECT oid FROM pg_roles WHERE rolname = current_user)
		FROM pg_database WHERE datname = current_database()`).Scan(&owner); err != nil {
		return err
	}
	if !owner {
		return errors.New("migrate must run as the database owner")
	}
	return nil
}

func run(mount func(*Server, *http.ServeMux)) error {
	observed := newObservations()
	log := slog.New(&observationHandler{next: slog.NewJSONHandler(os.Stdout, nil), observations: observed})
	cfg, err := loadConfig()
	if err != nil {
		return err
	}
	key, err := loadTicketKey(cfg.TicketKeyFile)
	if err != nil {
		return err
	}
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()

	role := ""
	if cfg.DatabasePasswordFile != "" {
		role = RoleRuntime
	}
	st, err := openStoreWithPassword(ctx, cfg.DatabaseURL, cfg.DatabasePasswordFile, role)
	if err != nil {
		return err
	}
	defer st.close()
	if err := st.checkSchema(ctx); err != nil {
		return fmt.Errorf("schema preflight: %w", err)
	}
	st.comfort = cfg.RoomComfortPercent
	a := newAPI(cfg, key, st, log)
	a.observations = observed
	archive := newObservationArchive(os.Getenv("META_OBSERVABILITY_FILE"), observed, st.health, a.rooms.traffic)
	if err := archive.load(); err != nil {
		log.Warn("history checkpoint could not be restored", "err", err)
	}
	stopArchive := archive.start(log)
	defer stopArchive()
	go a.tally.run(ctx, log, st.pool, 10*time.Second)
	go st.runOccupancySampler(ctx, log)
	go runRetention(ctx, log, "occupancy", st.pruneOccupancy)
	go st.runCapacitySampler(ctx, log, a.rooms.perf, observed)
	go runRetention(ctx, log, "capacity", st.pruneCapacity)
	go runRetention(ctx, log, "room registry", st.pruneRegistry)
	certs := newCertWatch(cfg, st, log)
	if certs != nil {
		go certs.run(ctx)
	}
	if cfg.HeartbeatURL != "" {
		walker, err := newProbe(ctx, cfg, st, key, a)
		if err != nil {
			return err
		}
		go newHeartbeat(cfg, st, certs, walker, a.rooms.restarts, a.rooms, log).run(ctx)
	}
	servers := []*http.Server{newHTTPServer(cfg.Addr, a.handler(mount))}
	if cfg.AdminAddr != "" {
		servers = append(servers, newHTTPServer(cfg.AdminAddr, newAdmin(a, certs).handler()))
	}
	errs := make(chan error, len(servers))
	for _, server := range servers {
		go func() { errs <- server.ListenAndServe() }()
		log.Info("listening", "addr", server.Addr)
	}

	select {
	case err := <-errs:
		return err
	case <-ctx.Done():
	}
	// In-flight requests get a moment; the OS gets the ports back either way.
	shutdownCtx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	for _, server := range servers {
		if err := server.Shutdown(shutdownCtx); err != nil && !errors.Is(err, context.DeadlineExceeded) {
			return err
		}
	}
	log.Info("stopped")
	return nil
}

func newHTTPServer(addr string, handler http.Handler) *http.Server {
	return &http.Server{
		Addr:              addr,
		Handler:           handler,
		ReadHeaderTimeout: 5 * time.Second,
		ReadTimeout:       15 * time.Second,
		WriteTimeout:      15 * time.Second,
		IdleTimeout:       2 * time.Minute,
		// The largest header a client sends is a bearer; the default (1 MB)
		// would let a flood of them buy memory before any limiter runs.
		MaxHeaderBytes: 16 << 10,
	}
}
