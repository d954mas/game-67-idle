package metaserver

import (
	"bytes"
	"context"
	"encoding/binary"
	"encoding/hex"
	"net/http"
	"net/http/httptest"
	"strconv"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/jackc/pgx/v5"
)

func roomBearerAt(key ticketKey, roomID uint32, incarnation uint64, expiresAt time.Time) string {
	ticket := encodeTicket(key, ticketKindRoom, incarnation, roomID, uint32(expiresAt.Unix()))
	return hex.EncodeToString(ticket[:])
}

// A room as the C host speaks it: a fresh ticket in every call.
func roomBearer(key ticketKey, roomID uint32, incarnation uint64) string {
	return roomBearerAt(key, roomID, incarnation, time.Now().Add(30*time.Second))
}

// loaded is a load answer: rev, then the document.
func loaded(rev uint64, doc string) []byte {
	return append(binary.LittleEndian.AppendUint64(nil, rev), doc...)
}

func saveBody(ifRev, saveID uint64, doc string) []byte {
	out := binary.LittleEndian.AppendUint64(nil, ifRev)
	out = binary.LittleEndian.AppendUint64(out, saveID)
	return append(out, doc...)
}

// conflict is a 412 answer: the rev stored now and the save that wrote it.
type conflict struct {
	rev         uint64
	room        uint32
	incarnation uint64
	saveID      uint64
}

func conflictOf(t *testing.T, rec *httptest.ResponseRecorder) conflict {
	t.Helper()
	b := rec.Body.Bytes()
	if len(b) != 28 {
		t.Fatalf("status %d, body %x is not a conflict", rec.Code, b)
	}
	return conflict{rev: binary.LittleEndian.Uint64(b), room: binary.LittleEndian.Uint32(b[8:]),
		incarnation: binary.LittleEndian.Uint64(b[12:]), saveID: binary.LittleEndian.Uint64(b[20:])}
}

func revOf(t *testing.T, rec *httptest.ResponseRecorder) uint64 {
	t.Helper()
	if rec.Body.Len() != 8 {
		t.Fatalf("status %d, body %q is not a rev", rec.Code, rec.Body.Bytes())
	}
	return binary.LittleEndian.Uint64(rec.Body.Bytes())
}

// The bytes the room's own test pins, so both sides prove they speak one
// layout.
func TestTicketMatchesTheRoomVector(t *testing.T) {
	got := encodeTicket(testKey, ticketKindPlayer, 0x0102030405060708, 7, 1700000030)
	want := "02540807060504030201070000001ef1536528d7b8b8d3fa973c90b19ce275f8c85c"
	if hex.EncodeToString(got[:]) != want {
		t.Fatalf("ticket bytes\n got %x\nwant %s", got, want)
	}
	back, genuine := decodeTicket(testKey, got[:])
	if !genuine || back != (ticket{kind: ticketKindPlayer, subject: 0x0102030405060708, roomID: 7, expiresAt: 1700000030}) {
		t.Fatalf("decoded %+v genuine=%v", back, genuine)
	}
	var otherKey ticketKey
	copy(otherKey[:], testKey[:])
	otherKey[0] ^= 1
	if _, genuine := decodeTicket(otherKey, got[:]); genuine {
		t.Fatal("a ticket verified under another key")
	}
	for i := range got {
		flipped := got
		flipped[i] ^= 1
		if _, genuine := decodeTicket(testKey, flipped[:]); genuine {
			t.Fatalf("byte %d flipped and the ticket still verified", i)
		}
	}
	if _, genuine := decodeTicket(testKey, got[1:]); genuine {
		t.Fatal("a ticket without its version byte verified")
	}
}

// Every room call is refused with 401 unless it carries a genuine,
// unexpired room ticket.
func TestRoomTicketsGuardTheirRoutes(t *testing.T) {
	st, _ := freshStore(t)
	a := newTestAPI(t, st)
	h := a.handler(testRoutes)
	id, err := st.createGuest(context.Background(), []byte("hash-of-a-secret-of-32-bytes...."))
	if err != nil {
		t.Fatal(err)
	}
	load := "/rooms/profiles/" + strconv.FormatInt(id, 10) + "/load"
	var otherKey ticketKey
	otherKey[0] = 9
	player := encodeTicket(testKey, ticketKindPlayer, 5, 12, uint32(time.Now().Add(time.Minute).Unix()))
	now := time.Now()
	for name, bearer := range map[string]string{
		"none":         "",
		"garbage":      strings.Repeat("zz", ticketSize),
		"wrong key":    roomBearer(otherKey, 12, 5),
		"expired":      roomBearerAt(testKey, 12, 5, now.Add(-(ticketSkewSeconds+5)*time.Second)),
		"player kind":  hex.EncodeToString(player[:]),
		"no room":      roomBearer(testKey, 0, 5),
		"no life":      roomBearer(testKey, 12, 0),
		"one byte cut": roomBearer(testKey, 12, 5)[:2*(ticketSize-1)],
	} {
		if rec := roomCall(t, h, load, bearer, nil); rec.Code != http.StatusUnauthorized {
			t.Errorf("load with %s: %d", name, rec.Code)
		}
		if rec := roomCall(t, h, "/rooms/profiles/"+strconv.FormatInt(id, 10)+"/save", bearer, saveBody(0, 1, "doc")); rec.Code != http.StatusUnauthorized {
			t.Errorf("save with %s: %d", name, rec.Code)
		}
	}
	// A clock a little behind ours is tolerated.
	if rec := roomCall(t, h, load, roomBearerAt(testKey, 12, 5, now.Add(-2*time.Second)), nil); rec.Code != http.StatusOK {
		t.Fatalf("within the skew: %d", rec.Code)
	}
	if got := a.rooms.ticketsRefused.Load(); got != 16 {
		t.Fatalf("tickets_refused = %d", got)
	}
}

