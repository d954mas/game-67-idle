-- Schema version 1. The migrate job records its checksum and applies it once;
-- a later change is a new numbered migration, never an edit of this file.
-- Roles are cluster-wide, so they are created only when missing.

DO $$ BEGIN
    CREATE ROLE meta_runtime NOLOGIN NOSUPERUSER NOCREATEDB NOCREATEROLE NOREPLICATION NOBYPASSRLS;
EXCEPTION WHEN duplicate_object THEN NULL;
END $$;
DO $$ BEGIN
    CREATE ROLE meta_backup NOLOGIN NOSUPERUSER NOCREATEDB NOCREATEROLE NOREPLICATION NOBYPASSRLS;
EXCEPTION WHEN duplicate_object THEN NULL;
END $$;
DO $$ BEGIN
    CREATE ROLE meta_admin_service NOLOGIN;
EXCEPTION WHEN duplicate_object THEN NULL;
END $$;
DO $$ BEGIN
    CREATE ROLE meta_admin_read NOLOGIN;
EXCEPTION WHEN duplicate_object THEN NULL;
END $$;
DO $$ BEGIN
    CREATE ROLE meta_admin_auth NOLOGIN;
EXCEPTION WHEN duplicate_object THEN NULL;
END $$;
DO $$ BEGIN
    CREATE ROLE meta_audit_writer NOLOGIN;
EXCEPTION WHEN duplicate_object THEN NULL;
END $$;

