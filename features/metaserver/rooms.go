package metaserver

import (
	"context"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log/slog"
	"math"
	"net/http"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"time"
)

// The room side of the service, calls a room makes over plain HTTP with
// its own ticket as the bearer: a status every couple of seconds and when
// its seats change, a profile load when an account sits down, saves for
// what changed. The ticket
// vouches for the room id and the incarnation (the start time of this
// life of the room), so the URLs carry neither. Nothing here waits on the
// game's clock: the room gives every call its own timeout and asks again.
type rooms struct {
	key   ticketKey
	store *store
	log   *slog.Logger
	now   func() time.Time
	// What the rooms were told since start, for the admin console: a refusal
	// is a player sent back or a walk not saved, which no log line counts.
	ticketsRefused  atomic.Int64
	statusesRefused atomic.Int64
	loadsRefused    atomic.Int64
	loadsFailed     atomic.Int64
	savesRefused    atomic.Int64
	savesFailed     atomic.Int64
	statuses        atomic.Int64
	loads           atomic.Int64
	saves           atomic.Int64
	saveBatches     atomic.Int64
	// The routes are public: a flood of bad tickets is a line per period
	// with its count, never a line per call.
	refusals sampledLog
	restarts *restarts
	perf     *perfBoard
	traffic  *trafficLedger
	plans    planCache
	alerts   *alertTracker
	// The largest document a room may save (META_PROFILE_MAX_BYTES).
	profileMax int
}

// sampledLog writes the first of a run at once and then one line per
// refusalPeriod carrying how many the run has grown by.
type sampledLog struct {
	mu     sync.Mutex
	lastAt time.Time
	since  int64
}

const refusalPeriod = 10 * time.Second

func (s *sampledLog) note(now time.Time) (count int64, write bool) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.since++
	if now.Sub(s.lastAt) < refusalPeriod {
		return s.since, false
	}
	count, s.since, s.lastAt = s.since, 0, now
	return count, true
}

const (
	// The room's status buffer is proven against the same number.
	statusMaxBody = 4096
	// The status version. Any other version, and any field this
	// decoder does not know, is refused with 400, which fences the room: a
	// mixed build on a stand stops loudly instead of reporting numbers
	// nobody can read. testdata/room_status_v3.json is the shape both
	// sides pin.
	statusVersion = 3
	// A room may not claim more seats than this; the accounts list and the
	// body cap follow from it.
	maxCapacity = 128
	// A room also lists every account whose profile it still holds unsaved:
	// a player that left, whose last save is still out, is routed back to it
	// rather than to a room that would load the state from before. Its last
	// saves are bounded at twice its seats, so its list is bounded at three
	// times its capacity.
	accountsPerSeat = 3
	// The store gets less than the room waits for a load (3 s), so a claim
	// never lands after the room has already given the seat up.
	storeTimeout = 2 * time.Second
)

// roomStatus is what a room reports every `period_ms`: where it is, what
// it can hold, what it holds, and how it breathed since the previous
// status. The columns are int32: a number that does not fit is a decode
// error, never a wrapped sum.
type roomStatus struct {
	V           int32   `json:"v"`
	Port        uint16  `json:"port"`
	Free        int32   `json:"free"`
	Accounts    []int64 `json:"accounts"`
	Seated      int32   `json:"seated"`
	Away        int32   `json:"away"`
	Passes      int32   `json:"passes"` // passes that ticked in the window
	PassAvgUs   int32   `json:"pass_avg_us"`
	PassMaxUs   int32   `json:"pass_max_us"`
	RoundTripMs int32   `json:"round_trip_ms"` // of the previous status, as the room saw it
	Capacity    int32   `json:"capacity"`      // seats in all
	PeriodMs    int32   `json:"period_ms"`     // how often the room reports; staleness follows from it
	Host        string  `json:"host"`          // what the proxy dials for this room
	// The room's performance window (roomperf.go), read apart so a window
	// this server cannot read never refuses the status.
	Perf json.RawMessage `json:"perf,omitempty"`
}