// Entry issues a ticket naming the account for the room it picked; a page
// reload asks for a page-entry ticket. A second login is sent to the room
// already holding the account, full or not, where the newer socket
// supersedes the seat or resumes it.
func TestEntryIssuesTheTicket(t *testing.T) {
	st, _ := freshStore(t)
	h := newTestAPI(t, st).handler(testRoutes)
	playerID, other := guest(t, h)
	// Only room 20 has a seat: the fixture's room 9 is full.
	roomCall(t, h, "/rooms/status", roomBearer(testKey, 9, 1), status(7001, 0))
	if rec := roomCall(t, h, "/rooms/status", roomBearer(testKey, 20, 5), status(7020, 3)); rec.Code != http.StatusNoContent {
		t.Fatalf("status: %d %s", rec.Code, rec.Body)
	}
	before := time.Now().Unix()
	code, body := call(t, h, "POST", "/api/enter", other)
	if code != http.StatusOK || body["room_id"].(float64) != 20 || body["url"] != "ws://127.0.0.1:7020/" {
		t.Fatalf("enter: %d %v", code, body)
	}
	raw := ticketBytes(t, body)
	got, genuine := decodeTicket(testKey, raw)
	if !genuine || got.kind != ticketKindPlayer || got.subject != uint64(playerID) || got.roomID != 20 ||
		int64(got.expiresAt) < before+30 || int64(got.expiresAt) > time.Now().Unix()+30 ||
		int64(got.expiresAt) != int64(body["expires_at"].(float64)) {
		t.Fatalf("ticket %+v genuine=%v", got, genuine)
	}
	code, body, _ = post(t, h, "/api/enter", other, map[string]any{"page_entry": true})
	if code != http.StatusOK {
		t.Fatalf("page entry: %d %v", code, body)
	}
	pageTicket, pageGenuine := decodeTicket(testKey, ticketBytes(t, body))
	if !pageGenuine || pageTicket.kind != ticketKindPageEntry || pageTicket.subject != uint64(playerID) || pageTicket.roomID != 20 {
		t.Fatalf("page ticket %+v genuine=%v", pageTicket, pageGenuine)
	}
	// The account sits in room 20, which is now full; another room has
	// seats. A second login still goes to room 20.
	roomCall(t, h, "/rooms/status", roomBearer(testKey, 21, 5), status(7021, 16))
	if rec := roomCall(t, h, "/rooms/status", roomBearer(testKey, 20, 5), status(7020, 0, playerID)); rec.Code != http.StatusNoContent {
		t.Fatalf("status: %d", rec.Code)
	}
	second := loginElsewhere(t, h, other)
	code, body = call(t, h, "POST", "/api/enter", second)
	if code != http.StatusOK || body["room_id"].(float64) != 20 {
		t.Fatalf("second login: %d %v", code, body)
	}
	if got, _ := decodeTicket(testKey, ticketBytes(t, body)); got.subject != uint64(playerID) {
		t.Fatalf("second login's ticket names %d", got.subject)
	}
}

// loginElsewhere registers the guest behind `secret` and logs it in again,
// which is a second device with a session of its own.
func loginElsewhere(t *testing.T, h http.Handler, secret string) string {
	t.Helper()
	login := "fighter" + strconv.FormatInt(time.Now().UnixNano()%1_000_000, 10)
	if code, body, _ := post(t, h, "/api/register", secret, map[string]string{"login": login, "password": "a long enough password"}); code != http.StatusOK {
		t.Fatalf("register: %d %v", code, body)
	}
	code, body, _ := post(t, h, "/api/login", "", map[string]string{"login": login, "password": "a long enough password"})
	if code != http.StatusOK {
		t.Fatalf("login: %d %v", code, body)
	}
	return body["secret"].(string)
}