-- A player is an id; a login and a password hash attach to it once, and
-- the id in the ticket never changes. The secret that opens the player
-- lives in `sessions`, one row per device, hashed: a leaked table logs
-- nobody in. A player with a login is an account; a lost password with no
-- session left is a lost account, by decision.
CREATE TABLE players (
    id            bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    login         text UNIQUE,
    password_hash text,
    created_at    timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX players_login_folded_prefix ON players (lower(login) text_pattern_ops);

-- Sessions never expire; a password change deletes every session of the
-- player but the one that changed it, and a logout deletes its own.
CREATE TABLE sessions (
    secret_hash  bytea PRIMARY KEY,
    player_id    bigint NOT NULL REFERENCES players(id),
    created_at   timestamptz NOT NULL DEFAULT now(),
    last_used_at timestamptz
);

-- The registry: what each room last reported. A row outlives the room; it
-- is the fence that keeps an older life of the same id out, and a stale
-- `seen_at` keeps players from being sent to a room that stopped talking.
-- seated..round_trip_ms are how the room breathed in the second before
-- `seen_at`; period_ms is how often it reports; host is what the proxy dials.
CREATE TABLE rooms (
    id            bigint PRIMARY KEY,
    incarnation   bigint NOT NULL,
    port          int NOT NULL,
    free          int NOT NULL,
    accounts      bigint[] NOT NULL,
    seen_at       timestamptz NOT NULL,
    seated        int NOT NULL DEFAULT 0,
    away          int NOT NULL DEFAULT 0,
    passes        int NOT NULL DEFAULT 0,
    pass_avg_us   int NOT NULL DEFAULT 0,
    pass_max_us   int NOT NULL DEFAULT 0,
    round_trip_ms int NOT NULL DEFAULT 0,
    capacity      int NOT NULL,
    period_ms     int NOT NULL,
    host          text NOT NULL
);

-- The room each account last left, and when: a departure is an account
-- in a room's previous status and not in its new one. A PvP lock lives in
-- the room's memory only, so an account that just left is sent back to
-- the same room while a lock could still run (store.pick). Written at the
-- rate players leave, not at the status rate.
CREATE TABLE last_rooms (
    account bigint PRIMARY KEY,
    room    bigint NOT NULL,
    left_at timestamptz NOT NULL
);
CREATE INDEX last_rooms_left_at ON last_rooms (left_at);

-- Each room machine's plan, as its rooms report it (roomperf.go perfPlan): it
-- runs the rooms room_id_base + 1 .. + rooms. A registry row outside its
-- host's plan is a room switched off, not one that stopped answering.
CREATE TABLE room_machines (
    host         text PRIMARY KEY,
    rooms        integer NOT NULL CHECK (rooms BETWEEN 1 AND 4096),
    room_id_base bigint NOT NULL CHECK (room_id_base >= 0),
    reported_at  timestamptz NOT NULL
);

-- A profile: the document as the room sent it, opaque here (the room is
-- its authority). `rev` counts the stored writes; a save names the rev it
-- read and lands only on that one. The owner is the life of the room that
-- loaded it last: a second login in another room takes the profile over,
-- and the first room's saves are refused from then on. The writer is the
-- save that produced `rev`: the room, its life and the save id it chose,
-- so a save retried after a lost answer is recognised as landed instead of
-- reading as another writer; the SHA-256 of its document tells a true
-- retry from a reused save id. A stored rev always has a document, and
-- rev 0 never has one.
CREATE TABLE profile_docs (
    account            bigint PRIMARY KEY REFERENCES players(id),
    owner_room         bigint NOT NULL,
    owner_incarnation  bigint NOT NULL,
    rev                bigint NOT NULL DEFAULT 0,
    doc                bytea NOT NULL DEFAULT ''::bytea,
    writer_room        bigint NOT NULL DEFAULT 0,
    writer_incarnation bigint NOT NULL DEFAULT 0,
    writer_save_id     bigint NOT NULL DEFAULT 0,
    writer_doc_hash    bytea NOT NULL DEFAULT ''::bytea,
    updated_at         timestamptz NOT NULL DEFAULT now(),
    CHECK ((rev = 0) = (octet_length(doc) = 0))
);

-- The operator console's own data, reached only through the ops views and
-- the column grants below.
CREATE SCHEMA ops;
REVOKE ALL ON SCHEMA ops FROM PUBLIC;

CREATE TABLE ops.admin_users (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    username text NOT NULL UNIQUE,
    password_hash text NOT NULL,
    totp_secret bytea NOT NULL,
    last_totp_step bigint NOT NULL DEFAULT -1,
    role text NOT NULL CHECK (role IN ('owner', 'operator', 'support', 'viewer')),
    disabled boolean NOT NULL DEFAULT false,
    created_at timestamptz NOT NULL DEFAULT now(),
    updated_at timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE ops.admin_sessions (
    secret_hash bytea PRIMARY KEY,
    user_id bigint NOT NULL REFERENCES ops.admin_users(id),
    created_at timestamptz NOT NULL DEFAULT now(),
    last_seen_at timestamptz NOT NULL DEFAULT now(),
    idle_expires_at timestamptz NOT NULL,
    absolute_expires_at timestamptz NOT NULL,
    revoked_at timestamptz
);
CREATE INDEX admin_sessions_user ON ops.admin_sessions (user_id);
CREATE INDEX admin_sessions_expiry ON ops.admin_sessions (absolute_expires_at);

CREATE TABLE ops.audit_actions (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    happened_at timestamptz NOT NULL DEFAULT now(),
    user_id bigint REFERENCES ops.admin_users(id),
    action text NOT NULL,
    target text NOT NULL DEFAULT '',
    result text NOT NULL,
    reason_digest bytea,
    correlation_id text NOT NULL DEFAULT ''
);

CREATE FUNCTION ops.reject_audit_change() RETURNS trigger
LANGUAGE plpgsql AS $$ BEGIN
    RAISE EXCEPTION 'audit actions are append-only';
END $$;
CREATE TRIGGER audit_no_update BEFORE UPDATE OR DELETE ON ops.audit_actions
FOR EACH ROW EXECUTE FUNCTION ops.reject_audit_change();
CREATE TRIGGER audit_no_truncate BEFORE TRUNCATE ON ops.audit_actions
FOR EACH STATEMENT EXECUTE FUNCTION ops.reject_audit_change();

CREATE VIEW ops.v_players WITH (security_barrier=true) AS
SELECT p.id, p.login, p.created_at, d.rev AS profile_rev, d.updated_at AS profile_saved_at
FROM public.players AS p
LEFT JOIN public.profile_docs AS d ON d.account = p.id;

CREATE VIEW ops.v_rooms WITH (security_barrier=true) AS
SELECT id, incarnation, host, port, free, seated, away, capacity, period_ms, seen_at, accounts
FROM public.rooms;

CREATE VIEW ops.v_room_machines WITH (security_barrier=true) AS
SELECT host, rooms, room_id_base, reported_at FROM public.room_machines;

-- The console reads a player's saved profile as text for the player card.
CREATE VIEW ops.v_player_profiles WITH (security_barrier=true) AS
SELECT account, rev, updated_at, doc FROM public.profile_docs;

-- Minute history: occupancy from the registry, capacity from the rooms'
-- performance windows, the API's own counters. A minute without a run is
-- missing, not zero; minute_utc is the start of the minute.
CREATE SCHEMA activity;
REVOKE ALL ON SCHEMA activity FROM PUBLIC;

CREATE TABLE activity.occupancy_runs (
    minute_utc timestamptz PRIMARY KEY,
    sampled_at timestamptz NOT NULL,
    rooms_seen integer NOT NULL DEFAULT 0 CHECK (rooms_seen >= 0),
    live_rooms integer NOT NULL DEFAULT 0 CHECK (live_rooms >= 0)
);

CREATE TABLE activity.occupancy_minutes (
    minute_utc timestamptz NOT NULL REFERENCES activity.occupancy_runs(minute_utc) ON DELETE CASCADE,
    room_id bigint NOT NULL,
    incarnation bigint NOT NULL,
    seated integer NOT NULL,
    away integer NOT NULL,
    capacity integer NOT NULL,
    alive boolean NOT NULL,
    room_seen_at timestamptz NOT NULL,
    PRIMARY KEY (minute_utc, room_id)
);

CREATE TABLE activity.capacity_runs (
    minute_utc timestamptz PRIMARY KEY,
    sampled_at timestamptz NOT NULL
);

-- Each machine's load averaged over the minute (NULL: no window from it)
-- and its rooms' seats as the minute ended.
CREATE TABLE activity.machine_minutes (
    minute_utc  timestamptz NOT NULL REFERENCES activity.capacity_runs(minute_utc) ON DELETE CASCADE,
    host        text NOT NULL,
    rooms       integer NOT NULL CHECK (rooms >= 0),
    live_rooms  integer NOT NULL CHECK (live_rooms >= 0),
    seated      integer NOT NULL CHECK (seated >= 0),
    away        integer NOT NULL CHECK (away >= 0),
    capacity    integer NOT NULL CHECK (capacity >= 0),
    cpu_pct     double precision,
    user_pct    double precision,
    system_pct  double precision,
    softirq_pct double precision,
    steal_pct   double precision,
    iowait_pct  double precision,
    rx_mbit_s   double precision,
    tx_mbit_s   double precision,
    rx_pps      double precision,
    tx_pps      double precision,
    PRIMARY KEY (minute_utc, host)
);

CREATE TABLE activity.fleet_minutes (
    minute_utc          timestamptz PRIMARY KEY REFERENCES activity.capacity_runs(minute_utc) ON DELETE CASCADE,
    windows             integer NOT NULL,
    avg_seated          double precision NOT NULL,
    cpu_ms_per_player_s double precision,
    step_p99_us         bigint NOT NULL,
    replicate_p99_us    bigint NOT NULL,
    late_p99_us         bigint NOT NULL,
    late_ticks          bigint NOT NULL,
    dropped_ticks       bigint NOT NULL,
    rx_msg_per_s        double precision NOT NULL,
    tx_writes_per_s     double precision NOT NULL,
    tx_mbit_s           double precision NOT NULL
);

-- A run without rows for a route served no requests on it that minute.
CREATE TABLE activity.api_minutes (
    minute_utc    timestamptz NOT NULL REFERENCES activity.capacity_runs(minute_utc) ON DELETE CASCADE,
    endpoint      text NOT NULL,
    requests      integer NOT NULL,
    client_errors integer NOT NULL,
    server_errors integer NOT NULL,
    rate_limited  integer NOT NULL,
    avg_ms        double precision NOT NULL,
    max_ms        double precision NOT NULL,
    PRIMARY KEY (minute_utc, endpoint)
);

CREATE VIEW activity.v_occupancy_runs WITH (security_barrier=true) AS
SELECT minute_utc, sampled_at, rooms_seen, live_rooms FROM activity.occupancy_runs;

CREATE VIEW activity.v_occupancy_minutes WITH (security_barrier=true) AS
SELECT minute_utc, room_id, incarnation, seated, away, capacity, alive, room_seen_at
FROM activity.occupancy_minutes;

CREATE VIEW activity.v_capacity_runs WITH (security_barrier=true) AS
SELECT minute_utc, sampled_at FROM activity.capacity_runs;

CREATE VIEW activity.v_machine_minutes WITH (security_barrier=true) AS
SELECT minute_utc, host, rooms, live_rooms, seated, away, capacity, cpu_pct, user_pct, system_pct,
    softirq_pct, steal_pct, iowait_pct, rx_mbit_s, tx_mbit_s, rx_pps, tx_pps
FROM activity.machine_minutes;

CREATE VIEW activity.v_fleet_minutes WITH (security_barrier=true) AS
SELECT minute_utc, windows, avg_seated, cpu_ms_per_player_s, step_p99_us, replicate_p99_us, late_p99_us,
    late_ticks, dropped_ticks, rx_msg_per_s, tx_writes_per_s, tx_mbit_s
FROM activity.fleet_minutes;

CREATE VIEW activity.v_api_minutes WITH (security_barrier=true) AS
SELECT minute_utc, endpoint, requests, client_errors, server_errors, rate_limited, avg_ms, max_ms
FROM activity.api_minutes;

-- The metaserver's own login: what the player and room calls touch, nothing
-- of the operator console's.
GRANT USAGE ON SCHEMA public, activity TO meta_runtime;
GRANT SELECT ON public.schema_migrations, public.players, public.sessions,
    public.rooms, public.last_rooms, public.profile_docs TO meta_runtime;
GRANT INSERT, UPDATE ON public.players, public.rooms, public.profile_docs TO meta_runtime;
GRANT DELETE ON public.rooms TO meta_runtime;
GRANT INSERT, UPDATE, DELETE ON public.sessions, public.last_rooms TO meta_runtime;
GRANT SELECT, INSERT, UPDATE, DELETE ON public.room_machines TO meta_runtime;
GRANT USAGE ON SEQUENCE public.players_id_seq TO meta_runtime;
GRANT SELECT, INSERT, UPDATE, DELETE ON activity.occupancy_runs TO meta_runtime;
GRANT SELECT, INSERT ON activity.occupancy_minutes TO meta_runtime;
GRANT SELECT, INSERT, DELETE ON activity.capacity_runs TO meta_runtime;
GRANT INSERT ON activity.machine_minutes, activity.fleet_minutes, activity.api_minutes TO meta_runtime;

GRANT pg_read_all_data TO meta_backup;

-- The console reads views only; it may record a TOTP step and its own
-- sessions, never change a user's credentials or the audit trail.
GRANT USAGE ON SCHEMA ops TO meta_admin_read, meta_admin_auth, meta_audit_writer;
GRANT USAGE ON SCHEMA activity TO meta_admin_read;
GRANT SELECT ON ops.v_players, ops.v_rooms, ops.v_room_machines, ops.v_player_profiles TO meta_admin_read;
GRANT SELECT ON activity.v_occupancy_runs, activity.v_occupancy_minutes, activity.v_capacity_runs,
    activity.v_machine_minutes, activity.v_fleet_minutes, activity.v_api_minutes TO meta_admin_read;
GRANT SELECT ON ops.admin_users, ops.admin_sessions TO meta_admin_auth;
GRANT UPDATE (last_totp_step) ON ops.admin_users TO meta_admin_auth;
GRANT INSERT ON ops.admin_sessions TO meta_admin_auth;
GRANT UPDATE (last_seen_at, idle_expires_at, revoked_at) ON ops.admin_sessions TO meta_admin_auth;
GRANT INSERT ON ops.audit_actions TO meta_audit_writer;
GRANT meta_admin_read, meta_admin_auth, meta_audit_writer TO meta_admin_service;
