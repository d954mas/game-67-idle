// Package admin is the operator console: its own login (password),
// read-only views of the metaserver and its database, and the owner's
// bootstrap and recovery ceremonies.
package admin

import (
	"context"
	"errors"
	"fmt"
	"net/http"
	"net/url"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"

	"github.com/jackc/pgx/v5/pgxpool"
)

// Brand names the game on the console page.
type Brand struct {
	Name string // "Example Game"
	Mark string // two letters for the badge
}

// slug names the console's session cookie and CSRF label: cookies are scoped
// by host, not port, so two games' consoles on one hostname must not share them.
func (b Brand) slug() string {
	var out strings.Builder
	dash := false
	for _, r := range strings.ToLower(b.Name) {
		if (r >= 'a' && r <= 'z') || (r >= '0' && r <= '9') {
			if dash && out.Len() > 0 {
				out.WriteByte('-')
			}
			out.WriteRune(r)
			dash = false
		} else {
			dash = true
		}
	}
	if out.Len() == 0 {
		return "game"
	}
	return out.String()
}

func (b Brand) sessionCookie() string { return "__Host-" + b.slug() + "-admin" }

type adminConfig struct {
	addr       string
	origin     string
	dbURL      string
	dbPassword string
	metaURL    string
	metaToken  string
}

func loadAdminConfig() (adminConfig, error) {
	var cfg adminConfig
	if os.Getenv("ADMIN_ENABLED") != "1" {
		return cfg, errors.New("admin service is disabled")
	}
	cfg.origin = strings.TrimRight(os.Getenv("ADMIN_ORIGIN"), "/")
	origin, err := url.Parse(cfg.origin)
	if err != nil || origin.Scheme != "https" || origin.Host == "" || origin.Path != "" ||
		origin.RawQuery != "" || origin.Fragment != "" || origin.User != nil {
		return cfg, errors.New("ADMIN_ORIGIN must be an https origin")
	}
	cfg.dbURL = os.Getenv("ADMIN_DATABASE_URL")
	if cfg.dbURL == "" {
		return cfg, errors.New("ADMIN_DATABASE_URL is required")
	}
	passwordFile := os.Getenv("ADMIN_DATABASE_PASSWORD_FILE")
	if passwordFile == "" {
		return cfg, errors.New("ADMIN_DATABASE_PASSWORD_FILE is required")
	}
	password, err := os.ReadFile(passwordFile)
	if err != nil {
		return cfg, err
	}
	cfg.dbPassword = strings.TrimSpace(string(password))
	if len(cfg.dbPassword) < 32 || len(cfg.dbPassword) > 256 {
		return cfg, errors.New("ADMIN_DATABASE_PASSWORD_FILE must contain a 32..256 character password")
	}
	cfg.addr = os.Getenv("ADMIN_ADDR")
	if cfg.addr == "" {
		cfg.addr = ":8082"
	}
	cfg.metaURL = os.Getenv("ADMIN_META_URL")
	if cfg.metaURL == "" {
		cfg.metaURL = "http://meta:8081"
	}
	metaURL, err := url.Parse(cfg.metaURL)
	if err != nil || (metaURL.Scheme != "http" && metaURL.Scheme != "https") || metaURL.Host == "" ||
		metaURL.User != nil || metaURL.Path != "" || metaURL.RawQuery != "" || metaURL.Fragment != "" {
		return cfg, errors.New("ADMIN_META_URL must be an http(s) origin")
	}
	tokenFile := os.Getenv("ADMIN_META_TOKEN_FILE")
	if tokenFile == "" {
		return cfg, errors.New("ADMIN_META_TOKEN_FILE is required")
	}
	token, err := os.ReadFile(tokenFile)
	if err != nil {
		return cfg, err
	}
	cfg.metaToken = strings.TrimSpace(string(token))
	if len(cfg.metaToken) < 32 {
		return cfg, errors.New("ADMIN_META_TOKEN_FILE must contain at least 32 characters")
	}
	return cfg, nil
}