// The profile store: a first load is rev 0 and no
// document; a save lands on the rev it read and increments it; a save on a
// stale rev is refused with the rev stored now and the save that wrote it;
// a room that loads the profile takes it over, and the room it left can no
// longer save (the seat was superseded); the same life loading again
// resumes with rev and ownership as they were; an older life cannot take
// it back.
// A room lists its seats and every account whose profile it still holds
// unsaved, up to three times its capacity.
func TestARoomListsItsUnsavedAccountsBeyondItsSeats(t *testing.T) {
	st, _ := freshStore(t)
	h := newTestAPI(t, st).handler(testRoutes)
	accounts := func(count int, from int64) []int64 {
		out := make([]int64, count)
		for i := range out {
			out[i] = from + int64(i)
		}
		return out
	}
	// status() declares capacity 16 and seated = len(accounts); the seats stay within it.
	listing := func(port uint16, list []int64) []byte {
		body := string(status(port, 0, list...))
		return []byte(strings.Replace(body, `"seated":`+strconv.Itoa(len(list)), `"seated":16`, 1))
	}
	if rec := roomCall(t, h, "/rooms/status", roomBearer(testKey, 40, 5), listing(7040, accounts(48, 1000))); rec.Code != http.StatusNoContent {
		t.Fatalf("3 x capacity accounts: %d %s", rec.Code, rec.Body)
	}
	if rec := roomCall(t, h, "/rooms/status", roomBearer(testKey, 40, 5), listing(7040, accounts(49, 1000))); rec.Code != http.StatusBadRequest {
		t.Fatalf("past 3 x capacity: %d", rec.Code)
	}
}

