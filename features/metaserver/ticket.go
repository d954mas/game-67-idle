package metaserver

import (
	"crypto/hmac"
	"crypto/sha256"
	"crypto/subtle"
	"encoding/binary"
	"encoding/hex"
	"errors"
	"os"
	"strings"
)

// The entry ticket a room verifies with the key it shares with this
// service; ticket_test.go pins the same bytes the room's C test pins.
//
// version u8 (2) | kind u8 | subject u64 | room_id u32 | expires_at u32 | mac[16]
//
// little-endian, mac = first 16 bytes of HMAC-SHA256(key, the 18 bytes).
// The version byte stays 2 so the wire matches rooms built before the
// layout became the only one. The subject is the account of a player
// ticket and the incarnation of a room ticket, which, with the room's own
// id, is how a room proves itself on every call it makes here.
const (
	ticketVersion       = 2
	ticketKindPlayer    = 0x54
	ticketKindPageEntry = 0x50
	ticketKindRoom      = 0x52
	ticketKeySize       = 32
	ticketMacSize       = 16
	ticketSignedSize    = 18
	ticketSize          = ticketSignedSize + ticketMacSize
	// Clock disagreement tolerated with a room, as the room tolerates ours.
	ticketSkewSeconds = 5
)

type ticketKey [ticketKeySize]byte

// loadTicketKey reads the key file: 64 hex digits, whitespace ignored.
func loadTicketKey(path string) (ticketKey, error) {
	var key ticketKey
	text, err := os.ReadFile(path)
	if err != nil {
		return key, err
	}
	raw, err := hex.DecodeString(strings.TrimSpace(string(text)))
	if err != nil || len(raw) != ticketKeySize {
		return key, errors.New("ticket key: expected 64 hex digits")
	}
	copy(key[:], raw)
	return key, nil
}

type ticket struct {
	kind      byte
	subject   uint64
	roomID    uint32
	expiresAt uint32
}

func encodeTicket(key ticketKey, kind byte, subject uint64, roomID uint32, expiresAt uint32) [ticketSize]byte {
	var out [ticketSize]byte
	out[0] = ticketVersion
	out[1] = kind
	binary.LittleEndian.PutUint64(out[2:], subject)
	binary.LittleEndian.PutUint32(out[10:], roomID)
	binary.LittleEndian.PutUint32(out[14:], expiresAt)
	mac := hmac.New(sha256.New, key[:])
	mac.Write(out[:ticketSignedSize])
	copy(out[ticketSignedSize:], mac.Sum(nil)[:ticketMacSize])
	return out
}

// decodeTicket reads the fields and verifies the mac under `key` in
// constant time; false for another length, another version or a forged
// mac. Expiry is the caller's judgement, since the tolerated skew is theirs.
func decodeTicket(key ticketKey, raw []byte) (ticket, bool) {
	if len(raw) != ticketSize || raw[0] != ticketVersion {
		return ticket{}, false
	}
	t := ticket{kind: raw[1], subject: binary.LittleEndian.Uint64(raw[2:]),
		roomID: binary.LittleEndian.Uint32(raw[10:]), expiresAt: binary.LittleEndian.Uint32(raw[14:])}
	want := encodeTicket(key, t.kind, t.subject, t.roomID, t.expiresAt)
	return t, subtle.ConstantTimeCompare(raw, want[:]) == 1
}