// valid refuses what the router cannot use (the version, the port, the
// host, the seats and the accounts against the capacity). The health numbers are observations: a pass that
// took a minute (a paused container) is true and is clamped, not a reason
// to fence.
func (st *roomStatus) valid() bool {
	if st.V != statusVersion || st.Port == 0 || !hostNameOK(st.Host) {
		return false
	}
	if st.Capacity < 1 || st.Capacity > maxCapacity {
		return false
	}
	if len(st.Accounts) > accountsPerSeat*int(st.Capacity) {
		return false
	}
	if st.PeriodMs < 100 || st.PeriodMs > 60_000 {
		return false
	}
	for _, n := range []int32{st.Free, st.Seated, st.Away} {
		if n < 0 || n > st.Capacity {
			return false
		}
	}
	if st.Free+st.Seated+st.Away > st.Capacity {
		return false
	}
	st.Passes = clamp(st.Passes, 1_000_000)
	st.PassAvgUs = clamp(st.PassAvgUs, 2_000_000_000)
	st.PassMaxUs = clamp(st.PassMaxUs, 2_000_000_000)
	st.RoundTripMs = clamp(st.RoundTripMs, 3_600_000)
	return true
}

// hostNameOK admits what may stand in a URL's host: 1-63 of letters,
// digits, dots and dashes.
func hostNameOK(host string) bool {
	if len(host) == 0 || len(host) > 63 {
		return false
	}
	for _, c := range host {
		ok := (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-'
		if !ok {
			return false
		}
	}
	return true
}

func clamp(n, top int32) int32 {
	if n < 0 {
		return 0
	}
	if n > top {
		return top
	}
	return n
}

// A room mints its bearer for at most roomBearerLife from its own clock;
// one claiming a longer life is refused, so a leaked bearer is worth
// minutes at most. The allowance for a room clock ahead of
// ours is safe: a room further ahead already finds every player ticket
// expired (TTL 30 s + 5 s skew) and seats nobody.
const (
	roomBearerLife = 300 * time.Second
	roomClockAhead = 30 * time.Second
)

// authorize verifies the room ticket in the Authorization header: hex of a
// ticket signed with the shared key, kind ROOM, unexpired, with a room id
// and an incarnation. False has already answered 401.
func (h *rooms) authorize(w http.ResponseWriter, r *http.Request) (roomID uint32, incarnation uint64, ok bool) {
	text, found := strings.CutPrefix(r.Header.Get("Authorization"), "Bearer ")
	text = strings.TrimSpace(text)
	reason := "room ticket required"
	var t ticket
	if found && len(text) == 2*ticketSize {
		raw, err := hex.DecodeString(text)
		genuine := false
		if err == nil {
			t, genuine = decodeTicket(h.key, raw)
		}
		switch {
		case err != nil:
		case t.kind != ticketKindRoom:
			// A player's ticket, or not a ticket at all.
		case !genuine:
			reason = "room ticket forged"
		case int64(t.expiresAt)+ticketSkewSeconds < h.now().Unix():
			reason = "room ticket expired"
		case int64(t.expiresAt) > h.now().Add(roomBearerLife+roomClockAhead).Unix():
			reason = "room ticket lives too long"
		case t.roomID == 0 || t.subject == 0 || t.subject > math.MaxInt64:
			reason = "room ticket without a room"
		default:
			return t.roomID, t.subject, true
		}
	}
	h.ticketsRefused.Add(1)
	if count, write := h.refusals.note(h.now()); write {
		h.log.Warn("room calls refused", "count", count, "last_reason", reason, "last_room_id", t.roomID,
			"last_remote", r.RemoteAddr)
	}
	http.Error(w, reason, http.StatusUnauthorized)
	return 0, 0, false
}

// RoomStatus: a room's status. 204 recorded; 409 an older life of this
// room id, which must stop hosting; 401 as above (a wrong key or a clock
// far off, which recur on a restart just the same).
func (h *rooms) RoomStatus(w http.ResponseWriter, r *http.Request) {
	roomID, incarnation, ok := h.authorize(w, r)
	if !ok {
		return
	}
	h.statuses.Add(1)
	var st roomStatus
	dec := json.NewDecoder(http.MaxBytesReader(w, r.Body, statusMaxBody))
	dec.DisallowUnknownFields()
	if err := dec.Decode(&st); err != nil || !st.valid() {
		h.statusesRefused.Add(1)
		h.log.Warn("room status unreadable", "room_id", roomID, "v", st.V, "err", err)
		http.Error(w, fmt.Sprintf("status v%d required", statusVersion), http.StatusBadRequest)
		return
	}
	ctx, cancel := context.WithTimeout(r.Context(), storeTimeout)
	defer cancel()
	current, err := h.store.reportRoom(ctx, roomID, incarnation, &st)
	if err != nil {
		h.log.Error("room status", "room_id", roomID, "err", err)
		http.Error(w, "store", http.StatusInternalServerError)
		return
	}
	if !current {
		h.statusesRefused.Add(1)
		h.log.Warn("room fenced", "room_id", roomID, "incarnation", incarnation)
		http.Error(w, "stale incarnation", http.StatusConflict)
		return
	}
	h.restarts.note(roomID, incarnation, h.now())
	if len(st.Perf) > 0 && h.perf != nil {
		if p, ok := decodeRoomPerf(st.Perf); ok {
			h.perf.record(h.now(), roomID, incarnation, &st, p)
			if p.Machine != nil {
				h.traffic.note(h.now(), st.Host, p.Machine.RxTotal, p.Machine.TxTotal)
			}
			if plan, ok := planOf(p, roomID); ok && h.plans.due(st.Host, plan, h.now()) {
				if err := h.store.reportPlan(r.Context(), st.Host, plan); err != nil {
					h.plans.forget(st.Host)
					h.log.Warn("room machine plan", "host", st.Host, "err", err)
				}
			}
		} else {
			h.perf.refused.Add(1)
		}
	}
	w.WriteHeader(http.StatusNoContent)
}

// player reads the {player} path value; 0 has already answered 400.
func player(w http.ResponseWriter, r *http.Request) int64 {
	id, err := strconv.ParseInt(r.PathValue("player"), 10, 64)
	if err != nil || id <= 0 {
		http.Error(w, "bad player", http.StatusBadRequest)
		return 0
	}
	return id
}

// ProfileLoad: a room loads {player}'s profile. 200 with `rev u64 LE |
// document`: rev 0 and no document is a first visit, any other rev comes
// with its document (the table's CHECK), and this life of the room owns
// the profile from then on. 409 an older life of the owning room may not
// take it back; 404 no such account; 413 the stored document is over the
// configured size, which the room sizes its buffer by, and claims
// nothing. The store call follows the request, so a room that gave up
// waiting gives up the claim.
func (h *rooms) ProfileLoad(w http.ResponseWriter, r *http.Request) {
	roomID, incarnation, ok := h.authorize(w, r)
	if !ok {
		return
	}
	id := player(w, r)
	if id == 0 {
		return
	}
	h.loads.Add(1)
	ctx, cancel := context.WithTimeout(r.Context(), storeTimeout)
	defer cancel()
	rev, doc, err := h.store.loadDoc(ctx, id, int64(roomID), int64(incarnation), h.profileMax)
	switch {
	case errors.Is(err, errOlderLife):
		h.loadsRefused.Add(1)
		h.log.Info("load refused, a newer life owns the profile", "room_id", roomID, "account", id)
		http.Error(w, "older life", http.StatusConflict)
		return
	case errors.Is(err, errNoAccount):
		h.loadsRefused.Add(1)
		http.Error(w, "no such account", http.StatusNotFound)
		return
	case errors.Is(err, errTooLarge):
		h.loadsFailed.Add(1)
		h.log.Error("stored profile over the limit", "room_id", roomID, "account", id)
		http.Error(w, "stored profile too large", http.StatusRequestEntityTooLarge)
		return
	case err != nil:
		h.loadsFailed.Add(1)
		h.log.Error("load profile", "room_id", roomID, "account", id, "err", err)
		http.Error(w, "store", http.StatusInternalServerError)
		return
	}
	body := make([]byte, 8, 8+len(doc))
	binary.LittleEndian.PutUint64(body, uint64(rev))
	w.Header().Set("Content-Type", "application/octet-stream")
	w.Write(append(body, doc...))
}

// saveHead is what precedes the document of a save: if_rev, save id.
const saveHead = 16

// ProfileSave: a room saves {player}'s profile, `if_rev u64 LE |
// save_id u64 LE | document`, a compare-and-swap on the rev the room read.
// The save id is the room's, unique within one life of the room, never 0.
// 200 with `rev u64 LE`: the new rev (if_rev + 1), or, for a save id that
// already landed, the rev it produced. 412 with `rev u64 LE | room u32 LE
// | incarnation u64 LE | save_id u64 LE`: the rev stored now and the save
// that wrote it (zeros for rev 0); nothing written, the room reads again.
// 409 another room or a newer life loaded the profile since (the seat was
// superseded: the room lets it go). 413 a document over the configured
// size; 400 no document, save id 0, if_rev past int64. It is one entry of
// saveAll, as a batch of one.
func (h *rooms) ProfileSave(w http.ResponseWriter, r *http.Request) {
	roomID, incarnation, ok := h.authorize(w, r)
	if !ok {
		return
	}
	id := player(w, r)
	if id == 0 {
		return
	}
	body, err := io.ReadAll(http.MaxBytesReader(w, r.Body, int64(saveHead+h.profileMax)))
	var tooLarge *http.MaxBytesError
	switch {
	case errors.As(err, &tooLarge):
		http.Error(w, "profile too large", http.StatusRequestEntityTooLarge)
		return
	case err != nil, len(body) <= saveHead:
		http.Error(w, "bad profile", http.StatusBadRequest)
		return
	}
	entry := docSaveEntry{account: id, ifRev: int64(binary.LittleEndian.Uint64(body)),
		saveID: int64(binary.LittleEndian.Uint64(body[8:])), doc: body[saveHead:]}
	status, answer := h.saveAll(roomID, incarnation, []docSaveEntry{entry})[0].answer()
	if status != http.StatusOK && status != http.StatusPreconditionFailed {
		http.Error(w, string(answer), status)
		return
	}
	w.Header().Set("Content-Type", "application/octet-stream")
	w.WriteHeader(status)
	w.Write(answer)
}

// batchEntryHead is what precedes the document of a batch's entry: account,
// if_rev, save id, the document's size.
const batchEntryHead = 28

// saveBatchMax: the entries of one batch. A room has at most its seats and
// its last saves out at once (three times its seats) and sends more in
// another call.
const saveBatchMax = 96

// ProfileSaves: a room's saves in one call:
// 1..saveBatchMax entries of `account u64 LE | if_rev u64 LE | save_id u64
// LE | size u32 LE | document`. 200 with one `status u16 LE | size u32 LE |
// body` per entry, in order, each what ProfileSave answers that save alone (an
// account of 0 is a 400 of its own). Each entry is its own compare-and-swap:
// a batch is not all or nothing, and a 500 entry is sent again as it was.
// 400 for a body that does not split into entries or holds more than
// saveBatchMax; 413 past what saveBatchMax entries can weigh.
func (h *rooms) ProfileSaves(w http.ResponseWriter, r *http.Request) {
	roomID, incarnation, ok := h.authorize(w, r)
	if !ok {
		return
	}
	body, err := io.ReadAll(http.MaxBytesReader(w, r.Body, int64(saveBatchMax*(batchEntryHead+h.profileMax))))
	var tooLarge *http.MaxBytesError
	if errors.As(err, &tooLarge) {
		http.Error(w, "saves too large", http.StatusRequestEntityTooLarge)
		return
	}
	entries, ok := splitSaves(body)
	if err != nil || !ok {
		http.Error(w, "bad saves", http.StatusBadRequest)
		return
	}
	h.saveBatches.Add(1)
	var answer []byte
	for _, v := range h.saveAll(roomID, incarnation, entries) {
		status, body := v.answer()
		answer = binary.LittleEndian.AppendUint16(answer, uint16(status))
		answer = binary.LittleEndian.AppendUint32(answer, uint32(len(body)))
		answer = append(answer, body...)
	}
	w.Header().Set("Content-Type", "application/octet-stream")
	w.Write(answer)
}

// splitSaves reads a batch's entries; false unless the body is 1..saveBatchMax whole entries.
func splitSaves(body []byte) ([]docSaveEntry, bool) {
	var entries []docSaveEntry
	for len(body) > 0 {
		if len(body) < batchEntryHead || len(entries) == saveBatchMax {
			return nil, false
		}
		size := uint64(binary.LittleEndian.Uint32(body[24:]))
		if size > uint64(len(body)-batchEntryHead) {
			return nil, false
		}
		end := batchEntryHead + int(size)
		entries = append(entries, docSaveEntry{account: int64(binary.LittleEndian.Uint64(body)),
			ifRev: int64(binary.LittleEndian.Uint64(body[8:])), saveID: int64(binary.LittleEndian.Uint64(body[16:])),
			doc: body[batchEntryHead:end]})
		body = body[end:]
	}
	return entries, len(entries) > 0
}

// saveVerdict is an entry's HTTP status and what the store said of it.
type saveVerdict struct {
	status int
	result saveResult
}

// answer is the status and body the entry gets, as ProfileSave would write them.
func (v saveVerdict) answer() (int, []byte) {
	switch v.status {
	case http.StatusOK:
		return v.status, binary.LittleEndian.AppendUint64(nil, uint64(v.result.rev))
	case http.StatusPreconditionFailed:
		answer := binary.LittleEndian.AppendUint64(nil, uint64(v.result.rev))
		answer = binary.LittleEndian.AppendUint32(answer, uint32(v.result.stored.room))
		answer = binary.LittleEndian.AppendUint64(answer, uint64(v.result.stored.incarnation))
		return v.status, binary.LittleEndian.AppendUint64(answer, uint64(v.result.stored.saveID))
	case http.StatusConflict:
		return v.status, []byte("not the owner")
	case http.StatusRequestEntityTooLarge:
		return v.status, []byte("profile too large")
	case http.StatusBadRequest:
		return v.status, []byte("bad profile")
	}
	return v.status, []byte("store")
}

// saveAll judges a room's entries and stores the sound ones in one call. The
// store call is detached from the request: a save that reached the database
// must not be rolled back because the room hung up.
func (h *rooms) saveAll(roomID uint32, incarnation uint64, entries []docSaveEntry) []saveVerdict {
	verdicts := make([]saveVerdict, len(entries))
	var sound []docSaveEntry
	var at []int
	for i, e := range entries {
		switch {
		case len(e.doc) > h.profileMax:
			verdicts[i].status = http.StatusRequestEntityTooLarge
		case e.account <= 0 || len(e.doc) == 0 || e.ifRev < 0 || e.saveID <= 0:
			verdicts[i].status = http.StatusBadRequest
		default:
			sound, at = append(sound, e), append(at, i)
		}
	}
	if len(sound) == 0 {
		return verdicts
	}
	h.saves.Add(int64(len(sound)))
	ctx, cancel := context.WithTimeout(context.Background(), storeTimeout)
	defer cancel()
	results, err := h.store.saveDocs(ctx, int64(roomID), int64(incarnation), sound)
	for j, e := range sound {
		v := &verdicts[at[j]]
		if err != nil {
			v.result.err = err
		} else {
			v.result = results[j]
		}
		switch {
		case v.result.err == nil:
			v.status = http.StatusOK
		case errors.Is(v.result.err, errStaleRev):
			v.status = http.StatusPreconditionFailed
			h.savesRefused.Add(1)
			h.log.Info("save on a stale rev", "room_id", roomID, "account", e.account, "if_rev", e.ifRev, "rev", v.result.rev)
		case errors.Is(v.result.err, errNotOwner):
			v.status = http.StatusConflict
			h.savesRefused.Add(1)
			h.log.Info("save refused, the profile is another room's", "room_id", roomID, "account", e.account)
		default:
			v.status = http.StatusInternalServerError
			h.savesFailed.Add(1)
			h.log.Error("save profile", "room_id", roomID, "account", e.account, "err", v.result.err)
		}
	}
	return verdicts
}