func TestProfileDocsCompareAndSwap(t *testing.T) {
	st, _ := freshStore(t)
	a := newTestAPI(t, st)
	h := a.handler(testRoutes)
	ctx := context.Background()
	id, err := st.createGuest(ctx, []byte("hash-of-a-secret-of-32-bytes...."))
	if err != nil {
		t.Fatal(err)
	}
	path := "/rooms/profiles/" + strconv.FormatInt(id, 10)
	roomA := roomBearer(testKey, 20, 5)
	var saveID uint64
	save := func(bearer string, ifRev uint64, doc string) *httptest.ResponseRecorder {
		saveID++
		return roomCall(t, h, path+"/save", bearer, saveBody(ifRev, saveID, doc))
	}

	if rec := roomCall(t, h, path+"/load", roomA, nil); rec.Code != http.StatusOK || !bytes.Equal(rec.Body.Bytes(), loaded(0, "")) {
		t.Fatalf("first load: %d %x", rec.Code, rec.Body.Bytes())
	}
	if rec := save(roomA, 0, "doc-1"); rec.Code != http.StatusOK || revOf(t, rec) != 1 {
		t.Fatalf("save on rev 0: %d %x", rec.Code, rec.Body.Bytes())
	}
	if rec := save(roomA, 0, "doc-stale"); rec.Code != http.StatusPreconditionFailed ||
		conflictOf(t, rec) != (conflict{rev: 1, room: 20, incarnation: 5, saveID: 1}) {
		t.Fatalf("save on a stale rev: %d %x", rec.Code, rec.Body.Bytes())
	}
	if rec := save(roomA, 7, "doc-ahead"); rec.Code != http.StatusPreconditionFailed || conflictOf(t, rec).rev != 1 {
		t.Fatalf("save on a rev never stored: %d %x", rec.Code, rec.Body.Bytes())
	}
	if rec := save(roomA, 1, "doc-2"); rec.Code != http.StatusOK || revOf(t, rec) != 2 {
		t.Fatalf("save on rev 1: %d %x", rec.Code, rec.Body.Bytes())
	}
	// The same life loading again (a resume) changes nothing.
	if rec := roomCall(t, h, path+"/load", roomA, nil); !bytes.Equal(rec.Body.Bytes(), loaded(2, "doc-2")) {
		t.Fatalf("reload: %x", rec.Body.Bytes())
	}
	if rec := save(roomA, 2, "doc-3"); rec.Code != http.StatusOK || revOf(t, rec) != 3 {
		t.Fatalf("save after the reload: %d", rec.Code)
	}

	// A second login seated in room B: B loads and owns it, A is refused
	// whatever rev it names.
	roomB := roomBearer(testKey, 21, 5)
	if rec := roomCall(t, h, path+"/load", roomB, nil); !bytes.Equal(rec.Body.Bytes(), loaded(3, "doc-3")) {
		t.Fatalf("load in room B: %x", rec.Body.Bytes())
	}
	if rec := save(roomA, 3, "doc-from-a"); rec.Code != http.StatusConflict {
		t.Fatalf("superseded room's save: %d", rec.Code)
	}
	if rec := save(roomB, 3, "doc-4"); rec.Code != http.StatusOK || revOf(t, rec) != 4 {
		t.Fatalf("owner's save: %d", rec.Code)
	}
	// A newer life of room B takes it; the older life can neither save nor
	// load it back.
	roomB2 := roomBearer(testKey, 21, 6)
	if rec := roomCall(t, h, path+"/load", roomB2, nil); rec.Code != http.StatusOK {
		t.Fatalf("newer life load: %d", rec.Code)
	}
	if rec := save(roomB, 4, "zombie"); rec.Code != http.StatusConflict {
		t.Fatalf("older life save: %d", rec.Code)
	}
	if rec := roomCall(t, h, path+"/load", roomB, nil); rec.Code != http.StatusConflict {
		t.Fatalf("older life load: %d", rec.Code)
	}
	if rec := roomCall(t, h, path+"/load", roomB2, nil); !bytes.Equal(rec.Body.Bytes(), loaded(4, "doc-4")) {
		t.Fatalf("nothing refused landed: %x", rec.Body.Bytes())
	}

	// A profile nobody loaded cannot be saved; an account nobody has
	// cannot be loaded; the path must name an account.
	other, err := st.createGuest(ctx, []byte("hash-of-another-secret-32-bytes."))
	if err != nil {
		t.Fatal(err)
	}
	if rec := roomCall(t, h, "/rooms/profiles/"+strconv.FormatInt(other, 10)+"/save", roomB2, saveBody(0, 1, "x")); rec.Code != http.StatusConflict {
		t.Fatalf("save before a load: %d", rec.Code)
	}
	if rec := roomCall(t, h, "/rooms/profiles/999999/load", roomB2, nil); rec.Code != http.StatusNotFound {
		t.Fatalf("unknown account: %d", rec.Code)
	}
	for _, bad := range []string{"0", "-1", "x"} {
		if rec := roomCall(t, h, "/rooms/profiles/"+bad+"/load", roomB2, nil); rec.Code != http.StatusBadRequest {
			t.Fatalf("account %q: %d", bad, rec.Code)
		}
	}

	// The size limit is the configured one, the document alone counted,
	// on the way in and on the way out.
	a.rooms.profileMax = 1024
	if rec := save(roomB2, 4, strings.Repeat("d", 1024)); rec.Code != http.StatusOK || revOf(t, rec) != 5 {
		t.Fatalf("document at the limit: %d", rec.Code)
	}
	if rec := save(roomB2, 5, strings.Repeat("d", 1025)); rec.Code != http.StatusRequestEntityTooLarge {
		t.Fatalf("document over the limit: %d", rec.Code)
	}
	for name, body := range map[string][]byte{
		"no document": saveBody(5, 99, ""),
		"short head":  saveBody(5, 99, "")[:15],
		"nothing":     nil,
		"negative":    saveBody(1<<63, 99, "x"),
		"save id 0":   saveBody(5, 0, "x"),
	} {
		if rec := roomCall(t, h, path+"/save", roomB2, body); rec.Code != http.StatusBadRequest {
			t.Fatalf("%s: %d", name, rec.Code)
		}
	}
	a.rooms.profileMax = 512
	if rec := roomCall(t, h, path+"/load", roomB2, nil); rec.Code != http.StatusRequestEntityTooLarge {
		t.Fatalf("stored document over a lowered limit: %d", rec.Code)
	}
	var rev int64
	var doc []byte
	if err := st.pool.QueryRow(ctx, `SELECT rev, doc FROM profile_docs WHERE account = $1`, id).Scan(&rev, &doc); err != nil ||
		rev != 5 || len(doc) != 1024 {
		t.Fatalf("stored: rev %d, %d bytes, %v", rev, len(doc), err)
	}
	// A stored rev without a document, or a document at rev 0, cannot exist.
	if _, err := st.pool.Exec(ctx, `UPDATE profile_docs SET doc = ''::bytea WHERE account = $1`, id); err == nil {
		t.Fatal("a rev without a document was stored")
	}
	if _, err := st.pool.Exec(ctx, `UPDATE profile_docs SET rev = 0 WHERE account = $1`, id); err == nil {
		t.Fatal("a document at rev 0 was stored")
	}
}

