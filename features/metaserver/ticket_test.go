package metaserver

import (
	"os"
	"path/filepath"
	"testing"
)

func TestTicketKeyFileIsHex(t *testing.T) {
	path := filepath.Join(t.TempDir(), "ticket.key")
	os.WriteFile(path, []byte("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f\r\n"), 0o600)
	key, err := loadTicketKey(path)
	if err != nil || key != testKey {
		t.Fatalf("key %x %v", key, err)
	}
}

func TestTicketKeyRejectsBadFiles(t *testing.T) {
	for _, text := range []string{"", "1:000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
		"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e", "zz"} {
		path := filepath.Join(t.TempDir(), "ticket.key")
		os.WriteFile(path, []byte(text), 0o600)
		if _, err := loadTicketKey(path); err == nil {
			t.Errorf("%q parsed", text)
		}
	}
	if _, err := loadTicketKey(filepath.Join(t.TempDir(), "missing")); err == nil {
		t.Error("missing file parsed")
	}
}