func runAdmin(brand Brand) error {
	cfg, err := loadAdminConfig()
	if err != nil {
		return err
	}
	poolConfig, err := pgxpool.ParseConfig(cfg.dbURL)
	if err != nil {
		return err
	}
	if poolConfig.ConnConfig.User != "meta_admin_service" {
		return fmt.Errorf("admin database role %q is not meta_admin_service", poolConfig.ConnConfig.User)
	}
	if poolConfig.ConnConfig.Password != "" {
		return errors.New("ADMIN_DATABASE_URL must not contain a password")
	}
	poolConfig.ConnConfig.Password = cfg.dbPassword
	poolConfig.MaxConns = 4
	poolConfig.ConnConfig.RuntimeParams["statement_timeout"] = "1000"
	poolConfig.ConnConfig.RuntimeParams["lock_timeout"] = "500"
	poolConfig.ConnConfig.RuntimeParams["search_path"] = "ops,pg_catalog"
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()
	pool, err := pgxpool.NewWithConfig(ctx, poolConfig)
	if err != nil {
		return err
	}
	defer pool.Close()
	var role string
	var superuser, createRole, createDB, bypassRLS bool
	if err := pool.QueryRow(ctx, `SELECT current_user, rolsuper, rolcreaterole, rolcreatedb, rolbypassrls
		FROM pg_roles WHERE rolname = current_user`).Scan(&role, &superuser, &createRole, &createDB,
		&bypassRLS); err != nil {
		return err
	}
	if role != "meta_admin_service" {
		return fmt.Errorf("admin database role %q is not meta_admin_service", role)
	}
	var directPlayers, changeAudit, changeCredentials bool
	if err := pool.QueryRow(ctx, `SELECT
		has_table_privilege(current_user, 'public.players', 'SELECT'),
		has_table_privilege(current_user, 'ops.audit_actions', 'UPDATE, DELETE, TRUNCATE'),
		has_column_privilege(current_user, 'ops.admin_users', 'role', 'UPDATE') OR
		has_column_privilege(current_user, 'ops.admin_users', 'password_hash', 'UPDATE') OR
		has_column_privilege(current_user, 'ops.admin_users', 'disabled', 'UPDATE')`).Scan(
		&directPlayers, &changeAudit, &changeCredentials); err != nil {
		return err
	}
	if superuser || createRole || createDB || bypassRLS || directPlayers || changeAudit || changeCredentials {
		return errors.New("admin database role has excessive privileges")
	}
	dummy, err := hashPassword("unavailable account")
	if err != nil {
		return err
	}
	store := &adminStore{pool: pool, dummyHash: dummy}
	app := &adminServer{brand: brand, store: store, meta: metaClient{baseURL: cfg.metaURL, token: cfg.metaToken},
		origin: cfg.origin}
	server := &http.Server{Addr: cfg.addr, Handler: app.handler(), ReadHeaderTimeout: 5 * time.Second,
		ReadTimeout: 15 * time.Second, WriteTimeout: 15 * time.Second, IdleTimeout: time.Minute,
		MaxHeaderBytes: 16 << 10}
	go func() {
		<-ctx.Done()
		shutdownCtx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		server.Shutdown(shutdownCtx)
	}()
	err = server.ListenAndServe()
	if errors.Is(err, http.ErrServerClosed) {
		return nil
	}
	return err
}

// Main runs the operator console as the process: no argument serves,
// `bootstrap-owner` and `recover-owner` are the owner's terminal ceremonies.
func Main(brand Brand) {
	var err error
	switch {
	case len(os.Args) == 1:
		err = runAdmin(brand)
	case len(os.Args) == 2 && (os.Args[1] == "bootstrap-owner" || os.Args[1] == "recover-owner"):
		err = runOwnerCeremony(os.Args[1], brand)
	default:
		err = errors.New("usage: admin [bootstrap-owner|recover-owner]")
	}
	if err != nil {
		fmt.Fprintln(os.Stderr, "admin:", err)
		os.Exit(1)
	}
}