// A save whose answer was lost is sent again with the same save id: it is
// told the rev it produced, not a conflict, even after the profile moved
// to another room; a save that did not land and whose rev moved on gets
// the conflict naming the writer, which the room can tell from its own.
func TestProfileDocSavesAreIdempotent(t *testing.T) {
	st, _ := freshStore(t)
	h := newTestAPI(t, st).handler(testRoutes)
	id, err := st.createGuest(context.Background(), []byte("hash-of-a-secret-of-32-bytes...."))
	if err != nil {
		t.Fatal(err)
	}
	path := "/rooms/profiles/" + strconv.FormatInt(id, 10)
	roomA := roomBearer(testKey, 20, 5)
	const first = 0x0001_0000_0001 // seat generation 1, save 1
	roomCall(t, h, path+"/load", roomA, nil)
	if rec := roomCall(t, h, path+"/save", roomA, saveBody(0, first, "doc-1")); rec.Code != http.StatusOK || revOf(t, rec) != 1 {
		t.Fatalf("save: %d", rec.Code)
	}
	for i := 0; i < 2; i++ {
		if rec := roomCall(t, h, path+"/save", roomA, saveBody(0, first, "doc-1")); rec.Code != http.StatusOK || revOf(t, rec) != 1 {
			t.Fatalf("repeat %d: %d %x", i, rec.Code, rec.Body.Bytes())
		}
	}
	if rec := roomCall(t, h, path+"/load", roomA, nil); !bytes.Equal(rec.Body.Bytes(), loaded(1, "doc-1")) {
		t.Fatalf("a repeat wrote: %x", rec.Body.Bytes())
	}
	// The same save id from another life of the room is another save.
	if rec := roomCall(t, h, path+"/save", roomBearer(testKey, 20, 4), saveBody(0, first, "doc-1")); rec.Code != http.StatusConflict {
		t.Fatalf("same id, older life: %d", rec.Code)
	}

	// Room B takes the profile over; A's repeat of its landed save still
	// reads as landed, and B's own save moves the rev on.
	roomB := roomBearer(testKey, 21, 5)
	roomCall(t, h, path+"/load", roomB, nil)
	if rec := roomCall(t, h, path+"/save", roomA, saveBody(0, first, "doc-1")); rec.Code != http.StatusOK || revOf(t, rec) != 1 {
		t.Fatalf("repeat after the takeover: %d", rec.Code)
	}
	if rec := roomCall(t, h, path+"/save", roomB, saveBody(1, 7, "doc-b")); rec.Code != http.StatusOK || revOf(t, rec) != 2 {
		t.Fatalf("B's save: %d", rec.Code)
	}
	if rec := roomCall(t, h, path+"/save", roomA, saveBody(0, first, "doc-1")); rec.Code != http.StatusConflict {
		t.Fatalf("A's repeat once B wrote: %d", rec.Code)
	}
	// B's second save, sent on a stale rev: the conflict names B's first.
	if rec := roomCall(t, h, path+"/save", roomB, saveBody(1, 8, "doc-b2")); rec.Code != http.StatusPreconditionFailed ||
		conflictOf(t, rec) != (conflict{rev: 2, room: 21, incarnation: 5, saveID: 7}) {
		t.Fatalf("stale in B: %d %x", rec.Code, rec.Body.Bytes())
	}
}

func TestProfileDocSavesUseOneDatabaseCommandOnSuccess(t *testing.T) {
	st, _ := freshStore(t)
	ctx := context.Background()
	var entries []docSaveEntry
	for i := 0; i < 3; i++ {
		id, err := st.createGuest(ctx, []byte("hash-of-a-secret-of-32-bytes..."+strconv.Itoa(i)))
		if err != nil {
			t.Fatal(err)
		}
		if _, _, err := st.loadDoc(ctx, id, 20, 5, 1024); err != nil {
			t.Fatal(err)
		}
		entries = append(entries, docSaveEntry{account: id, saveID: int64(i + 1), doc: []byte("doc")})
	}
	before := st.health.summary(time.Now()).Queries
	results, err := st.saveDocs(ctx, 20, 5, entries)
	if err != nil {
		t.Fatal(err)
	}
	for i, r := range results {
		if r.err != nil || r.rev != 1 || r.stored != (docWriter{room: 20, incarnation: 5, saveID: int64(i + 1)}) {
			t.Fatalf("entry %d: %+v", i, r)
		}
	}
	if commands := st.health.summary(time.Now()).Queries - before; commands != 1 {
		t.Fatalf("a batch that all landed used %d database commands, want 1", commands)
	}
}

