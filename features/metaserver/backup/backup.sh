#!/bin/sh
# A game's metaserver database backup (features/metaserver/README.md, "Backup").
#   loop        every hour a dump; the UTC day's first one is restore-checked,
#               then copied to daily/ (the default command)
#   once        a dump and its restore check; stdout: "<schema version> <file>"
#   check FILE  restores FILE into a throwaway server; stdout as once
# Everything but that one stdout line goes to stderr. The bucket's lifecycle
# and lock rules expire the copies, so this credential never deletes one.
set -u
# A dump holds password and session hashes: readable by its owner only.
umask 077
trap 'exit 143' TERM INT

dir=/backups
name=${BACKUP_NAME:-}
case "$name" in '' | *[!a-z0-9-]*) echo "backup: BACKUP_NAME must be a lowercase game name" >&2; exit 2 ;; esac

load_password() {
    [ -n "${PGPASSWORD_FILE:-}" ] || { echo "backup: PGPASSWORD_FILE is required" >&2; exit 1; }
    PGPASSWORD=$(tr -d '\r\n' < "$PGPASSWORD_FILE") || exit 1
    [ ${#PGPASSWORD} -ge 32 ] && [ ${#PGPASSWORD} -le 256 ] || {
        echo "backup: password file must contain 32..256 characters" >&2
        exit 1
    }
    export PGPASSWORD
}

# One dump or check at a time per directory, across containers.
# `command` keeps a failed redirection from exiting the shell.
lock() { command exec 9> "$dir/.lock" && flock 9 || { echo "backup: no lock on $dir" >&2; return 1; }; }
unlock() { exec 9>&-; }

# Sets $file. A file with the final name is always whole; a .part left
# while the lock is held is from a killed run. Keeps 25 hours of dumps.
dump() {
    rm -f "$dir/$name"-*.part
    file="$dir/$name-$(date -u +%Y%m%d-%H%M%S)-$(od -An -N3 -tx1 /dev/urandom | tr -d ' \n').dump"
    if ! pg_dump --format=custom --file "$file.part"; then
        rm -f "$file.part"
        echo "backup: dump failed" >&2
        return 1
    fi
    mv "$file.part" "$file" || { rm -f "$file.part"; echo "backup: dump failed" >&2; return 1; }
    find "$dir" -maxdepth 1 -name "$name-*.dump" -mmin +1500 -delete
    echo "backup: ${file##*/}" >&2
}

# Restores into a server of its own under /tmp: unix socket only, trust
# auth, gone on exit. pg_restore's own messages can quote row data, so
# only its exit code is reported.
check() (
    [ -f "$1" ] || { echo "backup: no dump $1" >&2; exit 1; }
    unset PGHOST PGHOSTADDR PGPORT PGUSER PGDATABASE PGPASSWORD PGPASSWORD_FILE PGSERVICE PGSERVICEFILE PGPASSFILE PGOPTIONS
    tmp=/tmp/backup-check
    stop() {
        [ -f "$tmp/data/postmaster.pid" ] && gosu postgres pg_ctl -D "$tmp/data" -m immediate -s stop >/dev/null 2>&1
        rm -rf "$tmp"
    }
    stop
    trap stop EXIT
    mkdir -p "$tmp/run" && chown -R postgres "$tmp" || exit 1
    gosu postgres initdb -D "$tmp/data" -U postgres --auth=trust --no-sync >/dev/null 2>&1 ||
        { echo "backup: check server did not initialise" >&2; exit 1; }
    gosu postgres pg_ctl -D "$tmp/data" -l "$tmp/log" -w -s \
        -o "-c listen_addresses='' -c unix_socket_directories=$tmp/run -c fsync=off" start >/dev/null 2>&1 ||
        { echo "backup: check server did not start" >&2; exit 1; }
    sql() { psql -X -h "$tmp/run" -U postgres -d check -v ON_ERROR_STOP=1 -Atq "$@"; }
    createdb -h "$tmp/run" -U postgres check || exit 1
    pg_restore -h "$tmp/run" -U postgres -d check --exit-on-error --single-transaction --no-owner --no-acl "$1" >/dev/null 2>&1
    code=$?
    [ "$code" -eq 0 ] || { echo "backup: ${1##*/} did not restore (pg_restore exit $code)" >&2; exit 1; }
    schema=$(sql -c "SELECT max(version) FROM schema_migrations") || exit 1
    case "$schema" in '' | *[!0-9]*) echo "backup: ${1##*/} restored without a schema version" >&2; exit 1 ;; esac
    sql -F ' ' >&2 <<'SQL' || exit 1
SELECT format('SELECT %L, count(*) FROM %I.%I', n.nspname || '.' || c.relname, n.nspname, c.relname)
FROM pg_class c JOIN pg_namespace n ON n.oid = c.relnamespace
WHERE c.relkind IN ('r', 'p') AND NOT c.relispartition
  AND n.nspname NOT IN ('pg_catalog', 'information_schema') AND n.nspname NOT LIKE 'pg\_toast%'
ORDER BY 1 \gexec
SQL
    echo "backup: ${1##*/} restored, schema $schema" >&2
    echo "$schema ${1##*/}"
)

ping() { # $1 = "" or "/fail", $2 = the line the check keeps
    [ -n "${BACKUP_HEARTBEAT_URL:-}" ] || return 0
    curl -fsS -m 10 -o /dev/null --data "$2" "${BACKUP_HEARTBEAT_URL}$1" || echo "backup: ping not delivered" >&2
}

copy() { # $1 = the dump, $2 = the prefix in the bucket
    for attempt in 1 2 3; do
        rclone --config /run/secrets/rclone.conf copy "$1" "$BACKUP_REMOTE/$2/" && return 0
        echo "backup: copy to $BACKUP_REMOTE/$2 failed, attempt $attempt" >&2
        sleep 60
    done
    return 1
}

# A day is marked only once its dump restored and every copy landed; a
# restart checks and copies the day again, which costs one more daily/ file.
# hourly/ gets every dump, so a broken check still leaves offsite copies;
# only a restored dump becomes the day's daily/ copy.
hour() {
    lock || { ping /fail "no lock"; return; }
    if ! dump; then unlock; ping /fail "dump failed"; return; fi
    base=${file##*/}
    day=$(date -u +%Y%m%d)
    daily=""
    restored=1
    if [ "$day" != "$done_day" ]; then
        if check "$file" >/dev/null; then daily=1; else restored=""; fi
    fi
    unlock
    if [ -n "${BACKUP_REMOTE:-}" ] && ! copy "$file" hourly; then
        ping /fail "copy of $base to $BACKUP_REMOTE failed"
        return
    fi
    [ -n "$restored" ] || { ping /fail "$base did not restore"; return; }
    if [ -n "$daily" ] && [ -n "${BACKUP_REMOTE:-}" ] && ! copy "$file" daily; then
        ping /fail "copy of $base to $BACKUP_REMOTE failed"
        return
    fi
    [ -z "$daily" ] || done_day=$day
    ping "" "$base${daily:+ restore-checked}${BACKUP_REMOTE:+ copied to $BACKUP_REMOTE}"
}

case "${1:-loop}" in
loop)
    load_password
    done_day=""
    while :; do
        hour
        sleep 3600 & wait $!
    done
    ;;
once)
    load_password
    lock || exit 1
    dump || exit 1
    check "$file"
    ;;
check)
    [ $# -eq 2 ] || { echo "usage: backup.sh check FILE" >&2; exit 2; }
    lock || exit 1
    check "$2"
    ;;
*)
    echo "usage: backup.sh [loop|once|check FILE]" >&2
    exit 2
    ;;
esac