// Saves racing on one rev: exactly one lands, every other is told the rev
// it made and its writer; nothing is lost silently. A retry racing its own
// original gets the original's rev, never a conflict. The row is held by
// another transaction until every racer is queued on it, so they all
// contend at once instead of arriving one by one.
func TestProfileDocSavesRaceOnOneRev(t *testing.T) {
	st, databaseURL := freshStore(t)
	h := newTestAPI(t, st).handler(testRoutes)
	id, err := st.createGuest(context.Background(), []byte("hash-of-a-secret-of-32-bytes...."))
	if err != nil {
		t.Fatal(err)
	}
	path := "/rooms/profiles/" + strconv.FormatInt(id, 10)
	room := roomBearer(testKey, 20, 5)
	if rec := roomCall(t, h, path+"/load", room, nil); rec.Code != http.StatusOK {
		t.Fatalf("load: %d", rec.Code)
	}
	// Racers 0-7 are distinct saves 1-8; racers 8-15 repeat save 1.
	const racers = 16
	idOf := func(i int) uint64 {
		if i >= 8 {
			return 1
		}
		return uint64(i + 1)
	}
	codes := make([]int, racers)
	bodies := make([][]byte, racers)
	ctx := context.Background()
	holder, err := pgx.Connect(ctx, databaseURL)
	if err != nil {
		t.Fatal(err)
	}
	defer holder.Close(ctx)
	hold, err := holder.Begin(ctx)
	if err != nil {
		t.Fatal(err)
	}
	if _, err := hold.Exec(ctx, `SELECT 1 FROM profile_docs WHERE account = $1 FOR UPDATE`, id); err != nil {
		t.Fatal(err)
	}
	var wg sync.WaitGroup
	for i := 0; i < racers; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			req := httptest.NewRequest("POST", path+"/save", bytes.NewReader(saveBody(0, idOf(i), "racer-"+strconv.FormatUint(idOf(i), 10))))
			req.Header.Set("Authorization", "Bearer "+room)
			rec := httptest.NewRecorder()
			h.ServeHTTP(rec, req)
			codes[i], bodies[i] = rec.Code, rec.Body.Bytes()
		}()
	}
	for waiting, deadline := 0, time.Now().Add(10*time.Second); waiting < racers; {
		if time.Now().After(deadline) {
			t.Fatalf("%d of %d racers queued on the row", waiting, racers)
		}
		// Asked on another connection: inside the holder's transaction the
		// activity view is a snapshot taken once.
		if err := st.pool.QueryRow(ctx, `SELECT count(*) FROM pg_stat_activity
			WHERE datname = current_database() AND wait_event_type = 'Lock'`).Scan(&waiting); err != nil {
			t.Fatal(err)
		}
	}
	if err := hold.Commit(ctx); err != nil {
		t.Fatal(err)
	}
	wg.Wait()
	winners := map[uint64]bool{}
	for i := range codes {
		if codes[i] == http.StatusOK && len(bodies[i]) == 8 && binary.LittleEndian.Uint64(bodies[i]) == 1 {
			winners[idOf(i)] = true
		}
	}
	if len(winners) != 1 {
		t.Fatalf("saves that landed on rev 1: %v (codes %v)", winners, codes)
	}
	var winner uint64
	for id := range winners {
		winner = id
	}
	for i := range codes {
		if idOf(i) == winner {
			continue
		}
		if codes[i] != http.StatusPreconditionFailed || len(bodies[i]) != 28 ||
			binary.LittleEndian.Uint64(bodies[i]) != 1 || binary.LittleEndian.Uint64(bodies[i][20:]) != winner {
			t.Fatalf("racer %d: %d %x", i, codes[i], bodies[i])
		}
	}
	if rec := roomCall(t, h, path+"/load", room, nil); !bytes.Equal(rec.Body.Bytes(), loaded(1, "racer-"+strconv.FormatUint(winner, 10))) {
		t.Fatalf("stored %x, the winner was %d", rec.Body.Bytes(), winner)
	}
}

// A retry must be byte-identical to be the same save: a repeated save id
// carrying another document is not answered as landed, it is refused and
// names the writer, and nothing is written.
func TestARepeatedSaveIDWithAnotherDocumentIsRefused(t *testing.T) {
	st, _ := freshStore(t)
	h := newTestAPI(t, st).handler(testRoutes)
	id, err := st.createGuest(context.Background(), []byte("hash-of-a-secret-of-32-bytes...."))
	if err != nil {
		t.Fatal(err)
	}
	path := "/rooms/profiles/" + strconv.FormatInt(id, 10)
	room := roomBearer(testKey, 20, 5)
	roomCall(t, h, path+"/load", room, nil)
	if rec := roomCall(t, h, path+"/save", room, saveBody(0, 3, "doc-a")); rec.Code != http.StatusOK || revOf(t, rec) != 1 {
		t.Fatalf("save: %d", rec.Code)
	}
	if rec := roomCall(t, h, path+"/save", room, saveBody(0, 3, "doc-b")); rec.Code != http.StatusPreconditionFailed ||
		conflictOf(t, rec) != (conflict{rev: 1, room: 20, incarnation: 5, saveID: 3}) {
		t.Fatalf("same id, other document: %d %x", rec.Code, rec.Body.Bytes())
	}
	if rec := roomCall(t, h, path+"/load", room, nil); !bytes.Equal(rec.Body.Bytes(), loaded(1, "doc-a")) {
		t.Fatalf("stored %x", rec.Body.Bytes())
	}
}

// An older life of a room cannot take a profile back by way of another
// room: once the registry knows a newer life of its id, its loads are
// refused.
func TestAnOlderLifeCannotReclaimThroughAnotherRoom(t *testing.T) {
	st, _ := freshStore(t)
	h := newTestAPI(t, st).handler(testRoutes)
	id, err := st.createGuest(context.Background(), []byte("hash-of-a-secret-of-32-bytes...."))
	if err != nil {
		t.Fatal(err)
	}
	if rec := roomCall(t, h, "/rooms/status", roomBearer(testKey, 20, 2), status(7020, 3)); rec.Code != http.StatusNoContent {
		t.Fatalf("status: %d", rec.Code)
	}
	path := "/rooms/profiles/" + strconv.FormatInt(id, 10) + "/load"
	for name, bearer := range map[string]string{"life 2": roomBearer(testKey, 20, 2), "room B": roomBearer(testKey, 21, 5)} {
		if rec := roomCall(t, h, path, bearer, nil); rec.Code != http.StatusOK {
			t.Fatalf("%s: %d", name, rec.Code)
		}
	}
	if rec := roomCall(t, h, path, roomBearer(testKey, 20, 1), nil); rec.Code != http.StatusConflict {
		t.Fatalf("zombie life 1 through room B: %d", rec.Code)
	}
}

// A room bearer is minted for a few minutes at most; one that claims to
// live longer is refused, on every route, and so is an incarnation past
// int64.
func TestRoomBearersHaveBoundedLives(t *testing.T) {
	st, _ := freshStore(t)
	h := newTestAPI(t, st).handler(testRoutes)
	id, err := st.createGuest(context.Background(), []byte("hash-of-a-secret-of-32-bytes...."))
	if err != nil {
		t.Fatal(err)
	}
	load := "/rooms/profiles/" + strconv.FormatInt(id, 10) + "/load"
	far := time.Now().Add(time.Hour)
	if rec := roomCall(t, h, load, roomBearerAt(testKey, 20, 5, far), nil); rec.Code != http.StatusUnauthorized {
		t.Fatalf("load bearer good for an hour: %d", rec.Code)
	}
	if rec := roomCall(t, h, "/rooms/status", roomBearerAt(testKey, 20, 5, far), status(7020, 3)); rec.Code != http.StatusUnauthorized {
		t.Fatalf("status bearer good for an hour: %d", rec.Code)
	}
	// What the room mints (now + 300 s) still passes.
	if rec := roomCall(t, h, load, roomBearerAt(testKey, 20, 5, time.Now().Add(roomBearerLife)), nil); rec.Code != http.StatusOK {
		t.Fatalf("a bearer at the room's own life: %d", rec.Code)
	}
	if rec := roomCall(t, h, load, roomBearer(testKey, 20, 1<<63), nil); rec.Code != http.StatusUnauthorized {
		t.Fatalf("incarnation past int64: %d", rec.Code)
	}
}

// A load refused for size leaves the owner where it was.
func TestAnOversizedLoadClaimsNothing(t *testing.T) {
	st, _ := freshStore(t)
	a := newTestAPI(t, st)
	h := a.handler(testRoutes)
	id, err := st.createGuest(context.Background(), []byte("hash-of-a-secret-of-32-bytes...."))
	if err != nil {
		t.Fatal(err)
	}
	path := "/rooms/profiles/" + strconv.FormatInt(id, 10)
	roomB := roomBearer(testKey, 21, 5)
	roomCall(t, h, path+"/load", roomB, nil)
	if rec := roomCall(t, h, path+"/save", roomB, saveBody(0, 1, strings.Repeat("d", 2048))); rec.Code != http.StatusOK {
		t.Fatalf("save: %d", rec.Code)
	}
	a.rooms.profileMax = 1024
	if rec := roomCall(t, h, path+"/load", roomBearer(testKey, 20, 5), nil); rec.Code != http.StatusRequestEntityTooLarge {
		t.Fatalf("oversized load: %d", rec.Code)
	}
	if rec := roomCall(t, h, path+"/save", roomB, saveBody(1, 2, "small")); rec.Code != http.StatusOK || revOf(t, rec) != 2 {
		t.Fatalf("the owner lost the profile to a refused load: %d", rec.Code)
	}
}

// batchEntry is one entry of a batch save, as the room writes it.
func batchEntry(account int64, ifRev, saveID uint64, doc string) []byte {
	out := binary.LittleEndian.AppendUint64(nil, uint64(account))
	out = binary.LittleEndian.AppendUint64(out, ifRev)
	out = binary.LittleEndian.AppendUint64(out, saveID)
	out = binary.LittleEndian.AppendUint32(out, uint32(len(doc)))
	return append(out, doc...)
}

// batchAnswer is one entry of a batch's answer.
type batchAnswer struct {
	status int
	body   []byte
}

func batchAnswers(t *testing.T, rec *httptest.ResponseRecorder) []batchAnswer {
	t.Helper()
	if rec.Code != http.StatusOK {
		t.Fatalf("batch: %d %q", rec.Code, rec.Body.Bytes())
	}
	var out []batchAnswer
	for b := rec.Body.Bytes(); len(b) > 0; {
		if len(b) < 6 {
			t.Fatalf("a cut answer: %x", b)
		}
		size := int(binary.LittleEndian.Uint32(b[2:]))
		if len(b) < 6+size {
			t.Fatalf("a cut answer: %x", b)
		}
		out = append(out, batchAnswer{int(binary.LittleEndian.Uint16(b)), b[6 : 6+size]})
		b = b[6+size:]
	}
	return out
}

// One call carries every entry and answers each as a save of its own:
// landed, stale (naming the writer), another room's, too large, malformed.
// A batch sent again as it was (its answer lost) lands nothing twice, and
// two entries on one account are judged one after the other.
func TestProfileDocSavesInOneBatch(t *testing.T) {
	st, _ := freshStore(t)
	a := newTestAPI(t, st)
	a.rooms.profileMax = 1024
	h := a.handler(testRoutes)
	var ids []int64
	for i := 0; i < 4; i++ {
		id, err := st.createGuest(context.Background(), []byte("hash-of-a-secret-of-32-bytes..."+strconv.Itoa(i)))
		if err != nil {
			t.Fatal(err)
		}
		ids = append(ids, id)
	}
	roomA, roomB := roomBearer(testKey, 20, 5), roomBearer(testKey, 21, 5)
	load := func(bearer string, id int64) {
		roomCall(t, h, "/rooms/profiles/"+strconv.FormatInt(id, 10)+"/load", bearer, nil)
	}
	load(roomA, ids[0])
	load(roomA, ids[1])
	load(roomB, ids[2]) // another room's
	load(roomA, ids[3])
	if rec := roomCall(t, h, "/rooms/profiles/"+strconv.FormatInt(ids[1], 10)+"/save", roomA, saveBody(0, 9, "earlier")); rec.Code != http.StatusOK {
		t.Fatalf("earlier save: %d", rec.Code)
	}
	batch := append(batchEntry(ids[0], 0, 1, "a"), batchEntry(ids[1], 0, 2, "b")...)
	batch = append(batch, batchEntry(ids[2], 0, 3, "c")...)
	batch = append(batch, batchEntry(ids[3], 0, 4, strings.Repeat("x", 1025))...)
	batch = append(batch, batchEntry(ids[3], 0, 0, "d")...)
	want := func(got []batchAnswer) {
		t.Helper()
		if len(got) != 5 {
			t.Fatalf("%d answers for 5 entries", len(got))
		}
		if got[0].status != http.StatusOK || binary.LittleEndian.Uint64(got[0].body) != 1 {
			t.Fatalf("landed: %+v", got[0])
		}
		if got[1].status != http.StatusPreconditionFailed || len(got[1].body) != 28 || binary.LittleEndian.Uint64(got[1].body[20:]) != 9 {
			t.Fatalf("stale: %+v", got[1])
		}
		if got[2].status != http.StatusConflict || got[3].status != http.StatusRequestEntityTooLarge || got[4].status != http.StatusBadRequest {
			t.Fatalf("refused: %d %d %d", got[2].status, got[3].status, got[4].status)
		}
	}
	want(batchAnswers(t, roomCall(t, h, "/rooms/profiles/save", roomA, batch)))
	want(batchAnswers(t, roomCall(t, h, "/rooms/profiles/save", roomA, batch)))
	if rec := roomCall(t, h, "/rooms/profiles/"+strconv.FormatInt(ids[0], 10)+"/load", roomA, nil); !bytes.Equal(rec.Body.Bytes(), loaded(1, "a")) {
		t.Fatalf("the repeat wrote: %x", rec.Body.Bytes())
	}

	// Saves of one account in one call go as if sent one after the other:
	// the first lands on its rev, the second on the same rev is stale and
	// names the first, the third on the rev the first made lands on top.
	three := append(batchEntry(ids[0], 1, 5, "a2"), batchEntry(ids[0], 1, 6, "a3")...)
	three = append(three, batchEntry(ids[0], 2, 7, "a4")...)
	got := batchAnswers(t, roomCall(t, h, "/rooms/profiles/save", roomA, three))
	if len(got) != 3 || got[0].status != http.StatusOK || binary.LittleEndian.Uint64(got[0].body) != 2 ||
		got[1].status != http.StatusPreconditionFailed || binary.LittleEndian.Uint64(got[1].body[20:]) != 5 ||
		got[2].status != http.StatusOK || binary.LittleEndian.Uint64(got[2].body) != 3 {
		t.Fatalf("one account three times: %+v", got)
	}

	// A body that is not whole entries is refused whole, as is a batch past the cap.
	for name, body := range map[string][]byte{
		"empty": nil,
		"cut":   batchEntry(ids[0], 3, 8, "a5")[:29],
		"short": batchEntry(ids[0], 3, 8, "a5")[:20],
		"many":  bytes.Repeat(batchEntry(ids[0], 3, 8, "a5"), saveBatchMax+1),
	} {
		if rec := roomCall(t, h, "/rooms/profiles/save", roomA, body); rec.Code != http.StatusBadRequest {
			t.Fatalf("%s: %d", name, rec.Code)
		}
	}
}
